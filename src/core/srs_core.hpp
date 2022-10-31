#ifndef SRS_CORE_HPP
#define SRS_CORE_HPP

// The version config.
#include <srs_core_version.hpp>
#include <srs_auto_headers.hpp>
#define SRS_INTERNAL_STR(v) #v
#define SRS_XSTR(v) SRS_INTERNAL_STR(v)

// The project informations, may sent to client in HTTP header or RTMP metadata.
#define RTMP_SIG_SRS_KEY "SRS"
#define RTMP_SIG_SRS_CODE "Bee"
#define RTMP_SIG_SRS_URL "https://github.com/ossrs/srs"
#define RTMP_SIG_SRS_LICENSE "MIT"
#define SRS_CONSTRIBUTORS "https://github.com/ossrs/srs/blob/develop/trunk/AUTHORS.md#contributors"
#define RTMP_SIG_SRS_VERSION SRS_XSTR(VERSION_MAJOR) "." SRS_XSTR(VERSION_MINOR) "." SRS_XSTR(VERSION_REVISION)
#define RTMP_SIG_SRS_SERVER RTMP_SIG_SRS_KEY "/" RTMP_SIG_SRS_VERSION "(" RTMP_SIG_SRS_CODE ")"
#define RTMP_SIG_SRS_DOMAIN "ossrs.net"

// For platform specified headers and defines.
#include <srs_core_platform.hpp>
// The time unit for timeout, interval or duration.
#include <srs_core_time.hpp>

class SrsCplxError;
typedef SrsCplxError* srs_error_t;

// To free the p and set to NULL.
// @remark The p must be a pointer T*.
#define srs_freep(p) \
    if (p) { \
        delete p; \
        p = NULL; \
    } \
    (void)0

// Please use the freepa(T[]) to free an array, otherwise the behavior is undefined.
//The (void)0 is used to avoid srs_freepa to play as a right value
#define srs_freepa(pa) \
    if(pa){ \
        delete[] pa; \
        pa = NULL; \
    } \
    (void)0


#include <string>
#if 1
class _SrsContextId
{
private:
    std::string v_;
public:
    _SrsContextId();
    _SrsContextId(const _SrsContextId& cp);
    _SrsContextId& operator=(const _SrsContextId& cp);
    virtual ~_SrsContextId();
public:
    const char* c_str() const;
    bool empty() const;
    int compare(const _SrsContextId& to) const;
    _SrsContextId& set_value(const std::string& v);
};
typedef _SrsContextId SrsContextId;
#else
typedef std::string SrsContextId;
#endif

#endif