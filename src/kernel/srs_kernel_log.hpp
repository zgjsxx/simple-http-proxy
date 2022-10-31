#ifndef SRS_KERNEL_LOG_HPP
#define SRS_KERNEL_LOG_HPP

#include <srs_core.hpp>

enum SrsLogLevel
{
    SrsLogLevelForbidden = 0x00,
    SrsLogLevelVerbose = 0x01,
    SrsLogLevelInfo = 0x02,
    SrsLogLevelTrace = 0x04,
    SrsLogLevelWarn = 0x08,
    SrsLogLevelError = 0x10,
    SrsLogLevelDisabled = 0x20,
};

// The log interface provides method to write log.
class ISrsLog
{
public:
    ISrsLog();
    virtual ~ISrsLog();
public:
    virtual srs_error_t initialize() = 0;
    virtual void reopen() = 0;

public:
    virtual void verbose(const char* tag, SrsContextId context_id, const char* fmt, const char* func, const char* file, int line, ...) = 0;
    virtual void info(const char* tag, SrsContextId context_id, const char* fmt, const char* func, const char* file, int line, ...) = 0;
    virtual void trace(const char* tag, SrsContextId context_id, const char* fmt, const char* func, const char* file, int line, ...) = 0;
    virtual void warn(const char* tag, SrsContextId context_id, const char* fmt, const char* func, const char* file, int line, ...) = 0;
    virtual void error(const char* tag, SrsContextId context_id, const char* fmt, const char* func, const char* file, int line, ...) = 0;
};

class ISrsContext
{
public:
    ISrsContext();
    virtual ~ISrsContext();
public:
    // Generate a new context id.
    // @remark We do not set to current thread, user should do this.
    virtual SrsContextId generate_id() = 0;
    // Get the context id of current thread.
    virtual const SrsContextId& get_id() = 0;
    // Set the context id of current thread.
    // @return the current context id.
    virtual const SrsContextId& set_id(const SrsContextId& v) = 0;
};
// @global User must provides a log object
extern ISrsLog* _srs_log;
// @global User must implements the LogContext and define a global instance.
extern ISrsContext* _srs_context;

#define srs_verbose(msg, ...) _srs_log->verbose(NULL, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define srs_info(msg, ...)    _srs_log->info(NULL, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define srs_trace(msg, ...)   _srs_log->trace(NULL, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define srs_warn(msg, ...)    _srs_log->warn(NULL, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define srs_error(msg, ...)   _srs_log->error(NULL, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)

// With tag.
#define srs_verbose2(tag, msg, ...) _srs_log->verbose(tag, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define srs_info2(tag, msg, ...)    _srs_log->info(tag, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define srs_trace2(tag, msg, ...)   _srs_log->trace(tag, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define srs_warn2(tag, msg, ...)    _srs_log->warn(tag, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define srs_error2(tag, msg, ...)   _srs_log->error(tag, _srs_context->get_id(), msg, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)

#endif