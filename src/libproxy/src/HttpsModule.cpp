#include "HttpsModule.h"
#include "ServerLog.h"
#include "openssl/err.h"
int HttpsModule::handshakeServer(SSL *serverSSL)
{
    int ret;
    {
        ERR_clear_error();
        int ret = SSL_connect(serverSSL);   
    }
    if(ret == 1)
    {
        LOG_DEBUG("Handshake with server is sucessful");
        return https_success;
    }
    else
    {
        int err = SSL_get_error(serverSSL, ret);
        if (err == SSL_ERROR_WANT_READ)
        {
            LOG_DEBUG("SSL_connect error: error want read");
            return https_err_want_read;
        }        
 		else if (err == SSL_ERROR_WANT_WRITE)
		{
            LOG_DEBUG("SSL_connect error: error want write");
			return https_err_want_write;
		}       
        else if (err == SSL_ERROR_ZERO_RETURN)
        {
            return https_err_socket_closed;
        }
        LOG_DEBUG("SSL_get_error: 0x%x", err);
        const char *file;
		int line;
		unsigned long errcode = ERR_get_error_line(&file, &line);
        if(errcode)
        {
            char buf[4096];
			ERR_error_string_n(errcode, buf, sizeof(buf));
            LOG_DEBUG("errcode: 0x%x, %s, file: %s:%d", errcode, buf, file, line);
			if (ERR_GET_LIB(errcode) == ERR_LIB_SSL)
			{
				if (SSL_R_UNSUPPORTED_PROTOCOL == ERR_GET_REASON(errcode) ||
					SSL_R_UNSUPPORTED_SSL_VERSION == ERR_GET_REASON(errcode) ||
					SSL_R_UNKNOWN_PROTOCOL == ERR_GET_REASON(errcode) ||
					SSL_R_UNKNOWN_SSL_VERSION == ERR_GET_REASON(errcode) ||
					SSL_R_VERSION_TOO_HIGH == ERR_GET_REASON(errcode) ||
					SSL_R_VERSION_TOO_LOW == ERR_GET_REASON(errcode) ||
					SSL_R_WRONG_SSL_VERSION == ERR_GET_REASON(errcode) ||
					SSL_R_WRONG_VERSION_NUMBER == ERR_GET_REASON(errcode) ||
					SSL_R_BAD_PROTOCOL_VERSION_NUMBER == ERR_GET_REASON(errcode) ||
					SSL_R_TLSV1_ALERT_PROTOCOL_VERSION == ERR_GET_REASON(errcode))
				{
					return https_handshake_server_err_ssl_not_match;
				}
			}
        }
        else
        {
            //unknown error
            return https_unknown_error;
        }

    }

}

int HttpsModule::handshakeClient(SSL *clientSSL)
{
	int ret = 0;
	{
		ERR_clear_error();
		ret = SSL_accept(clientSSL);

	}    
    if(ret == 1)
    {
        return https_success;
    }
    else
    {
       int err = SSL_get_error(clientSSL, ret);
        if (err == SSL_ERROR_WANT_READ)
        {
            LOG_DEBUG("SSL_connect error: error want read");
            return https_err_want_read;
        }        
 		else if (err == SSL_ERROR_WANT_WRITE)
		{
            LOG_DEBUG("SSL_connect error: error want write");
			return https_err_want_write;
		}   
        else if (err == SSL_ERROR_SSL)    
        {
            LOG_DEBUG("SSL_connect error: error ssl");
            return https_unknown_error;
        }
    }
}

