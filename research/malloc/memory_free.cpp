#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <srs_app_utility.hpp>
#include <srs_core.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_threads.hpp>
#include <srs_app_config.hpp>
#include <srs_app_hybrid.hpp>
#include <srs_app_server.hpp>
#include <srs_protocol_log.hpp>
#include <srs_app_log.hpp>
#include <srs_app_policy.hpp>
#include <srs_app_access_log.hpp>
#include <malloc.h>
#define MAXBUF 1024

// @global log and context.
ISrsLog* _srs_log = NULL;
// It SHOULD be thread-safe, because it use thread-local thread private data.
ISrsContext* _srs_context = NULL;
// @global config object for app module.
SrsConfig* _srs_config = NULL;
// @global policy object for app module
SrsPolicy* _srs_policy = NULL;
// @global notification object for app module
SrsNotification* _srs_notification = NULL;

SrsAccessLog* _srs_access_log = NULL;
SrsCircuitBreaker* _srs_circuit_breaker = NULL;

void print_cpu_and_mem()
{
    srs_update_proc_stat();
    SrsProcSelfStat* u = srs_get_self_proc_stat();
    // Resident Set Size: number of pages the process has in real memory.
    int memory = (int)(u->rss * 4 / 1024);
    printf("Process: cpu=%.2f%%,%dMB\n", u->percent * 100, memory);
}

void test1()
{
    vector<char*> char_vec;
    for (int i = 0; i < 5000; i++) {
        char* a = (char*)malloc(1024*127);
        memset(a, '0', 1024*127);
        printf("malloc once\n");
        char_vec.push_back(a);
        usleep(1000);
        print_cpu_and_mem();
    }

    for (size_t i = 0; i < char_vec.size() -1; i++) {
        usleep(1000);
        printf("free once\n");
        free(char_vec[i]);
        print_cpu_and_mem();
        if(i == 2500)
        {
            malloc_trim(0);
        }
    }
}

void test2()
{
    vector<char*> char_vec;
    for (int i = 0; i < 5000; i++) {
        char* a = (char*)malloc(1024*129);
        memset(a, '0', 1024*129);
        printf("malloc once\n");
        char_vec.push_back(a);
        usleep(1000);
        print_cpu_and_mem();
    }

    for (size_t i = 0; i < char_vec.size() -1; i++) {
        usleep(1000);
        printf("free once\n");
        free(char_vec[i]);
        print_cpu_and_mem();
    }
}

int main(int argc, char **argv)
{
    _srs_context = new SrsThreadContext();
    _srs_log = new SrsFileLog();
    _srs_log->initialize();
    test1();
    test2();

    return 0;
}