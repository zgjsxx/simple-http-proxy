#ifndef SRS_APP_LOG_HPP
#define SRS_APP_LOG_HPP

#include "srs_kernel_log.hpp"
#include "srs_app_threads.hpp"
#include "srs_app_reload.hpp"

// For log TAGs.
#define TAG_MAIN "MAIN"
#define TAG_MAYBE "MAYBE"
#define TAG_DTLS_ALERT "DTLS_ALERT"
#define TAG_DTLS_HANG "DTLS_HANG"
#define TAG_RESOURCE_UNSUB "RESOURCE_UNSUB"
#define TAG_LARGE_TIMER "LARGE_TIMER"

class SrsFileLog : public ISrsLog, public ISrsReloadHandler
{
protected: 
    SrsLogLevel level;
private:
    char* log_data;
    bool log_to_file_tank;
    int fd;
    bool utc;
    SrsThreadMutex* mutex_;
public:
    SrsFileLog();
    virtual ~SrsFileLog();
// Interface ISrsLog
public:
    virtual srs_error_t initialize();
    virtual void reopen();
    virtual void verbose(const char* tag, SrsContextId context_id, const char* func, const char* file, int line, const char* fmt, ...);
    virtual void info(const char* tag, SrsContextId context_id, const char* func, const char* file, int line, const char* fmt, ...);
    virtual void trace(const char* tag, SrsContextId context_id, const char* func, const char* file, int line, const char* fmt, ...);
    virtual void warn(const char* tag, SrsContextId context_id, const char* func, const char* file, int line, const char* fmt, ...);
    virtual void error(const char* tag, SrsContextId context_id, const char* func, const char* file, int line, const char* fmt, ...);
private:
    virtual void write_log(int& fd, char* str_log, int size, int level);
    virtual void open_log_file();
};

#endif