#include <srs_app_listener.hpp>
#include <srs_kernel_error.hpp>
#include <srs_core_time.hpp>
#include <srs_kernel_log.hpp>

ISrsTcpHandler::ISrsTcpHandler()
{
}

ISrsTcpHandler::~ISrsTcpHandler()
{
}

SrsTcpListener::SrsTcpListener(ISrsTcpHandler* h, string i, int p)
{
    handler = h;
    ip = i;
    port = p;

    lfd = NULL;
    
    trd = new SrsDummyCoroutine();
}

SrsTcpListener::~SrsTcpListener()
{
    srs_freep(trd);
    srs_close_stfd(lfd);
}

int SrsTcpListener::fd()
{
    return srs_netfd_fileno(lfd);
}

srs_error_t SrsTcpListener::listen()
{
    srs_error_t err = srs_success;
    srs_trace("srs_tcp_listen");
    if ((err = srs_tcp_listen(ip, port, &lfd)) != srs_success) {
        return srs_error_wrap(err, "listen at %s:%d", ip.c_str(), port);
    }
    
    srs_freep(trd);
    trd = new SrsSTCoroutine("tcp", this);
    if ((err = trd->start()) != srs_success) {
        return srs_error_wrap(err, "start coroutine");
    }
    
    return err;
}

srs_error_t SrsTcpListener::cycle()
{
    srs_error_t err = srs_success;
    while (true) {
        if ((err = trd->pull()) != srs_success) {
            return srs_error_wrap(err, "tcp listener");
        }

        srs_netfd_t fd = srs_accept(lfd, NULL, NULL, SRS_UTIME_NO_TIMEOUT);
        if(fd == NULL){
            return srs_error_new(ERROR_SOCKET_ACCEPT, "accept at fd=%d", srs_netfd_fileno(lfd));
        }
        
        if ((err = srs_fd_closeexec(srs_netfd_fileno(fd))) != srs_success) {
            return srs_error_wrap(err, "set closeexec");
        }
        srs_trace("handler->on_tcp_client(fd)");
        if ((err = handler->on_tcp_client(fd)) != srs_success) {
            return srs_error_wrap(err, "handle fd=%d", srs_netfd_fileno(fd));
        }
    }
    
    return err;
}