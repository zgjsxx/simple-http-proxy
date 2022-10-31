#include <sys/socket.h>
#include <netdb.h>
#include <srs_app_utility.hpp>

SrsLogLevel srs_get_log_level(string level)
{
    if ("verbose" == level) {
        return SrsLogLevelVerbose;
    } else if ("info" == level) {
        return SrsLogLevelInfo;
    } else if ("trace" == level) {
        return SrsLogLevelTrace;
    } else if ("warn" == level) {
        return SrsLogLevelWarn;
    } else if ("error" == level) {
        return SrsLogLevelError;
    } else {
        return SrsLogLevelDisabled;
    }
}

string srs_get_peer_ip(int fd)
{
    // discovery client information
    sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    if (getpeername(fd, (sockaddr*)&addr, &addrlen) == -1) {
        return "";
    }

    char saddr[64];
    char* h = (char*)saddr;
    socklen_t nbh = (socklen_t)sizeof(saddr);
    const int r0 = getnameinfo((const sockaddr*)&addr, addrlen, h, nbh, NULL, 0, NI_NUMERICHOST);
    if(r0) {
        return "";
    }

    return std::string(saddr);
}

int srs_get_peer_port(int fd)
{
    // discovery client information
    sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    if (getpeername(fd, (sockaddr*)&addr, &addrlen) == -1) {
        return 0;
    }

    int port = 0;
    switch(addr.ss_family) {
        case AF_INET:
            port = ntohs(((sockaddr_in*)&addr)->sin_port);
         break;
        case AF_INET6:
            port = ntohs(((sockaddr_in6*)&addr)->sin6_port);
         break;
    }

    return port;
}