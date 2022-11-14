#ifndef SRS_APP_POLICY_HPP
#define SRS_APP_POLICY_HPP
#include <vector>
#include <string>
#include <string.h>

using std::string;
class SrsPolicy
{
public:
    SrsPolicy();
    ~SrsPolicy();
public:
    virtual void init();
public:
    virtual void loadPolicy();
public:
    virtual bool is_https_descrypt_enable();
    virtual bool match_black_list(std::string domain);
    virtual bool match_tunnel_domain_list(std::string domain);
public:
    std::vector<std::string> black_list_vec;
    std::vector<std::string> tunnel_domain_vec;
    bool https_descrypt_enable;
};

class SrsNotification
{
public:
    SrsNotification();
    ~SrsNotification();
public:
    virtual void init();
public:
    virtual void loadNotification();
public:
    virtual string getNotification(std::string domain, std::string client_ip);
private:
    std::string notification_page;
};


#endif