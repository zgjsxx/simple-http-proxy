#ifndef _WIN32
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#endif

#include <inttypes.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <sys/stat.h>
#include <srs_kernel_utility.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <srs_kernel_consts.hpp>

using namespace std;

// this value must:
// equals to (SRS_SYS_CYCLE_INTERVAL*SRS_SYS_TIME_RESOLUTION_MS_TIMES)*1000
// @see SRS_SYS_TIME_RESOLUTION_MS_TIMES
#define SYS_TIME_RESOLUTION_US 300*1000

srs_utime_t _srs_system_time_us_cache = 0;
srs_utime_t _srs_system_time_startup_time = 0;


srs_utime_t srs_get_system_time()
{
    if (_srs_system_time_us_cache <= 0) {
        srs_update_system_time();
    }
    
    return _srs_system_time_us_cache;
}

srs_utime_t srs_get_system_startup_time()
{
    if (_srs_system_time_startup_time <= 0) {
        srs_update_system_time();
    }

    return _srs_system_time_startup_time;
}

// For utest to mock it.
#ifndef SRS_OSX
srs_gettimeofday_t _srs_gettimeofday = (srs_gettimeofday_t)::gettimeofday;
#endif


srs_utime_t srs_update_system_time()
{
    timeval now;
    
    if (_srs_gettimeofday(&now, NULL) < 0) {
        // srs_warn("gettimeofday failed, ignore");
        return -1;
    }
    
    // we must convert the tv_sec/tv_usec to int64_t.
    int64_t now_us = ((int64_t)now.tv_sec) * 1000 * 1000 + (int64_t)now.tv_usec;
    
    // for some ARM os, the starttime maybe invalid,
    // for example, on the cubieboard2, the srs_startup_time is 1262304014640,
    // while now is 1403842979210 in ms, diff is 141538964570 ms, 1638 days
    // it's impossible, and maybe the problem of startup time is invalid.
    // use date +%s to get system time is 1403844851.
    // so we use relative time.
    if (_srs_system_time_us_cache <= 0) {
        _srs_system_time_startup_time = _srs_system_time_us_cache = now_us;
        return _srs_system_time_us_cache;
    }
    
    // use relative time.
    int64_t diff = now_us - _srs_system_time_us_cache;
    diff = srs_max(0, diff);
    if (diff < 0 || diff > 1000 * SYS_TIME_RESOLUTION_US) {
        // srs_warn("clock jump, history=%" PRId64 "us, now=%" PRId64 "us, diff=%" PRId64 "us", _srs_system_time_us_cache, now_us, diff);
        _srs_system_time_startup_time += diff;
    }
    
    _srs_system_time_us_cache = now_us;
    // srs_info("clock updated, startup=%" PRId64 "us, now=%" PRId64 "us", _srs_system_time_startup_time, _srs_system_time_us_cache);
    
    return _srs_system_time_us_cache;
}

string srs_string_replace(string str, string old_str, string new_str)
{
    std::string ret = str;
    
    if (old_str == new_str) {
        return ret;
    }
    
    size_t pos = 0;
    while ((pos = ret.find(old_str, pos)) != std::string::npos) {
        ret = ret.replace(pos, old_str.length(), new_str);
        pos += new_str.length();
    }
    
    return ret;
}

bool srs_string_ends_with(string str, string flag)
{
    const size_t pos = str.rfind(flag);
    return (pos != string::npos) && (pos == str.length() - flag.length());
}

string srs_int2str(int64_t value)
{
    // len(max int64_t) is 20, plus one "+-."
    char tmp[21 + 1];
    snprintf(tmp, sizeof(tmp), "%" PRId64, value);
    return tmp;
}

string srs_float2str(double value)
{
    // len(max int64_t) is 20, plus one "+-."
    char tmp[21 + 1];
    snprintf(tmp, sizeof(tmp), "%.2f", value);
    return tmp;
}

bool srs_string_contains(string str, string flag)
{
    return str.find(flag) != string::npos;
}

bool srs_string_contains(string str, string flag0, string flag1)
{
    return str.find(flag0) != string::npos || str.find(flag1) != string::npos;
}