void HttpsModule::initOpenssl()
{
    SSL_load_error_strings();
    SSL_library_init();
   
     

    //     SSL_CTX* server_ctx;
    //     server_ctx = SSL_CTX_new(TLS_client_method());
    //     SSL *server_ssl;
    //     server_ssl = SSL_new(server_ctx);

    //     SSL_set_fd(server_ssl, st_netfd_fileno(remote_nfd));
    //     while(1)
    //     {

    //     }


        
    //     EVP_PKEY* ca_key = create_key();
    //     X509 *fake_x509 = create_fake_certificate(server_ssl, ca_key);
    //     int key_length = 2048;
    // 	RSA *new_cert_rsa = get_new_cert_rsa(key_length);
	// 	EVP_PKEY* server_key = EVP_PKEY_new();
    //     EVP_PKEY_set1_RSA(server_key, new_cert_rsa);


	// 	RSA_free(new_cert_rsa);
    //     X509_set_pubkey(fake_x509, server_key);

    //     const char * resp = "HTTP/1.1 200 Connection Established\r\n\r\n";
    //     int len = strlen(resp);
    //     char buffer[len+1];
    //     strcpy(buffer,resp);
    //     if(st_write(cli_nfd, buffer, len, ST_UTIME_NO_TIMEOUT) < 0)
    //     {
    //         //syslog(LOG_NOTICE,"Send http tunnel response  failed\n");
    //         return nullptr;
    //     }   


    //     SSL_CTX * client_ctx;
    //     client_ctx = SSL_CTX_new(TLS_server_method());

    // /* Set the key and cert */
    // // if (SSL_CTX_use_certificate_file(client_ctx, "./cert/server-cert.pem", SSL_FILETYPE_PEM) <= 0) {

    // // }

    // // if (SSL_CTX_use_PrivateKey_file(client_ctx, "./cert/server-key.pem", SSL_FILETYPE_PEM) <= 0 ) {

    // // }  
    //     if (fake_x509 && server_key) {
    //     if (SSL_CTX_use_certificate(client_ctx, fake_x509) != 1)
    //         LOG_DEBUG("Certificate error");
    //     if (SSL_CTX_use_PrivateKey(client_ctx, server_key) != 1)
    //         LOG_DEBUG("key error");
    //     if (SSL_CTX_check_private_key(client_ctx) != 1)
    //         LOG_DEBUG("Private key does not match the certificate public key");
    // }
} 

// X509* create_fake_certificate(SSL* server_ssl, EVP_PKEY *key) 
// {
//     unsigned char buffer[128] = { 0 };
//     int length = 0, loc;
//     X509 *server_x509 = SSL_get_peer_certificate(server_ssl);
//     X509 *fake_x509 = X509_dup(server_x509);
//     if (server_x509 == NULL)
//     {
//         LOG_DEBUG("Fail to get the certificate from server!"); 
//     }

//     X509_set_version(fake_x509, X509_get_version(server_x509));
//     ASN1_INTEGER *a = X509_get_serialNumber(fake_x509);
//     a->data[0] = a->data[0] + 1; 
//     X509_NAME *issuer = X509_NAME_new(); 
//     X509_NAME_add_entry_by_txt(issuer, "CN", MBSTRING_ASC,
//             (const unsigned char*)"TrendXu CA", -1, -1, 0);
//     X509_NAME_add_entry_by_txt(issuer, "O", MBSTRING_ASC, (const unsigned char*)"Thawte Consulting (Pty) Ltd.", -1, -1, 0);
//     X509_NAME_add_entry_by_txt(issuer, "OU", MBSTRING_ASC, (const unsigned char*)"Thawte SGC CA", -1,
//             -1, 0);
//     X509_set_issuer_name(fake_x509, issuer);  
//     X509_sign(fake_x509, key, EVP_sha1()); 

//     return fake_x509;
// }


// // 从文件读取伪造SSL证书时需要的RAS私钥和公钥
// EVP_PKEY* create_key() 
// {
//     EVP_PKEY *key = EVP_PKEY_new();
//     RSA *rsa = RSA_new();

//     FILE *fp;
//     if ((fp = fopen("ca-key.pem", "r")) == NULL)
//     {
//         LOG_DEBUG("private.key");
//     } 
//     PEM_read_RSAPrivateKey(fp, &rsa, NULL, NULL);
    
//     if ((fp = fopen("ca-cert.pem", "r")) == NULL)
//     {
//         LOG_DEBUG("public.key");
//     } 
//     PEM_read_RSAPublicKey(fp, &rsa, NULL, NULL);

//     EVP_PKEY_assign_RSA(key,rsa);
//     return key;
// }

// static RSA *get_new_cert_rsa(int key_length)
// {
// 	RSA *rsa = RSA_new();
// 	BIGNUM *bn = BN_new();

// 	if (BN_set_word(bn, RSA_F4) <= 0 ||
// 			RSA_generate_key_ex(rsa, key_length, bn, NULL) <= 0)
// 	{
// 		abort();
// 	}

// 	BN_free(bn);
// 	return rsa;
// }