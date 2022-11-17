#include <srs_app_access_log.hpp>
#include <srs_kernel_log.hpp>

SrsAccessLog::SrsAccessLog()
{
    init();
}

SrsAccessLog::~SrsAccessLog()
{
    close();
}

void SrsAccessLog::init()
{
    open("./output/access.log");
}

void SrsAccessLog::write_access_log(SrsAccessLogInfo* info)
{
    std::string tmp;
    tmp.append(info->client_ip);tmp.append("\t");
    tmp.append(info->domain);tmp.append("\t");
    tmp.append(std::to_string(info->status_code));tmp.append("\r\n");
    write((void*)tmp.c_str(), tmp.size(), NULL);
    srs_trace("write finish");
}