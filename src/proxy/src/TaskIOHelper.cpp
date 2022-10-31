#include "TaskIOHelper.h"
#include "ServerLog.h"
//if timeout = ST_UTIME_NO_TIMEOUT(-1LL), it means no timeout 
bool TaskIOHelper::waitSocketReadable(int sock,st_utime_t timeout)//micro-second
{
    struct pollfd pd;
    int n;

    pd.fd = sock;
    pd.events = POLLIN;
    pd.revents = 0;

    if ((n = st_poll(&pd, 1, timeout)) < 0) 
    {
        return false;
    } 
    if(n == 0)
    {
        //timeout
        errno = ETIME;
        return false;
    }
    if (pd.revents & POLLNVAL) {
        errno = EBADF;
        return false;
    }    

    return true;
}

bool TaskIOHelper::waitSocketWriteable(int sock, st_utime_t timeout)//micro-second
{
    struct pollfd pd;
    int n;

    pd.fd = sock;
    pd.events = POLLOUT;
    pd.revents = 0;

    if ((n = st_poll(&pd, 1, timeout)) < 0) 
    {
        return false;
    } 
    if(n == 0)
    {
        //timeout
        errno = ETIME;
        return false;
    }
    if (pd.revents & POLLNVAL) {
        errno = EBADF;
        return false;
    }    

    return true;
}

bool TaskIOHelper::waitAnySocket(pollfd* pd, int size, st_utime_t timeout,pollfd &result_pd)
{
    int n;
    for(int i = 0;i < size;i++)
    {
        LOG_DEBUG("fd = %d",pd[i].fd);
        LOG_DEBUG("event = %d",pd[i].events);
    }
    if ((n = st_poll(pd, size, timeout)) < 0) 
    {
        return false;
    } 
    for(int i = 0;i < size;i++)
    {
        if (pd[i].revents & POLLIN) 
        {
            result_pd.fd = pd[i].fd;
            result_pd.revents = pd[i].revents;
            
            break;
        }
        if (pd[i].revents & POLLOUT) 
        {
            result_pd.fd = pd[i].fd;
            result_pd.revents = pd[i].revents;
            break;
        }
    }
    return true;
}