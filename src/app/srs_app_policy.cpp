#include <srs_app_policy.hpp>
#include <srs_kernel_log.hpp>

SrsPolicy::SrsPolicy()
{
    black_list_vec.push_back("www.baidu.com");
    black_list_vec.push_back("www.example.com");
}

SrsPolicy::~SrsPolicy()
{
    
}

//domain black list
bool SrsPolicy::match_black_list(std::string url)
{
    for(std::string black_item : black_list_vec)
    {
        srs_trace("black_item is %s", black_item.c_str());
        if(url.find(black_item) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}