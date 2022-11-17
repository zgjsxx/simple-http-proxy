#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <srs_protocol_utility.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_kernel_error.hpp>
#include <srs_protocol_http_stack.hpp>
#include <srs_core_auto_free.hpp>
#include <srs_kernel_log.hpp>

using namespace std;

void srs_parse_query_string(string q, map<string,string>& query)
{
    // query string flags.
    static vector<string> flags;
    if (flags.empty()) {
        flags.push_back("=");
        flags.push_back(",");
        flags.push_back("&&");
        flags.push_back("&");
        flags.push_back(";");
    }
    
    vector<string> kvs = srs_string_split(q, flags);
    for (int i = 0; i < (int)kvs.size(); i+=2) {
        string k = kvs.at(i);
        string v = (i < (int)kvs.size() - 1)? kvs.at(i+1):"";
        
        query[k] = v;
    }
}

std::string srs_random_str(int len)
{
    static string random_table = "01234567890123456789012345678901234567890123456789abcdefghijklmnopqrstuvwxyz";
    string ret;
    ret.reserve(len);
    for(int i = 0; i < len; ++i){
        ret.append(1, random_table[srs_random() % random_table.size()]);
    }

    return ret;
}

long srs_random()
{
    static bool _random_initialized = false;
    if(!_random_initialized){
        _random_initialized = true;
        //generate the seed
        ::srandom((unsigned long)(srs_update_system_time() |(::getpid() << 13 )));
    }

    return random();
}

std::string _public_internet_address;

string srs_get_public_internet_address(bool ipv4_only)
{
    //TODO
}

srs_error_t srs_write_large_iovs(ISrsProtocolReadWriter* skt, iovec* iovs, int size, ssize_t* pnwrite)
{
    srs_error_t err = srs_success;
    
    // the limits of writev iovs.
#ifndef _WIN32
    // for linux, generally it's 1024.
    static int limits = (int)sysconf(_SC_IOV_MAX);
#else
    static int limits = 1024;
#endif
    
    // send in a time.
    if (size <= limits) {
        if ((err = skt->writev(iovs, size, pnwrite)) != srs_success) {
            return srs_error_wrap(err, "writev");
        }
        return err;
    }
   
    // send in multiple times.
    int cur_iov = 0;
    ssize_t nwrite = 0;
    while (cur_iov < size) {
        int cur_count = srs_min(limits, size - cur_iov);
        if ((err = skt->writev(iovs + cur_iov, cur_count, &nwrite)) != srs_success) {
            return srs_error_wrap(err, "writev");
        }
        cur_iov += cur_count;
        if (pnwrite) {
            *pnwrite += nwrite;
        }
    }
    
    return err;
}

srs_error_t srs_ioutil_read_all(ISrsReader* in, std::string& content)
{
    srs_error_t err = srs_success;

    // Cache to read, it might cause coroutine switch, so we use local cache here.
    char* buf = new char[SRS_HTTP_READ_CACHE_BYTES];
    SrsAutoFreeA(char, buf);

    // Whatever, read util EOF.
    while (true) {
        ssize_t nb_read = 0;
        if ((err = in->read(buf, SRS_HTTP_READ_CACHE_BYTES, &nb_read)) != srs_success) {
            int code = srs_error_code(err);
            if (code == ERROR_SYSTEM_FILE_EOF || code == ERROR_HTTP_RESPONSE_EOF || code == ERROR_HTTP_REQUEST_EOF
                || code == ERROR_HTTP_STREAM_EOF
            ) {
                srs_freep(err);
                return err;
            }
            return srs_error_wrap(err, "read body");
        }

        if (nb_read > 0) {
            content.append(buf, nb_read);
        }
    }

    return err;
}

srs_error_t srs_ioutil_read_part(ISrsReader* in, std::string& content, int size, int& finish)
{
    srs_error_t err = srs_success;
    srs_trace("srs_ioutil_read_part");
    // Cache to read, it might cause coroutine switch, so we use local cache here.
    char* buf = new char[SRS_HTTP_READ_CACHE_BYTES];
    SrsAutoFreeA(char, buf);

    // Whatever, read util EOF.
    while (true) {
        ssize_t nb_read = 0;
        if ((err = in->read(buf, SRS_HTTP_READ_CACHE_BYTES, &nb_read)) != srs_success) {
            int code = srs_error_code(err);
            if (code == ERROR_SYSTEM_FILE_EOF || code == ERROR_HTTP_RESPONSE_EOF || code == ERROR_HTTP_REQUEST_EOF
                || code == ERROR_HTTP_STREAM_EOF
            ) {
                finish = 1;
                srs_freep(err);
                return err;
            }
            return srs_error_wrap(err, "read body");
        }

        if (nb_read > 0) {
            content.append(buf, nb_read);
        }
        srs_trace("content size = %d", content.size());
        if(content.size() > size)
        {
            break;
        }
    }

    return err;
}