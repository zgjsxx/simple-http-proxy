#ifndef SRS_APP_LISTENER_HPP
#define SRS_APP_LISTENER_HPP

#include <string>

#include <srs_kernel_error.hpp>
#include <srs_protocol_st.hpp>
#include <srs_app_st.hpp>

using std::string;

// The tcp connection handler.
class ISrsTcpHandler
{
public:
    ISrsTcpHandler();
    virtual ~ISrsTcpHandler();
public:
    // When got tcp client.
    virtual srs_error_t on_tcp_client(srs_netfd_t stfd) = 0;
};

class SrsTcpListener : public ISrsCoroutineHandler
{
private:
    srs_netfd_t lfd;
    SrsCoroutine* trd;
private:
    ISrsTcpHandler* handler;
    std::string ip;
    int port;
public:
    SrsTcpListener(ISrsTcpHandler* h, std::string i, int p);
    virtual ~SrsTcpListener();
public:
    virtual int fd();
public:
    virtual srs_error_t listen();
// Interface ISrsReusableThreadHandler.
public:
    virtual srs_error_t cycle();
};
#endif