#pragma once

#include "HttpTask.h"
#include "openssl/ssl.h"
#include "st.h"

class HttpsModule
{
public:
	static void sslHandWithClient(SSL* clientSSL, int clientFd);
	static int sslHandWithServer(SSL *serverSSL, int serverSock);
    static int handshakeServer(SSL *serverSSL);
    static int handshakeClient(SSL *clientSSL); 
    static void initOpenssl();
    static void prepareResignCA();
    static void prepareResignedEndpointCert(X509 *fake_x509, EVP_PKEY* server_key, SSL* serverSSL);
    static RSA* get_new_cert_rsa(int key_length);
    static int st_SSL_read(int fd, SSL* ssl, void *buf, size_t nbyte,
		       st_utime_t timeout, HttpTask* task);
    static int st_SSL_write(int fd, SSL* ssl, void *buf, size_t nbyte,
		       st_utime_t timeout, HttpTask* task);

public:
    static EVP_PKEY *m_caKey;
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
