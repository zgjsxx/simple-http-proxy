#include "HttpsModule.h"
#include "ServerLog.h"
#include "openssl/err.h"
#include "TaskIOHelper.h"


EVP_PKEY* HttpsModule::m_caKey = nullptr;


void HttpsModule::sslHandWithServer(SSL *serverSSL, int serverSock)
{
	while(1)
	{	int ret = HttpsModule::handshakeServer(serverSSL);


		if(ret == https_success)
		{
			LOG_DEBUG("establish TLS connection to server successfully");
			break;
		}
		else if(ret == https_err_want_read)
		{
			TaskIOHelper::waitSocketReadable(serverSock,ST_UTIME_NO_TIMEOUT);
			LOG_DEBUG("m_serverFd %d has read event",serverSock);
			continue;
		}
		else if(ret == https_err_want_write)
		{
			TaskIOHelper::waitSocketWriteable(serverSock,ST_UTIME_NO_TIMEOUT);
			LOG_DEBUG("m_serverFd %d has read event",serverSock);
			continue;
		}
		else
		{
			LOG_DEBUG("uknown error");
			break;
		}

	}
}


int HttpsModule::handshakeServer(SSL *serverSSL)
{
    int ret;
    {
        ERR_clear_error();
        ret = SSL_connect(serverSSL);
        LOG_DEBUG("handshakeServer ret = %d",ret);
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


void HttpsModule::sslHandWithClient(SSL* clientSSL, int clientFd)
{
	while(1)
	{

		int ret = HttpsModule::handshakeClient(clientSSL);


		if(ret == https_success)
		{
			LOG_DEBUG("establish TLS connection to client successfully");
			break;
		}
		else if(ret == https_err_want_read)
		{
			TaskIOHelper::waitSocketReadable(clientFd,ST_UTIME_NO_TIMEOUT);
			LOG_DEBUG("m_serverFd %d has read event",clientFd);
			continue;
		}
		else if(ret == https_err_want_write)
		{
			TaskIOHelper::waitSocketWriteable(clientFd,ST_UTIME_NO_TIMEOUT);
			LOG_DEBUG("m_serverFd %d has read event",clientFd);
			continue;
		}
		else
		{
			LOG_DEBUG("uknown error");
			break;
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
}

void HttpsModule::prepareResignCA()
{
	m_caKey = EVP_PKEY_new();
	RSA *rsa = RSA_new();

	FILE *fp;
	if ((fp = fopen("cert/ca-key.pem", "r")) == NULL)
	{
		LOG_DEBUG("load private.key failed");
	}
	PEM_read_RSAPrivateKey(fp, &rsa, NULL, NULL);

	if ((fp = fopen("cert/ca-cert.pem", "r")) == NULL)
	{
		LOG_DEBUG("laod public.key failed");
	}
	PEM_read_RSAPublicKey(fp, &rsa, NULL, NULL);
	EVP_PKEY_assign_RSA(m_caKey, rsa);

}

void HttpsModule::prepareResignedEndpointCert(X509 *fake_x509, EVP_PKEY* server_key, SSL* serverSSL)
{
	X509 *server_x509 = SSL_get_peer_certificate(serverSSL);
	{
		X509_NAME * issuer = X509_get_issuer_name(server_x509);
		char issuer_name[2048] = { 0 };
		X509_NAME_oneline(issuer, issuer_name, sizeof(issuer_name) - 1);
		issuer_name[sizeof(issuer_name) - 1] = 0;

		X509_NAME * subject = X509_get_subject_name(server_x509);
		char subject_name[2048] = { 0 };
				X509_NAME_oneline(subject, subject_name, sizeof(subject_name) - 1);
				subject_name[sizeof(subject_name) - 1] = 0;
		LOG_DEBUG("server certificate subject: %s ",subject_name);
		LOG_DEBUG("server certificate issuer : %s",issuer_name);

	}

	//X509 *fake_x509 = X509_new();
	X509_set_version(fake_x509, 2);//v3
	X509_set_serialNumber(fake_x509,X509_get_serialNumber(server_x509));
	X509_NAME *issuer = X509_NAME_new();
	X509_NAME_add_entry_by_txt(issuer, "CN", MBSTRING_ASC, (const unsigned char*)"www.xx.com", -1, -1, 0);
	X509_NAME_add_entry_by_txt(issuer, "O", MBSTRING_ASC, (const unsigned char*)"XX (Pty) Ltd.", -1, -1, 0);
	X509_NAME_add_entry_by_txt(issuer, "OU", MBSTRING_ASC, (const unsigned char*)"XX CA", -1,-1, 0);
	X509_NAME_add_entry_by_txt(issuer, "L", MBSTRING_ASC, (const unsigned char*)"Boston", -1,-1, 0);
	X509_NAME_add_entry_by_txt(issuer, "ST", MBSTRING_ASC, (const unsigned char*)"MA", -1,-1, 0);
	X509_NAME_add_entry_by_txt(issuer, "C", MBSTRING_ASC, (const unsigned char*)"US", -1,-1, 0);
	X509_set_issuer_name(fake_x509, issuer);
	X509_set_subject_name(fake_x509,X509_get_subject_name(server_x509));


	SSL_set_servername(fake_x509, SSL_get_servername(serverSSL, TLSEXT_NAMETYPE_host_name));

	ASN1_UTCTIME *s=ASN1_UTCTIME_new();
	X509_gmtime_adj(s, long(0));
	int days = 365;
	X509_set_notBefore(fake_x509, s);
	X509_gmtime_adj(s, (long)60*60*24*days);
	X509_set_notAfter(fake_x509, s);
	ASN1_UTCTIME_free(s);
	int key_length = 2048;
	RSA *new_cert_rsa = HttpsModule::get_new_cert_rsa(key_length);
	//EVP_PKEY* server_key = EVP_PKEY_new();
	EVP_PKEY_set1_RSA(server_key, new_cert_rsa);
	X509_set_pubkey(fake_x509, server_key);

	//get subject alternative name (SAN) extension
	std::vector<int> ext_id_list = {NID_subject_alt_name};
	for(auto ext_id: ext_id_list)
	{
		X509_EXTENSION *ext_to_add = NULL;

		int extcount = X509_get_ext_count(server_x509);
		for (int i = 0; i < extcount; i++)
		{
			X509_EXTENSION * ext = X509_get_ext(server_x509, i);
			if (OBJ_obj2nid(X509_EXTENSION_get_object(ext)) == ext_id)
			{
				ext_to_add = ext;
				break;
			}
		}

		if (ext_to_add != NULL)
		{
			X509_add_ext(fake_x509, ext_to_add, -1);
		}
	}
	//end of get san


	int nid = NID_sha256WithRSAEncryption;
	X509_sign(fake_x509, HttpsModule::m_caKey, EVP_get_digestbynid(nid));
}

RSA* HttpsModule::get_new_cert_rsa(int key_length)
 {
 	RSA *rsa = RSA_new();
 	BIGNUM *bn = BN_new();

 	if (BN_set_word(bn, RSA_F4) <= 0 ||
 			RSA_generate_key_ex(rsa, key_length, bn, NULL) <= 0)
 	{
 		abort();
 	}

 	BN_free(bn);
 	return rsa;
 }


int HttpsModule::st_SSL_read(int fd, SSL* ssl, void *buf, size_t nbyte,
	       st_utime_t timeout)
{
	int ret;
	while(1)
	{
		TaskIOHelper::waitSocketReadable(fd, timeout);
		LOG_DEBUG("socket %d has read event",fd);
		ret = SSL_read(ssl , buf ,nbyte);
		if(ret >= 0)break;
	}

	return ret;
}

int HttpsModule::st_SSL_write(int fd, SSL* ssl, void *buf, size_t nbyte,
	       st_utime_t timeout)
{
	TaskIOHelper::waitSocketWriteable(fd, timeout);
	LOG_DEBUG("socket %d can write",fd);
	int ret = SSL_write(ssl , buf ,nbyte);
	return ret;
}
