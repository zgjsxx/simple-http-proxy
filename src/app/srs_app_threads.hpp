#ifndef SRS_APP_THREADS_HPP
#define SRS_APP_THREADS_HPP

#include <pthread.h>
#include <vector>

#include <srs_core.hpp>
#include <srs_core_time.hpp>
#include <srs_app_hourglass.hpp>

extern srs_error_t srs_global_initialize();
class SrsThreadPool;

// Protect server in high load.
class SrsCircuitBreaker : public ISrsFastTimer
{
private:
    // The config for high/critical water level.
    bool enabled_;
    int high_threshold_;
    int high_pulse_;
    int critical_threshold_;
    int critical_pulse_;
    int dying_threshold_;
    int dying_pulse_;
private:
    // Reset the water-level when CPU is low for N times.
    // @note To avoid the CPU change rapidly.
    // high < critical < die
    int hybrid_high_water_level_;
    int hybrid_critical_water_level_;
    int hybrid_dying_water_level_;
public:
    SrsCircuitBreaker();
    virtual ~SrsCircuitBreaker();
public:
    srs_error_t initialize();
public:
    // Whether hybrid server water-level is high.
    bool hybrid_high_water_level();
    bool hybrid_critical_water_level();
    bool hybrid_dying_water_level();
// interface ISrsFastTimer
private:
    srs_error_t on_timer(srs_utime_t interval);
};

class SrsThreadMutex
{
private:
    pthread_mutex_t lock_;
    pthread_mutexattr_t attr_;
public:
    SrsThreadMutex();
    virtual ~SrsThreadMutex();

public:
    void lock();
    void unlock();
};

#define SrsThreadLocker(instance) \
    impl__SrsThreadLocker _SRS_free_##instance(instance)
    
class impl__SrsThreadLocker
{
private:
    SrsThreadMutex* lock;
public:
    impl__SrsThreadLocker(SrsThreadMutex* l){
        lock = l;
        lock->lock();
    }
    virtual ~impl__SrsThreadLocker(){
        lock->unlock();
    }
};

// The information for a thread.
class SrsThreadEntry
{
public:
    SrsThreadPool* pool;
    std::string label;
    std::string name;
    srs_error_t (*start)(void* arg);
    void* arg;
    int num;
    // @see https://man7.org/linux/man-pages/man2/gettid.2.html
    pid_t tid;
public:
    // The thread object.
    pthread_t trd;
    // The exit error of thread.
    srs_error_t err;
public:
    SrsThreadEntry();
    virtual ~SrsThreadEntry();
};

class SrsThreadPool
{
private:
    SrsThreadEntry* entry_;
    srs_utime_t interval_;
private:
    SrsThreadMutex* lock_;
    std::vector<SrsThreadEntry*> threads_;
private:
    // The hybrid server entry, the cpu percent used for circuit breaker.
    SrsThreadEntry* hybrid_;    
    std::vector<SrsThreadEntry*> hybrids_;
private:
    int pid_fd;
public:
    SrsThreadPool();
    virtual ~SrsThreadPool();
public:
    // Setup the thread-local variables.
    static srs_error_t setup_thread_locals();
    // Initialize the thread pool.
    srs_error_t initialize();
private:
    // Require the PID file for the whole process.
    virtual srs_error_t acquire_pid_file();
        
public:
    // Execute start function with label in thread.
    srs_error_t execute(std::string label, srs_error_t (*start)(void* arg), void* arg);
    // Run in the primordial thread, util stop or quit.
    srs_error_t run();
    // Stop the thread pool and quit the primordial thread.
    void stop();
private:
    static void* start(void* arg);
};

// It MUST be thread-safe, global and shared object.
extern SrsThreadPool* _srs_thread_pool;

#endif