#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <srs_app_utility.hpp>
#include <srs_kernel_utility.hpp>

static SrsProcSelfStat _srs_system_cpu_self_stat;

bool get_proc_self_stat(SrsProcSelfStat& r)
{
#if !defined(SRS_OSX)
    FILE* f = fopen("/proc/self/stat", "r");
    if (f == NULL) {
        srs_warn("open self cpu stat failed, ignore");
        return false;
    }

    // Note that we must read less than the size of r.comm, such as %31s for r.comm is char[32].
    fscanf(f, "%d %31s %c %d %d %d %d "
           "%d %u %lu %lu %lu %lu "
           "%lu %lu %ld %ld %ld %ld "
           "%ld %ld %llu %lu %ld "
           "%lu %lu %lu %lu %lu "
           "%lu %lu %lu %lu %lu "
           "%lu %lu %lu %d %d "
           "%u %u %llu "
           "%lu %ld",
           &r.pid, r.comm, &r.state, &r.ppid, &r.pgrp, &r.session, &r.tty_nr,
           &r.tpgid, &r.flags, &r.minflt, &r.cminflt, &r.majflt, &r.cmajflt,
           &r.utime, &r.stime, &r.cutime, &r.cstime, &r.priority, &r.nice,
           &r.num_threads, &r.itrealvalue, &r.starttime, &r.vsize, &r.rss,
           &r.rsslim, &r.startcode, &r.endcode, &r.startstack, &r.kstkesp,
           &r.kstkeip, &r.signal, &r.blocked, &r.sigignore, &r.sigcatch,
           &r.wchan, &r.nswap, &r.cnswap, &r.exit_signal, &r.processor,
           &r.rt_priority, &r.policy, &r.delayacct_blkio_ticks,
           &r.guest_time, &r.cguest_time);
    
    fclose(f);
#endif

    r.ok = true;
    
    return true;
}

void srs_update_proc_stat()
{
    // @see: http://stackoverflow.com/questions/7298646/calculating-user-nice-sys-idle-iowait-irq-and-sirq-from-proc-stat/7298711
    // @see https://github.com/ossrs/srs/issues/397
    static int user_hz = 0;
    if (user_hz <= 0) {
        user_hz = (int)sysconf(_SC_CLK_TCK);
        srs_info("USER_HZ=%d", user_hz);
        srs_assert(user_hz > 0);
    }
    
    // system cpu stat
    // if (true) {
    //     SrsProcSystemStat r;
    //     if (!get_proc_system_stat(r)) {
    //         return;
    //     }
        
    //     r.sample_time = srsu2ms(srs_update_system_time());
        
    //     // calc usage in percent
    //     SrsProcSystemStat& o = _srs_system_cpu_system_stat;
        
    //     // @see: http://blog.csdn.net/nineday/article/details/1928847
    //     // @see: http://stackoverflow.com/questions/16011677/calculating-cpu-usage-using-proc-files
    //     if (o.total() > 0) {
    //         r.total_delta = r.total() - o.total();
    //     }
    //     if (r.total_delta > 0) {
    //         int64_t idle = r.idle - o.idle;
    //         r.percent = (float)(1 - idle / (double)r.total_delta);
    //     }
        
    //     // upate cache.
    //     _srs_system_cpu_system_stat = r;
    // }
    
    // self cpu stat
    if (true) {
        SrsProcSelfStat r;
        if (!get_proc_self_stat(r)) {
            return;
        }
        
        r.sample_time = srsu2ms(srs_update_system_time());
        
        // calc usage in percent
        SrsProcSelfStat& o = _srs_system_cpu_self_stat;
        
        // @see: http://stackoverflow.com/questions/16011677/calculating-cpu-usage-using-proc-files
        int64_t total = r.sample_time - o.sample_time;
        int64_t usage = (r.utime + r.stime) - (o.utime + o.stime);
        if (total > 0) {
            r.percent = (float)(usage * 1000 / (double)total / user_hz);
        }

        // upate cache.
        _srs_system_cpu_self_stat = r;
    }
}

SrsProcSelfStat* srs_get_self_proc_stat()
{
    return &_srs_system_cpu_self_stat;
}

SrsProcSelfStat::SrsProcSelfStat()
{
    ok = false;
    sample_time = 0;
    percent = 0;
    
    pid = 0;
    memset(comm, 0, sizeof(comm));
    state = '0';
    ppid = 0;
    pgrp = 0;
    session = 0;
    tty_nr = 0;
    tpgid = 0;
    flags = 0;
    minflt = 0;
    cminflt = 0;
    majflt = 0;
    cmajflt = 0;
    utime = 0;
    stime = 0;
    cutime = 0;
    cstime = 0;
    priority = 0;
    nice = 0;
    num_threads = 0;
    itrealvalue = 0;
    starttime = 0;
    vsize = 0;
    rss = 0;
    rsslim = 0;
    startcode = 0;
    endcode = 0;
    startstack = 0;
    kstkesp = 0;
    kstkeip = 0;
    signal = 0;
    blocked = 0;
    sigignore = 0;
    sigcatch = 0;
    wchan = 0;
    nswap = 0;
    cnswap = 0;
    exit_signal = 0;
    processor = 0;
    rt_priority = 0;
    policy = 0;
    delayacct_blkio_ticks = 0;
    guest_time = 0;
    cguest_time = 0;
}

SrsLogLevel srs_get_log_level(string level)
{
    if ("verbose" == level) {
        return SrsLogLevelVerbose;
    } else if ("info" == level) {
        return SrsLogLevelInfo;
    } else if ("trace" == level) {
        return SrsLogLevelTrace;
    } else if ("warn" == level) {
        return SrsLogLevelWarn;
    } else if ("error" == level) {
        return SrsLogLevelError;
    } else {
        return SrsLogLevelDisabled;
    }
}

string srs_get_peer_ip(int fd)
{
    // discovery client information
    sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    if (getpeername(fd, (sockaddr*)&addr, &addrlen) == -1) {
        return "";
    }

    char saddr[64];
    char* h = (char*)saddr;
    socklen_t nbh = (socklen_t)sizeof(saddr);
    const int r0 = getnameinfo((const sockaddr*)&addr, addrlen, h, nbh, NULL, 0, NI_NUMERICHOST);
    if(r0) {
        return "";
    }

    return std::string(saddr);
}

int srs_get_peer_port(int fd)
{
    // discovery client information
    sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    if (getpeername(fd, (sockaddr*)&addr, &addrlen) == -1) {
        return 0;
    }

    int port = 0;
    switch(addr.ss_family) {
        case AF_INET:
            port = ntohs(((sockaddr_in*)&addr)->sin_port);
         break;
        case AF_INET6:
            port = ntohs(((sockaddr_in6*)&addr)->sin6_port);
         break;
    }

    return port;
}