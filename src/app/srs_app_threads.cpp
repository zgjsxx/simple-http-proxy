#include <unistd.h> 
#include <fcntl.h>
#include <stdlib.h>

#include <srs_core.hpp>
#include <srs_kernel_error.hpp>
#include <srs_app_threads.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_log.hpp>
#include <srs_protocol_st.hpp>
#include <srs_app_config.hpp>
#include <srs_protocol_log.hpp>
#include <srs_app_hybrid.hpp>
#include <srs_app_policy.hpp>
#include <srs_app_utility.hpp>
#include <srs_app_access_log.hpp>
using namespace std;

extern SrsPolicy* _srs_policy;
extern ISrsLog* _srs_log;
extern SrsConfig* _srs_config;
extern ISrsContext* _srs_context;
extern SrsNotification* _srs_notification;
extern SrsAccessLog* _srs_access_log;
#ifdef SRS_OSX
    pid_t gettid() {
        return 0;
    }
#else
    #if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
        #include <sys/syscall.h>
        #define gettid() syscall(SYS_gettid)
    #endif
#endif

SrsThreadMutex::SrsThreadMutex()
{
    // https://man7.org/linux/man-pages/man3/pthread_mutexattr_init.3.html
    int r0 = pthread_mutexattr_init(&attr_);
    // srs_assert(!r0);
    
    // return an error if you lock twice in one thread, 
    // or unlock the lock which is owned by another thread 
    // https://man7.org/linux/man-pages/man3/pthread_mutexattr_gettype.3p.html
    r0 = pthread_mutexattr_settype(&attr_, PTHREAD_MUTEX_ERRORCHECK);
    // srs_assert(!r0);

    r0 = pthread_mutex_init(&lock_, &attr_);
    // srs_assert(!r0);
}

SrsThreadMutex::~SrsThreadMutex()
{
    int r0 = pthread_mutex_destroy(&lock_);
    // srs_assert(!r0);
    r0 = pthread_mutexattr_destroy(&attr_);
    // srs_assert(!r0);
}

void SrsThreadMutex::lock()
{
    // https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3p.html
    // EDEADLK:
    //         The mutex type is PTHREAD_MUTEX_ERRORCHECK and the current
    //         thread already owns the mutex.
    int r0 = pthread_mutex_lock(&lock_);
    //srs_assert(!r0);
}

void SrsThreadMutex::unlock()
{
    int r0 = pthread_mutex_unlock(&lock_);
    // srs_assert(!r0);
}


srs_error_t srs_global_initialize()
{
    srs_error_t err = srs_success;

    // Root global objects.
    _srs_log = new SrsFileLog();
    _srs_context = new SrsThreadContext();
    _srs_config = new SrsConfig();
    // The global objects which depends on ST.
    _srs_hybrid = new SrsHybridServer();
    _srs_policy = new SrsPolicy();
    _srs_notification = new SrsNotification();
    _srs_access_log = new SrsAccessLog();
    return err;
}


SrsThreadEntry::SrsThreadEntry()
{
    pool = NULL;
    start = NULL;
    arg = NULL;
    num = 0;
    tid = 0;

    err = srs_success;
}

SrsThreadEntry::~SrsThreadEntry()
{
    srs_freep(err);
}

SrsThreadPool::SrsThreadPool()
{
    entry_ = NULL;
    lock_ = new SrsThreadMutex();
    hybrid_ = NULL;

    SrsThreadEntry* entry = new SrsThreadEntry();
    threads_.push_back(entry);
    entry_ = entry;

    entry->pool = this;
    entry->label = "primordial";
    entry->start = NULL;
    entry->arg = NULL;
    entry->num = 1;
    entry->trd = pthread_self();
    entry->tid = gettid();

    char buf[256];
    snprintf(buf, sizeof(buf), "srs-master-%d", entry->num);
    entry->name = buf;

    pid_fd = -1;
}

SrsThreadPool::~SrsThreadPool()
{
    srs_freep(lock_);

    if(pid_fd > 0){
        ::close(pid_fd);
        pid_fd = -1;
    }
}

srs_error_t SrsThreadPool::setup_thread_locals()
{
    srs_error_t err = srs_success;

    // Initialize ST
    if ((err = srs_st_init()) != srs_success) {
        return srs_error_wrap(err, "initialize st failed");
    }
    return err;
}

srs_error_t SrsThreadPool::execute(string label, srs_error_t (*start)(void* arg), void* arg)
{
    srs_error_t err = srs_success;

    SrsThreadEntry* entry = new SrsThreadEntry();
    // Update the hybrid thread entry for circuit breaker.
    if (label == "hybrid") {
        hybrid_ = entry;
        hybrids_.push_back(entry);
    }

    // To protect the threads_ for executing thread-safe.
    if (true) {
        SrsThreadLocker(lock_);
        threads_.push_back(entry);
    }
    
    entry->pool = this;
    entry->label = label;
    entry->start = start;
    entry->arg = arg;

    // The id of thread, should equal to the debugger thread id.
    // For gdb, it's: info threads
    // For lldb, it's: thread list
    static int num = entry_->num + 1;
    entry->num = num++;

    char buf[256];
    snprintf(buf, sizeof(buf), "srs-%s-%d", entry->label.c_str(), entry->num);
    entry->name = buf;

    // https://man7.org/linux/man-pages/man3/pthread_create.3.html
    pthread_t trd;
    int r0 = pthread_create(&trd, NULL, SrsThreadPool::start, entry);
    if (r0 != 0) {
        entry->err = srs_error_new(ERROR_THREAD_CREATE, "create thread %s, r0=%d", label.c_str(), r0);
        return srs_error_copy(entry->err);
    }

    entry->trd = trd;

    return err;
}

