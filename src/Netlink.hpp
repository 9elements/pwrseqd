#pragma once

#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>

#include <linux/netlink.h>
#include <linux/genetlink.h>

#include <map>

using namespace std;

class NetlinkRegulatorEvents
{
  public:
    NetlinkRegulatorEvents(boost::asio::io_context& io);
    ~NetlinkRegulatorEvents();
    void Register(std::string name,
        const std::function<void(std::string name, uint64_t events)> handler);
    void Unregister(std::string name);
    void SetEvent(struct reg_genl_event *edata);

  private:
    void WaitForEvent(void);

    NetlinkRegulatorEvents(const NetlinkRegulatorEvents&) = delete;
    NetlinkRegulatorEvents& operator=(NetlinkRegulatorEvents const&) = delete;
    boost::asio::io_context* io;
    map<std::string, std::function<void(std::string name, uint64_t events)>>
        callbacks;
    struct nl_sock *nlsock;
    struct nl_cb *cb;
    boost::asio::posix::stream_descriptor streamDesc;
};

NetlinkRegulatorEvents* GetNetlinkRegulatorEvents(boost::asio::io_context& io);
