#ifndef SRS_PROTOCOL_ASYNC_DNS_HPP    
#define SRS_PROTOCOL_ASYNC_DNS_HPP
#include <string>
#include <string.h>
#include <ares.h>
#include <poll.h>
#include <netdb.h>
#include <arpa/inet.h>
using namespace std;

void srs_sock_state_cb(void* data, int fd, int readable, int writable);

class SrsAsyncDns
{
public:
    SrsAsyncDns();
    ~SrsAsyncDns();
public:
    virtual void init();
    virtual void do_resolve(string hostname, int port, struct sockaddr_in* server_addr);
    virtual bool getLookupResult();
private:
    ares_channel channel;
    bool lookup_success;
    string dns_server_ip;
public:
    struct pollfd pds;
};


#endif
