#ifndef SRS_PROTOCOL_UTILITY_HPP
#define SRS_PROTOCOL_UTILITY_HPP
#include <string>
#include <srs_core.hpp>
#include <srs_protocol_io.hpp>

// parse query string to map(k,v).
// must format as key=value&...&keyN=valueN
extern void srs_parse_query_string(std::string q, std::map<std::string, std::string>& query);

// Generate random string [0-9a-z] in size of len bytes.
extern std::string srs_random_str(int len);
// Generate random value, use srandom(now_us) to init seed if not initialized.
extern long srs_random();

// Get local public ip, empty string if no public internet address found.
extern std::string srs_get_public_internet_address(bool ipv4_only = false);

// write large numbers of iovs.
extern srs_error_t srs_write_large_iovs(ISrsProtocolReadWriter* skt, iovec* iovs, int size, ssize_t* pnwrite = NULL);

// Read all content util EOF.
extern srs_error_t srs_ioutil_read_all(ISrsReader* in, std::string& content);

#endif