#ifndef SRS_KERNEL_UTILITY_HPP
#define SRS_KERNEL_UTILITY_HPP

#include <vector>
#include <srs_core_time.hpp>

// Basic compare function.
#define srs_min(a, b) (((a) < (b))? (a) : (b))
#define srs_max(a, b) (((a) < (b))? (b) : (a))

// Get current system time in srs_utime_t, use cache to avoid performance problem
extern srs_utime_t srs_get_system_time();
extern srs_utime_t srs_get_system_startup_time();

extern srs_utime_t srs_update_system_time();

// The "ANY" address to listen, it's "0.0.0.0" for ipv4, and "::" for ipv6.
// @remark We prefer ipv4, only use ipv6 if ipv4 is disabled.
extern std::string srs_any_address_for_listener();

// Parse the endpoint to ip and port.
// @remark The hostport format in <[ip:]port>, where ip is default to "0.0.0.0".
extern void srs_parse_endpoint(std::string hostport, std::string& ip, int& port);

// Replace old_str to new_str of str
extern std::string srs_string_replace(std::string str, std::string old_str, std::string new_str);
extern bool srs_string_ends_with(std::string str, std::string flag);

// Parse the int64 value to string.
extern std::string srs_int2str(int64_t value);
// Parse the float value to string, precise is 2.
extern std::string srs_float2str(double value);

// Whether string contains with
extern bool srs_string_contains(std::string str, std::string flag);
extern bool srs_string_contains(std::string str, std::string flag0, std::string flag1);
extern bool srs_string_contains(std::string str, std::string flag0, std::string flag1, std::string flag2);
// Find the min match in str for flags.
extern std::string srs_string_min_match(std::string str, std::vector<std::string> flags);
// Split the string by seperator to array.
extern std::vector<std::string> srs_string_split(std::string s, std::string seperator);
extern std::vector<std::string> srs_string_split(std::string s, std::vector<std::string> seperators);

// Whether path exists.
extern bool srs_path_exists(std::string path);
// Get the dirname of path, for instance, dirname("/live/livestream")="/live"
extern std::string srs_path_dirname(std::string path);
// Get the basename of path, for instance, basename("/live/livestream")="livestream"
extern std::string srs_path_basename(std::string path);
// Get the filename of path, for instance, filename("livestream.flv")="livestream"
extern std::string srs_path_filename(std::string path);
// Get the file extension of path, for instance, filext("live.flv")=".flv"
extern std::string srs_path_filext(std::string path);

#include <sys/time.h>
#ifdef SRS_OSX
    #define _srs_gettimeofday gettimeofday
#else
    typedef int (*srs_gettimeofday_t) (struct timeval* tv, struct timezone* tz);
#endif

#endif