bool srs_string_contains(string str, string flag0, string flag1, string flag2)
{
    return str.find(flag0) != string::npos || str.find(flag1) != string::npos || str.find(flag2) != string::npos;
}

string srs_string_min_match(string str, vector<string> seperators)
{
    string match;
    
    if (seperators.empty()) {
        return str;
    }
    
    size_t min_pos = string::npos;
    for (vector<string>::iterator it = seperators.begin(); it != seperators.end(); ++it) {
        string seperator = *it;
        
        size_t pos = str.find(seperator);
        if (pos == string::npos) {
            continue;
        }
        
        if (min_pos == string::npos || pos < min_pos) {
            min_pos = pos;
            match = seperator;
        }
    }
    
    return match;
}

vector<string> srs_string_split(string str, vector<string> seperators)
{
    vector<string> arr;
    
    size_t pos = string::npos;
    string s = str;
    
    while (true) {
        string seperator = srs_string_min_match(s, seperators);
        if (seperator.empty()) {
            break;
        }
        
        if ((pos = s.find(seperator)) == string::npos) {
            break;
        }

        arr.push_back(s.substr(0, pos));
        s = s.substr(pos + seperator.length());
    }
    
    if (!s.empty()) {
        arr.push_back(s);
    }
    
    return arr;
}

bool srs_path_exists(std::string path)
{
    struct stat st;
    
    // stat current dir, if exists, return error.
    if (stat(path.c_str(), &st) == 0) {
        return true;
    }
    
    return false;
}

string srs_path_dirname(string path)
{
    std::string dirname = path;

    // No slash, it must be current dir.
    size_t pos = string::npos;
    if ((pos = dirname.rfind("/")) == string::npos) {
        return "./";
    }

    // Path under root.
    if (pos == 0) {
        return "/";
    }

    // Fetch the directory.
    dirname = dirname.substr(0, pos);
    return dirname;
}

string srs_path_basename(string path)
{
    std::string dirname = path;
    size_t pos = string::npos;
    
    if ((pos = dirname.rfind("/")) != string::npos) {
        // the basename("/") is "/"
        if (dirname.length() == 1) {
            return dirname;
        }
        dirname = dirname.substr(pos + 1);
    }
    
    return dirname;
}

string srs_path_filename(string path)
{
    std::string filename = path;
    size_t pos = string::npos;
    
    if ((pos = filename.rfind(".")) != string::npos) {
        return filename.substr(0, pos);
    }
    
    return filename;
}

string srs_path_filext(string path)
{
    size_t pos = string::npos;
    
    if ((pos = path.rfind(".")) != string::npos) {
        return path.substr(pos);
    }
    
    return "";
}

void srs_parse_endpoint(string hostport, string& ip, int& port)
{
    const size_t pos = hostport.rfind(":");   // Look for ":" from the end, to work with IPv6.
    if (pos != std::string::npos) {
        if ((pos >= 1) && (hostport[0] == '[') && (hostport[pos - 1] == ']')) {
            // Handle IPv6 in RFC 2732 format, e.g. [3ffe:dead:beef::1]:1935
            ip = hostport.substr(1, pos - 2);
        } else {
            // Handle IP address
            ip = hostport.substr(0, pos);
        }
        
        const string sport = hostport.substr(pos + 1);
        port = ::atoi(sport.c_str());
    } else {
        ip = srs_any_address_for_listener();
        port = ::atoi(hostport.c_str());
    }
}

string srs_any_address_for_listener()
{
    bool ipv4_active = false;
    bool ipv6_active = false;

    if (true) {
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(fd != -1) {
            ipv4_active = true;
            close(fd);
        }
    }
    if (true) {
        int fd = socket(AF_INET6, SOCK_DGRAM, 0);
        if(fd != -1) {
            ipv6_active = true;
            close(fd);
        }
    }

    if (ipv6_active && !ipv4_active) {
        return SRS_CONSTS_LOOPBACK6;
    }
    return SRS_CONSTS_LOOPBACK;
}