#ifndef SRS_PROTOCOL_ST_HPP
#define SRS_PROTOCOL_ST_HPP

#include <sys/uio.h>
#include <srs_core.hpp>
#include <srs_core_time.hpp>

// Wrap for coroutine.
typedef void* srs_netfd_t;
typedef void* srs_thread_t;
typedef void* srs_cond_t;
typedef void* srs_mutex_t;

// Initialize st, requires epoll.
extern srs_error_t srs_st_init();

extern int srs_thread_setspecific(int key, void *value);
extern int srs_key_create(int *keyp, void (*destructor)(void *));
extern void *srs_thread_getspecific(int key);

// For utest to mock the thread create.
typedef void* (*_ST_THREAD_CREATE_PFN)(void *(*start)(void *arg), void *arg, int joinable, int stack_size);
extern _ST_THREAD_CREATE_PFN _pfn_st_thread_create;

extern int srs_thread_join(srs_thread_t thread, void **retvalp);
extern void srs_thread_interrupt(srs_thread_t thread);
// Close the netfd, and close the underlayer fd.
// @remark when close, user must ensure io completed.
extern void srs_close_stfd(srs_netfd_t& stfd);

// Set the FD_CLOEXEC of FD.
extern srs_error_t srs_fd_closeexec(int fd);

// Set the SO_KEEPALIVE of fd.
extern srs_error_t srs_fd_keepalive(int fd);

// Set the SO_REUSEADDR of fd.
extern srs_error_t srs_fd_reuseaddr(int fd);

extern srs_netfd_t srs_netfd_open_socket(int osfd);
extern srs_netfd_t srs_netfd_open(int osfd);

// For server, listen at TCP endpoint.
extern srs_error_t srs_tcp_listen(std::string ip, int port, srs_netfd_t* pfd);

// Wrap for coroutine.
extern srs_cond_t srs_cond_new();
extern int srs_cond_destroy(srs_cond_t cond);
extern int srs_cond_wait(srs_cond_t cond);
extern int srs_cond_timedwait(srs_cond_t cond, srs_utime_t timeout);
extern int srs_cond_signal(srs_cond_t cond);
extern int srs_cond_broadcast(srs_cond_t cond);

extern srs_mutex_t srs_mutex_new();
extern int srs_mutex_destroy(srs_mutex_t mutex);
extern int srs_mutex_lock(srs_mutex_t mutex);
extern int srs_mutex_unlock(srs_mutex_t mutex);

extern srs_netfd_t srs_accept(srs_netfd_t stfd, struct sockaddr *addr, int *addrlen, srs_utime_t timeout);

extern ssize_t srs_read(srs_netfd_t stfd, void *buf, size_t nbyte, srs_utime_t timeout);

extern int srs_netfd_fileno(srs_netfd_t stfd);

extern int srs_usleep(srs_utime_t usecs);

// The mutex locker.
#define SrsLocker(instance) \
    impl__SrsLocker _SRS_free_##instance(&instance)

class impl__SrsLocker
{
private:
    srs_mutex_t* lock;
public:
    impl__SrsLocker(srs_mutex_t* l) {
        lock = l;
        int r0 = srs_mutex_lock(*lock);
        srs_assert(!r0);
    }
    virtual ~impl__SrsLocker() {
        int r0 = srs_mutex_unlock(*lock);
        srs_assert(!r0);
    }
};

// the socket provides TCP socket over st,
// that is, the sync socket mechanism.
class SrsStSocket
{
private:
    // The recv/send timeout in srs_utime_t.
    // @remark Use SRS_UTIME_NO_TIMEOUT for never timeout.
    srs_utime_t rtm;
    srs_utime_t stm;
    // The recv/send data in bytes
    int64_t rbytes;
    int64_t sbytes;
    // The underlayer st fd.
    srs_netfd_t stfd_;
public:
    SrsStSocket();
    SrsStSocket(srs_netfd_t fd);
    virtual ~SrsStSocket();
private:
    void init(srs_netfd_t fd);
public:
    virtual void set_recv_timeout(srs_utime_t tm);
    virtual srs_utime_t get_recv_timeout();
    virtual void set_send_timeout(srs_utime_t tm);
    virtual srs_utime_t get_send_timeout();
    virtual int64_t get_recv_bytes();
    virtual int64_t get_send_bytes();
public:
    // @param nread, the actual read bytes, ignore if NULL.
    virtual srs_error_t read(void* buf, size_t size, ssize_t* nread);
    virtual srs_error_t read_fully(void* buf, size_t size, ssize_t* nread);
    // @param nwrite, the actual write bytes, ignore if NULL.
    virtual srs_error_t write(void* buf, size_t size, ssize_t* nwrite);
    virtual srs_error_t writev(const iovec *iov, int iov_size, ssize_t* nwrite);
};


#endif