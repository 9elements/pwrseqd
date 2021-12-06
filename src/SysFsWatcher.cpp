
#include "SysFsWatcher.hpp"

#include "Logging.hpp"

#include <boost/thread/lock_guard.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace std;
using namespace std::chrono_literals;

static SysFsWatcher* w;
SysFsWatcher* GetSysFsWatcher(boost::asio::io_context& io)
{
    if (w)
        return w;
    w = new SysFsWatcher(io);
    return w;
}

// FIXME: This should use BOOST asio, but it's missing proper documentation

void SysFsWatcher::Register(
    filesystem::path p,
    std::function<void(filesystem::path, const char*)> handler)
{
    int fd;
    SysFsEvent event;

    this->Stop();

    // Open a connection to the attribute file.
    if ((fd = open(p.c_str(), O_RDONLY)) < 0)
    {
        log_err("Failed to open " + p.string() + ", ret = " + to_string(fd));
        return;
    }

    // Add to internal event list
    event.path = p;
    event.handler = handler;
    event.fd = fd;

    this->events[fd] = event;

    this->Start();
}

void SysFsWatcher::Unregister(filesystem::path p)
{
    this->Stop();

    for (auto const& x : this->events)
    {
        if (x.second.path == p)
        {
            close(x.second.fd);
            this->events.erase(x.second.fd);
            break;
        }
    }

    this->Start();
}

void SysFsWatcher::Stop(void)
{
    char dummy = 0;

    if (this->controlFd > 0)
    {
        write(this->controlFd, &dummy, 1);
        this->runner->join();
        delete this->runner;
        this->runner = nullptr;
        close(this->controlFd);
        this->controlFd = -1;
    }
}

int SysFsWatcher::Start(void)
{
    int controlFd[2];
    int statusFd[2];

    if (pipe(controlFd))
    {
        return -1;
    }
    if (pipe(statusFd))
    {
        return -1;
    }
    this->controlFd = controlFd[1];

    this->runner =
        new thread(&SysFsWatcher::Main, this, controlFd[0], statusFd[1]);

    // Wait for thread to start
    {
        char dummy;
        read(statusFd[0], &dummy, 1);
    }
    close(statusFd[0]);

    return 0;
}

// FIXME: Keep fds open on remove/insertion
int SysFsWatcher::Main(int ctrlFd, int statusFd)
{
    const int n = this->events.size() + 1;
    struct pollfd* ufds = new struct pollfd[n];
    char dummyData[1024] = {};
    int rv, i;
    SysFsEvent event;
    char notify = 0;

    log_debug("Starting SysFS watcher thread");

    ufds[0].fd = ctrlFd;
    ufds[0].events = POLLIN;
    ufds[0].revents = 0;

    i = 1;
    for (auto const& x : this->events)
    {
        ufds[i].fd = x.first;
        ufds[i].events = POLLPRI | POLLERR;
        ufds[i].revents = 0;

        // Someone suggested dummy reads before the poll() call
        read(ufds[i].fd, dummyData, sizeof(dummyData));

        i++;
    }

    write(statusFd, &notify, 1);
    close(statusFd);

    while (1)
    {
        if ((rv = poll(ufds, n, -1)) <= 0)
            break;

        if (ufds[0].revents & POLLIN)
        {
            char dummy;
            ufds[0].revents = 0;
            if (read(ctrlFd, &dummy, 1) == 1 && dummy == 0)
                break;
            rv--;
        }

        for (i = 1; i < n && rv > 0; i++)
        {
            if ((ufds[i].revents & (POLLPRI | POLLERR)) == (POLLPRI | POLLERR))
            {
                event = this->events[ufds[i].fd];

                // lseek+read is required to clear the POLLPRI | POLLERR
                // condition
                if (lseek(ufds[i].fd, 0, SEEK_SET) < 0)
                    break;
                int n = read(ufds[i].fd, event.data, sizeof(event.data));
                if (n > 0)
                    this->io->post(
                        [event] { event.handler(event.path, event.data); });

                ufds[i].revents = 0;
                rv--;
            }
        }
    }

    delete ufds;
    close(ctrlFd);
    log_debug("Terminating SysFS watcher thread");

    return 0;
}

SysFsWatcher::SysFsWatcher(boost::asio::io_context& io) :
    io(&io), controlFd(-1), runner(nullptr)
{}

SysFsWatcher::~SysFsWatcher()
{
    this->Stop();
    for (auto const& x : this->events)
        close(x.first);
}