#include <srs_app_policy.hpp>
#include <srs_kernel_log.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_file.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_protocol_json.hpp>
#include <srs_core_auto_free.hpp>

SrsPolicy::SrsPolicy()
{
    init();
}

SrsPolicy::~SrsPolicy()
{
    
}

void SrsPolicy::init()
{
    https_descrypt_enable = false;
    loadPolicy();
}

void SrsPolicy::loadPolicy()
{
    srs_error_t err;
    SrsFileReader* read = new SrsFileReader();
    SrsAutoFree(SrsFileReader, read);
    srs_trace("start to load policy");
    read->open("./conf/policy.json");
    std::string poliy;
    char buf[4096];
    ssize_t nread = 0;
    while(1)
    {
        err = read->read(buf, sizeof(buf), &nread);
        if(nread == 0)
        {
            break;
        }
        if(err != srs_success)
        {
            break;
        }
        poliy.append(buf, nread);
        memset(buf, 0 , sizeof(buf));
    }

    SrsJsonAny* any = NULL;
    if ((any = SrsJsonAny::loads(poliy)) == NULL) {
        return;
    }

    SrsJsonObject *obj_req = NULL;
    SrsAutoFree(SrsJsonObject, obj_req);
    if(any->is_object())
    {
        obj_req = any->to_object();
    }

    SrsJsonAny* prop = obj_req->get_property("black_list");
    if(!prop->is_array())
    {
        //parse error
        return;
    }

    SrsJsonArray* array = prop->to_array();

    for(size_t i = 0; i < array->count(); i++)
    {
        std::string black_domain = array->at(i)->to_str();
        srs_trace("push back %s", black_domain.c_str());
        black_list_vec.push_back(black_domain);
    }

    prop = obj_req->get_property("https_descrypt");
    if(!prop->is_boolean())
    {
        return;
    }
    https_descrypt_enable = prop->to_boolean();


    prop = obj_req->get_property("tunnel_domain");
    if(!prop->is_array())
    {
        return;
    }

    array = prop->to_array();

    for(size_t i = 0; i < array->count(); i++)
    {
        std::string tunnel_domain = array->at(i)->to_str();
        srs_trace("push back %s", tunnel_domain.c_str());
        tunnel_domain_vec.push_back(tunnel_domain);
    }
}

//domain black list
bool SrsPolicy::match_black_list(std::string url)
{
    for(std::string black_item : black_list_vec)
    {
        // srs_trace("black_item is %s", black_item.c_str());
        if(url.find(black_item) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

// tunnel domain list
bool SrsPolicy::match_tunnel_domain_list(std::string url)
{
    for(std::string tunnel_domain : tunnel_domain_vec)
    {
        srs_trace("tunnel_domain_item is %s", tunnel_domain.c_str());
        if(url.find(tunnel_domain) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

bool SrsPolicy::is_https_descrypt_enable()
{
    return https_descrypt_enable;
}

SrsNotification::SrsNotification()
{
    init();
}

SrsNotification::~SrsNotification()
{

}

void SrsNotification::init()
{
    loadNotification();
}

void SrsNotification::loadNotification()
{
    srs_error_t err;
    SrsFileReader* read = new SrsFileReader();
    SrsAutoFree(SrsFileReader, read);
    read->open("./conf/block_page/black_list.html");
    char buf[4096];
    ssize_t nread = 0;
    while(1)
    {
        err = read->read(buf, sizeof(buf), &nread);
        if(nread == 0)
        {
            break;
        }
        if(err != srs_success)
        {
            break;
        }
        notification_page.append(buf, nread);
        memset(buf, 0 , sizeof(buf));
    }
    
}

std::string SrsNotification::getNotification(std::string domain, std::string client_ip)
{
    string res = srs_string_replace(notification_page, "%URL%", domain);
    res = srs_string_replace(res, "%IP%", client_ip);
    return res;
}
