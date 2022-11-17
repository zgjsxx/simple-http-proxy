#ifndef SRS_APP_UTILITY_HPP
#define SRS_APP_UTILITY_HPP

#include <srs_kernel_log.hpp>
using std::string;
// Convert level in string to log level in int.
// @return the log level defined in SrsLogLevel.
extern SrsLogLevel srs_get_log_level(std::string level);
// Where peer ip is the client public ip which connected to server.
extern std::string srs_get_peer_ip(int fd);
extern int srs_get_peer_port(int fd);
#endif