srs_error_t SrsThreadPool::run()
{
    srs_error_t err = srs_success;

    while (true) {
        // Show statistics 
        srs_update_proc_stat();
        SrsProcSelfStat* u = srs_get_self_proc_stat();
        // Resident Set Size: number of pages the process has in real memory.
        int memory = (int)(u->rss * 4 / 1024);

        srs_trace("Process: cpu=%.2f%%,%dMB, threads=%d", u->percent * 100, memory, (int)threads_.size());
        srs_usleep(1 * SRS_UTIME_SECONDS);
    }
    return err;
}

void SrsThreadPool::stop()
{
    // TODO: FIXME: Should notify other threads to do cleanup and quit.
}

void* SrsThreadPool::start(void* arg)
{
    srs_error_t err = srs_success;

    SrsThreadEntry* entry = (SrsThreadEntry*)arg;

    // Initialize thread-local variables.
    if ((err = SrsThreadPool::setup_thread_locals()) != srs_success) {
        entry->err = err;
        return NULL;
    }

    // Set the thread local fields.
    entry->tid = gettid();

#ifndef SRS_OSX
    // https://man7.org/linux/man-pages/man3/pthread_setname_np.3.html
    pthread_setname_np(pthread_self(), entry->name.c_str());
#else
    pthread_setname_np(entry->name.c_str());
#endif

    srs_trace("Thread #%d: run with tid=%d, entry=%p, label=%s, name=%s", entry->num, (int)entry->tid, entry, entry->label.c_str(), entry->name.c_str());

    if ((err = entry->start(entry->arg)) != srs_success) {
        entry->err = err;
    }

    // We use a special error to indicates the normally done.
    if (entry->err == srs_success) {
        entry->err = srs_error_new(ERROR_THREAD_FINISHED, "finished normally");
    }

    // We do not use the return value, the err has been set to entry->err.
    return NULL;
}

srs_error_t SrsThreadPool::initialize()
{
    srs_error_t err = srs_success;
    if ((err = acquire_pid_file()) != srs_success) {
        return srs_error_wrap(err, "acquire pid file");
    }
    
    // Initialize the master primordial thread.
    SrsThreadEntry* entry = (SrsThreadEntry*)entry_;
    interval_ = _srs_config->get_threads_interval();

    srs_trace("Thread #%d(%s): init name=%s, interval=%dms", entry->num, entry->label.c_str(), entry->name.c_str(), srsu2msi(interval_));
    return err;
}

srs_error_t SrsThreadPool::acquire_pid_file()
{
    std::string pid_file = _srs_config->get_pid_file();

    // -rw-r--r--
    // 644
    int mode = S_IRUSR | S_IWUSR |  S_IRGRP | S_IROTH;
    
    int fd;
    // open pid file
    if ((fd = ::open(pid_file.c_str(), O_WRONLY | O_CREAT, mode)) == -1) {
        return srs_error_new(ERROR_SYSTEM_PID_ACQUIRE, "open pid file=%s", pid_file.c_str());
    }
    // require write lock
    struct flock lock;

    lock.l_type = F_WRLCK; // F_RDLCK, F_WRLCK, F_UNLCK
    lock.l_start = 0; // type offset, relative to l_whence
    lock.l_whence = SEEK_SET;  // SEEK_SET, SEEK_CUR, SEEK_END
    lock.l_len = 0;

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        if(errno == EACCES || errno == EAGAIN) {
            ::close(fd);
            srs_error("srs is already running!");
            return srs_error_new(ERROR_SYSTEM_PID_ALREADY_RUNNING, "srs is already running");
        }
        return srs_error_new(ERROR_SYSTEM_PID_LOCK, "access to pid=%s", pid_file.c_str());
    }
    // truncate file
    // if (ftruncate(fd, 0) != 0) {
    //     return srs_error_new(ERROR_SYSTEM_PID_TRUNCATE_FILE, "truncate pid file=%s", pid_file.c_str());
    // }

    // // write the pid
    // string pid = srs_int2str(getpid());
    // if (write(fd, pid.c_str(), pid.length()) != (int)pid.length()) {
    //     return srs_error_new(ERROR_SYSTEM_PID_WRITE_FILE, "write pid=%s to file=%s", pid.c_str(), pid_file.c_str());
    // }

    // // auto close when fork child process.
    // int val;
    // if ((val = fcntl(fd, F_GETFD, 0)) < 0) {
    //     return srs_error_new(ERROR_SYSTEM_PID_GET_FILE_INFO, "fcntl fd=%d", fd);
    // }
    // val |= FD_CLOEXEC;
    // if (fcntl(fd, F_SETFD, val) < 0) {
    //     return srs_error_new(ERROR_SYSTEM_PID_SET_FILE_INFO, "lock file=%s fd=%d", pid_file.c_str(), fd);
    // }

    // srs_trace("write pid=%s to %s success!", pid.c_str(), pid_file.c_str());
    // pid_fd = fd;    
    return srs_success;
}

// It MUST be thread-safe, global and shared object.
SrsThreadPool* _srs_thread_pool = new SrsThreadPool();
