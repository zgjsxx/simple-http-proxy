#ifndef SRS_APP_POLICY_HPP
#define SRS_APP_POLICY_HPP
#include <vector>
#include <string>

class SrsPolicy
{
public:
    SrsPolicy();
    ~SrsPolicy();
public:
    std::vector<std::string> black_list_vec;
};


#endif