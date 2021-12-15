#pragma once
#include "openssl/ssl.h"
class HttpsModule
{
public:
    static int handshakeServer(SSL *serverSSL);
    static int handshakeClient(SSL *clientSSL); 
    static void initOpenssl();
};


enum https_error
{
    https_success = 0,
    https_err_want_read = -1,
    https_err_want_write = -2,
    https_err_socket_closed = -3,
    https_handshake_server_err_ssl_not_match = -4,
    https_unknown_error = 5,
};