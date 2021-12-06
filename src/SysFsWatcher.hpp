#pragma once

#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/thread/mutex.hpp>

#include <filesystem>
#include <map>

using namespace std;
using namespace std::filesystem;

struct SysFsEvent
{
    filesystem::path path;
    std::function<void(filesystem::path, const char*)> handler;
    char data[1024];
    int fd;
};

class SysFsWatcher
{
  public:
    SysFsWatcher(boost::asio::io_context& io);
    ~SysFsWatcher();
    int Main(int ctrlFd, int statusFd);
    bool IsRunning();
    int Start();
    void Stop();
    void Register(
        filesystem::path,
        const std::function<void(filesystem::path, const char*)> handler);
    void Unregister(filesystem::path);

  private:
    int controlFd;
    std::thread* runner;
    boost::asio::io_context* io;
    map<filesystem::path, std::function<void(filesystem::path, const char*)>>
        callbacks;
    map<int, SysFsEvent> events;
};

SysFsWatcher* GetSysFsWatcher(boost::asio::io_context& io);
