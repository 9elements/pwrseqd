
#include "Netlink.hpp"

#include "Logging.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

using namespace std;
using namespace std::chrono_literals;

#define MAX_PAYLOAD 1024
// Family name and multicast group name
#define FAMILY_NAME "reg_event"
#define MULTICAST_GROUP_NAME "reg_mc_group"

struct reg_genl_event {
    char reg_name[32];
    uint64_t event;
};

// Callback function to handle received messages
static int handle_msg(struct nl_msg *msg, void *arg) {
    struct nlmsghdr *nlh = nlmsg_hdr(msg);
    struct genlmsghdr *ghdr = (struct genlmsghdr *)nlmsg_data(nlh);

    struct nlattr *attr = nlmsg_attrdata(nlh, sizeof(struct genlmsghdr));
    int attrlen = nlmsg_attrlen(nlh, sizeof(struct genlmsghdr));

    if (attr) {
        struct reg_genl_event *edata = (struct reg_genl_event *)nla_data(attr);
        static_cast<NetlinkRegulatorEvents*>(arg)->SetEvent(edata);
    } else {
        log_err("No attribute data.");
    }

    return NL_OK;
}

static NetlinkRegulatorEvents* w;
NetlinkRegulatorEvents* GetNetlinkRegulatorEvents(boost::asio::io_context& io)
{
    if (w)
        return w;
    w = new NetlinkRegulatorEvents(io);
    return w;
}

void NetlinkRegulatorEvents::Register(std::string name,
    const std::function<void(std::string name, uint64_t events)> handler)
{
    auto it = callbacks.find(name);
    if (it != callbacks.end())
        throw std::runtime_error("Callback for regulator " + name + " already in use!");

    callbacks[name] = handler;
}

void NetlinkRegulatorEvents::Unregister(std::string name)
{
    auto it = callbacks.find(name);
    if (it != callbacks.end())
        callbacks.erase(it);
}

void NetlinkRegulatorEvents::SetEvent(struct reg_genl_event *edata)
{
    std::string name = string(edata->reg_name);
    uint64_t events = edata->event;

    auto it = callbacks.find(name);
    if (it != callbacks.end())
        it->second(name, events);
}

void NetlinkRegulatorEvents::WaitForEvent(void)
{
    this->streamDesc.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [this](const boost::system::error_code ec) {
            int ret;
            if (ec)
            {
                return;
            }
            do {
                ret = nl_recvmsgs(nlsock, cb);
            } while (ret >= 0);
            WaitForEvent();
        });
}

NetlinkRegulatorEvents::NetlinkRegulatorEvents(boost::asio::io_context& io) :
    io(&io), callbacks(), nlsock(nullptr), cb(nullptr), streamDesc(io)
{
    int family_id, group_id;
    log_debug("Starting Generic Netlink Multicast Receiver...");

    // Initialize Netlink library and socket
    log_debug("Allocating Netlink socket...");
    nlsock = nl_socket_alloc();
    if (!nlsock) {
        throw std::runtime_error("Error allocating Netlink socket.");
    }

    // Use genl_connect to connect to the family
    log_debug("Connecting to Generic Netlink family...");
    if (genl_connect(nlsock)) {
        nl_socket_free(nlsock);
        throw std::runtime_error("Error connecting to Generic Netlink family");
    }
    log_debug("Connected to Generic Netlink family");
    
    // Connect to the Generic Netlink family
    log_debug("Resolving family ID for " + string(FAMILY_NAME));
    family_id = genl_ctrl_resolve(nlsock, FAMILY_NAME);
    if (family_id < 0) {
        nl_socket_free(nlsock);
        throw std::runtime_error("Error resolving family ID for " + string(FAMILY_NAME));
    }
    log_debug("Resolved family ID: " + family_id);

    // Subscribe to the multicast group
    log_debug("Resolving group ID for " + string(MULTICAST_GROUP_NAME));
    group_id = genl_ctrl_resolve_grp(nlsock, FAMILY_NAME, MULTICAST_GROUP_NAME);
    if (group_id < 0) {
        nl_socket_free(nlsock);
        throw std::runtime_error("Error resolving group ID for " + string(MULTICAST_GROUP_NAME));
    }
    log_debug("Resolved group ID: " + to_string(group_id));

    log_debug("Adding membership to multicast group...");

    if (nl_socket_add_membership(nlsock, group_id) < 0) {
        nl_socket_free(nlsock);
        throw std::runtime_error("Error adding membership to multicast group");
    }
    nl_socket_disable_auto_ack(nlsock);

    // Set up callback function
    cb = nl_cb_alloc(NL_CB_DEFAULT);
    nl_cb_set(cb, NL_CB_MSG_IN, NL_CB_CUSTOM, handle_msg, (void *)this);

    nl_socket_set_nonblocking(nlsock);
    this->streamDesc.assign(nl_socket_get_fd(nlsock));

    WaitForEvent();
}

NetlinkRegulatorEvents::~NetlinkRegulatorEvents()
{
    // Cleanup
    nl_cb_put(cb);
    nl_socket_free(nlsock);
}