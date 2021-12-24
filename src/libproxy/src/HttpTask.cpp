#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "openssl/x509.h"

#include "HttpTask.h"
#include "TaskIOHelper.h"
#include "ServerLog.h"
#include "st.h"
#include "HttpsModule.h"

HttpTask::HttpTask()
{
    m_size = 2;
    for(int i = 0; i < m_size; i++)
    {
        m_socketArray[i] = -1;
        m_sockAddEvent[i] = 0;
    }
    m_serverPort = 80;

    //http transaction state
    m_bisClientReqRecvFinised = false;
    m_bisClientReqSendFinished = false;
    m_bisServerRespRecvFinised = false;
    m_bisServerRespSendFinised = false;


    //request
    m_req.m_reqHeaderBufSize = 8192;
    m_req.m_pReqHeaderBuf = (char *) malloc(m_req.m_reqHeaderBufSize);//8KB
    m_req.m_pReqBodyBuf  =(char *)malloc(m_req.m_reqHeaderBufSize * 10);//80KB
    m_req.m_reqHeaderBufPos = 0;
    m_req.m_reqBodyBufPos = 0;
    m_req.m_headerRecvFinished = false;
    m_req.m_bodyRecvFinished = false;
    m_req.contentLength = 0;

    //response
    m_resp.m_respHeaderBufSize = 8192;
    m_resp.m_pRespHeaderBuf = (char *) malloc(m_resp.m_respHeaderBufSize);
    m_resp.m_pRespBodyBuf = (char *) malloc(m_resp.m_respHeaderBufSize * 10);
    m_resp.m_respHeaderBufPos = 0;
    m_resp.m_respBodyBufPos = 0;
    m_resp.m_headerRecvFinished = false;
    m_resp.m_bodyRecvFinished = false;
    m_resp.contentLength = 0;

    m_isHttps = false;
    m_serverFd = -1;
    m_clientFd = -1;

}
HttpTask::~HttpTask()
{
	free(m_req.m_pReqHeaderBuf);
	free(m_req.m_pReqBodyBuf);

	free(m_resp.m_pRespHeaderBuf);
	free(m_resp.m_pRespBodyBuf);
	st_netfd_close(m_client_nfd);

	if(m_serverFd > 0)
	{
		st_netfd_close(m_server_nfd);
	}

}

void HttpTask::taskRun()
{   
    LOG_DEBUG("http task is running, address: %x",this);
    m_bTaskOver = false;
    while(!m_bTaskOver)
    {
    	int ret = handleIOEvent();
    	if(ret < 0)
    	{
    		m_bTaskOver = true;
    	}
    }
    LOG_DEBUG("http task is finish");


}

int HttpTask::handleIOEvent()
{
	/*
	 * step1: client ->  proxy
	 * step2: proxy  ->  server
	 * step3: proxy  <-  server
	 * step4: client <-  proxy
	 */
	int ret = 0;
	ret = recvReqFromClient(false);
	if(ret < 0)
	{
		LOG_DEBUG("recv from client failed");
		//will terminate this task
		return -1;
	}
	ret = createServerConnection();
	if(ret < 0)
	{
		LOG_DEBUG("create server connection failed");
		return -1;
	}
	if(m_isHttps)
	{
		int ret = processHttpsTransaction();
	}
	else
	{
		ret = sendReqToServer(false);
		if(ret < 0)
		{
			LOG_DEBUG("send to server failed");
			//will terminate this task
		}
		ret = recvRespFromServer(false);
		if(ret < 0)
		{
			LOG_DEBUG("recv from server failed");
			//will terminate this task
		}
		ret = sendRespToClient(false);
		if(ret < 0)
		{
			LOG_DEBUG("send to client failed");
			//will terminate this task
		}

		m_bTaskOver = true;
	}


    return 0;
}

int HttpTask::recvReqFromClient(bool useHttpsRead)
{
	while(!m_bisClientReqRecvFinised)
	{
		char buf[8192];
		int size;
		if(useHttpsRead)
		{
			size = HttpsModule::st_SSL_read(m_clientFd, m_pClientSSL, buf, 8192, ST_UTIME_NO_TIMEOUT);
		}
		else
		{
			size = st_read(m_client_nfd, buf, 8192,ST_UTIME_NO_TIMEOUT);
		}

		LOG_DEBUG("size = %d",size);
		if(size == 0)
		{
			LOG_DEBUG("client is closed");
			return -1;
		}
		if(!m_req.m_headerRecvFinished)
		{
			memcpy(m_req.m_pReqHeaderBuf + m_req.m_reqHeaderBufPos, buf, size);
			m_req.m_reqHeaderBufPos += size;
			char *pos = nullptr;
			pos = strstr(m_req.m_pReqHeaderBuf,"\r\n\r\n");
			if(pos != nullptr)
			{
				m_req.m_headerRecvFinished = true;
				LOG_DEBUG("header recv is finished");
				int ret = getContentLengthFromHeader(true);
				if(ret < 0)
				{
					LOG_DEBUG("extract content length from header failed");
					return -1;
				}
				if(m_req.contentLength == 0)
				{
					LOG_DEBUG("no content length, recv is finished");
					m_req.m_bodyRecvFinished = true;
					m_bisClientReqRecvFinised = true;
					//LOG_DEBUG("m_req.m_pReqHeaderBuf is %s",m_req.m_pReqHeaderBuf);
				}
				else
				{
					LOG_DEBUG("m_req.m_reqHeaderBufPos = %d",m_req.m_reqHeaderBufPos);
					int headerEndPos = int(pos - m_req.m_pReqHeaderBuf) + 4;
					LOG_DEBUG("headerEndPos = %d",headerEndPos);
					int bodySize = m_req.m_reqHeaderBufPos - headerEndPos;
					memcpy(m_req.m_pReqBodyBuf, m_req.m_pReqHeaderBuf + headerEndPos, bodySize);
					memset(m_req.m_pReqHeaderBuf + headerEndPos, 0 , bodySize);
					m_req.m_reqBodyBufPos = bodySize;
					m_req.m_reqHeaderBufPos = headerEndPos;
					LOG_DEBUG("m_req.m_pReqHeaderBuf is %s",m_req.m_pReqHeaderBuf);
					LOG_DEBUG("m_req.m_pReqBodyBuf is %s",m_req.m_pReqBodyBuf);
					if(m_req.m_reqBodyBufPos == m_req.contentLength)
					{
						LOG_DEBUG("first recv , header and body is all recved, do not read more data");
						m_req.m_bodyRecvFinished = true;
						m_bisClientReqRecvFinised = true;
					}
					else
					{
						continue;
					}

				}
			}
			else
			{
				if(size == 8192)
				{
					LOG_DEBUG("too long header, not handle it");
					return -1;
				}
				else
				{
					//continue to recv;
					continue;
				}
			}
		}
		if(!m_req.m_bodyRecvFinished)
		{
			if(m_req.m_reqBodyBufPos == m_req.contentLength)
			{
				LOG_DEBUG("current recv bytes = %d, content length = %d",m_req.m_reqBodyBufPos,m_req.contentLength);
				m_req.m_bodyRecvFinished = true;
				m_bisClientReqRecvFinised = true;
				LOG_DEBUG("body recv is finished");
			}
			else
			{
				memcpy(m_req.m_pReqBodyBuf + m_req.m_reqBodyBufPos, buf, size);
				m_req.m_reqBodyBufPos += size;
				LOG_DEBUG("current recv bytes = %d, content length = %d",m_req.m_reqBodyBufPos,m_req.contentLength);
				continue;
			}

		}

		int ret = extractHostFromHeader();
		if(ret < 0)
		{
			LOG_DEBUG("extract from header failed");
			return -1;
		}

	}
}

int HttpTask::sendReqToServer(bool useHttpsRead)
{
	if(!m_bisClientReqSendFinished)
	{
		LOG_DEBUG("totol size = %d",m_req.m_reqHeaderBufPos);
		int writtenBytes = 0;
		int leftBytes = m_req.m_reqHeaderBufPos;
		char *ptr = m_req.m_pReqHeaderBuf;
		while(leftBytes > 0)
		{
			if(useHttpsRead)
			{
				writtenBytes = HttpsModule::st_SSL_write(m_serverFd, m_pServerSSL, ptr, m_req.m_reqHeaderBufPos,ST_UTIME_NO_TIMEOUT);
			}
			else
			{
				writtenBytes = st_write(m_server_nfd, ptr, m_req.m_reqHeaderBufPos,ST_UTIME_NO_TIMEOUT);
			}

			LOG_DEBUG("writtenBytes = %d",writtenBytes);
			leftBytes -= writtenBytes;
			ptr += writtenBytes;
		}
		LOG_DEBUG("write header to server finish");

		leftBytes = m_req.m_reqBodyBufPos;
		ptr = m_req.m_pReqBodyBuf;
		while(leftBytes > 0)
		{
			if(useHttpsRead)
			{
				writtenBytes = HttpsModule::st_SSL_write(m_serverFd, m_pServerSSL, ptr, m_req.m_reqBodyBufPos, ST_UTIME_NO_TIMEOUT);
			}
			else
			{
				writtenBytes = st_write(m_server_nfd, ptr, m_req.m_reqBodyBufPos,ST_UTIME_NO_TIMEOUT);
			}


			LOG_DEBUG("writtenBytes = %d",writtenBytes);
			leftBytes -= writtenBytes;
			ptr += writtenBytes;
		}
		LOG_DEBUG("write body to server finish");
		m_bisClientReqSendFinished = true;
	}
}

int HttpTask::recvRespFromServer(bool useHttpsRead)
{
	LOG_DEBUG("recvRespFromServer");
	while(!m_bisServerRespRecvFinised)
	{
		char buf[8192];
		int size;
		if(useHttpsRead)
		{
			size = HttpsModule::st_SSL_read(m_serverFd, m_pServerSSL, buf, 8192, ST_UTIME_NO_TIMEOUT);
		}
		else
		{
			size = st_read(m_server_nfd, buf, 8192,ST_UTIME_NO_TIMEOUT);
		}

		LOG_DEBUG("recv size = %d",size);
		LOG_DEBUG("size = %d",size);
		if(size == 0)
		{
			LOG_DEBUG("server is closed");
			return -1;
		}
		if(!m_resp.m_headerRecvFinished)
		{
			memcpy(m_resp.m_pRespHeaderBuf + m_resp.m_respHeaderBufPos, buf, size);
			m_resp.m_respHeaderBufPos += size;
			char *pos = nullptr;
			pos = strstr(m_resp.m_pRespHeaderBuf,"\r\n\r\n");
			if(pos != nullptr)
			{
				m_resp.m_headerRecvFinished = true;
				LOG_DEBUG("header recv is finished");
				int ret = getContentLengthFromHeader(false);//response
				if(ret < 0)
				{
					LOG_DEBUG("extract content length from header failed");
					return -1;
				}
				if(m_resp.contentLength == 0)
				{
					LOG_DEBUG("no content length, recv is finished");
					m_resp.m_bodyRecvFinished = true;
					m_bisServerRespRecvFinised = true;
				}
				else
				{
					LOG_DEBUG("m_resp.m_reqHeaderBufPos = %d",m_resp.m_respHeaderBufPos);
					int headerEndPos = int(pos - m_resp.m_pRespHeaderBuf) + 4;
					LOG_DEBUG("headerEndPos = %d",headerEndPos);
					int bodySize = m_resp.m_respHeaderBufPos - headerEndPos;
					memcpy(m_resp.m_pRespBodyBuf, m_resp.m_pRespHeaderBuf + headerEndPos, bodySize);
					memset(m_resp.m_pRespHeaderBuf + headerEndPos, 0 , bodySize);
					m_resp.m_respBodyBufPos = bodySize;
					m_resp.m_respHeaderBufPos = headerEndPos;
					LOG_DEBUG("m_req.m_pReqHeaderBuf is %s",m_resp.m_pRespHeaderBuf);
					if(m_resp.m_respBodyBufPos == m_resp.contentLength)
					{
						LOG_DEBUG("first recv , header and body is all recved, do not read more data");
						LOG_DEBUG("m_req.m_pReqBodyBuf is %s",m_resp.m_pRespBodyBuf);
						m_resp.m_bodyRecvFinished = true;
						m_bisServerRespRecvFinised = true;
					}
					else
					{
						continue;
					}

				}
			}
			else
			{
				if(size == 8192)
				{
					LOG_DEBUG("too long header, not handle it");
					return -1;
				}
				else
				{
					//continue to recv;
					continue;
				}
			}
		}
		if(!m_resp.m_bodyRecvFinished)
		{
			if(m_resp.m_respBodyBufPos == m_resp.contentLength)
			{
				LOG_DEBUG("current recv bytes = %d, content length = %d",m_resp.m_respBodyBufPos,m_resp.contentLength);
				m_resp.m_bodyRecvFinished = true;
				m_bisServerRespRecvFinised = true;
				LOG_DEBUG("body recv is finished");
			}
			else
			{
				memcpy(m_resp.m_pRespBodyBuf + m_resp.m_respBodyBufPos, buf, size);
				m_resp.m_respBodyBufPos += size;
				LOG_DEBUG("current recv bytes = %d, content length = %d",m_resp.m_respBodyBufPos,m_resp.contentLength);
				if(m_resp.m_respBodyBufPos == m_resp.contentLength)
				{
					m_resp.m_bodyRecvFinished = true;
					m_bisServerRespRecvFinised = true;
					LOG_DEBUG("body recv is finished");
				}
				else
				{
					continue;
				}
			}

		}
	}
}

int HttpTask::sendRespToClient(bool useHttpsRead)
{
	while(!m_bisServerRespSendFinised)
	{
		LOG_DEBUG("totol size = %d",m_resp.m_respHeaderBufPos);
		int writtenBytes = 0;
		int leftBytes = m_resp.m_respHeaderBufPos;
		char *ptr = m_resp.m_pRespHeaderBuf;
		while(leftBytes > 0)
		{
			if(useHttpsRead)
			{
				writtenBytes = HttpsModule::st_SSL_write(m_clientFd, m_pClientSSL, ptr, m_resp.m_respHeaderBufPos,ST_UTIME_NO_TIMEOUT);
			}
			else
			{
				writtenBytes = st_write(m_client_nfd, ptr, m_resp.m_respHeaderBufPos,ST_UTIME_NO_TIMEOUT);
			}

			LOG_DEBUG("writtenBytes = %d",writtenBytes);
			leftBytes -= writtenBytes;
			ptr += writtenBytes;
		}
		LOG_DEBUG("write to client finish");

		leftBytes = m_resp.m_respBodyBufPos;
		ptr = m_resp.m_pRespBodyBuf;
		while(leftBytes > 0)
		{
			if(useHttpsRead)
			{
				writtenBytes = HttpsModule::st_SSL_write(m_clientFd, m_pClientSSL, ptr, m_resp.m_respBodyBufPos,ST_UTIME_NO_TIMEOUT);
			}
			else
			{
				writtenBytes = st_write(m_client_nfd, ptr, m_resp.m_respBodyBufPos,ST_UTIME_NO_TIMEOUT);
			}

			LOG_DEBUG("writtenBytes = %d",writtenBytes);
			leftBytes -= writtenBytes;
			ptr += writtenBytes;
		}
		LOG_DEBUG("write body to client finish");
		m_bisServerRespSendFinised = true;

	}


}

int HttpTask::getSocketCareEvent(pollfd* ev)
{
    int num = 0;
    for(int i = 0;i < 2; i++)
    {
        int fd = m_socketArray[i];
        LOG_DEBUG("fd = %d",fd);
        if(fd <= 0)
        {
            break;
        }
        ev[i].fd = fd;
        ev[i].events |= m_sockAddEvent[i];
        num++;
    }
    return num;
}

int HttpTask::read_nbytes(st_netfd_t cli_nfd, char *buf, int len)
{
    LOG_DEBUG("st_read");
    size_t size = read(st_netfd_fileno(cli_nfd), buf, len);
    LOG_DEBUG("st_read size %d",size);
    return size;
}

ssize_t HttpTask::read_line(st_netfd_t cli_nfd, void *buffer, size_t max_size)
{
    size_t total_read = 0;
    size_t num;
    char ch;
    char *buf = (char *)buffer;
    for( ; ; )
    {
        num = read_nbytes(cli_nfd ,&ch, 1);
        if(num == -1)
        {
            if(errno == EINTR)
                continue;
            else
                return -1; 
        }
        else if(num == 0)
        {
            if(total_read == 0)
                return 0;
            else
                break;
        }
        else
        {
            if(total_read < max_size - 1)
            {
                total_read++;
                *buf++ = ch;
            }
            if(ch == '\n')
                break;      
        }
    }
    *buf = '\0';
    return total_read;
}

int HttpTask::extractHostFromHeader()
{
    char remote_host[100];
    int remote_port;
    const char * _p = strstr(m_req.m_pReqHeaderBuf, "CONNECT");//CONNECT example.com:443 HTTP/1.1
    const char * _p1 = nullptr;
    const char * _p2 = nullptr;
    const char * _p3 = nullptr;
    //HTTP Connect Request
    if(_p)
    {
        _p1 = strstr(_p," ");
        _p2 = strstr(_p1 + 1 , ":");
        _p3 = strstr(_p1 + 1, " ");
        if(_p2)
        {
            char s_port[10];
            bzero(s_port,10);
            strncpy(remote_host,_p1 + 1, (int)(_p2 - _p1) - 1);
            strncpy(s_port,_p2 + 1,(int)(_p3 - _p2) - 1);
            remote_port = atoi(s_port);
            m_serverHostName = remote_host;
            m_serverPort = remote_port;

        }
        else
        {
            strncpy(remote_host,_p1 + 1, int(_p2 - _p1));
            remote_port = 80;
            m_serverHostName = remote_host;
            m_serverPort = remote_port;
        }
        m_isHttps = true;
        return 0;
    }
    //HTTP request
    //Host: example.com
    _p = strstr(m_req.m_pReqHeaderBuf, "Host:");
    if(!_p)
    {
        return -1;
    }
    _p1 = strchr(_p, '\n');
    if(!_p1)
    {
        return -1;
    }
    _p2 = strchr(_p + 5, ':');
    if(_p2 && _p2 < _p1)
    {
        int p_len = (int)(_p1 - _p2 - 1);
        char s_port[10];
        bzero(s_port,10);
        strncpy(s_port, _p2 + 1, p_len);
        s_port[p_len] = '\0';
        remote_port = atoi(s_port);
        m_serverHostName = remote_host;
        m_serverPort = remote_port;
    }
    else
    {
        int h_len = (int)(_p1 - _p - 5 -1 -1); 
        strncpy(remote_host,_p + 5 + 1,h_len);
        remote_host[h_len] = '\0';
        remote_port = 80; 
        m_serverHostName = remote_host;
        m_serverPort = remote_port;      
    }

    return 0;
}


int HttpTask::getContentLengthFromHeader(bool isRequest)
{
    char *pos1 = nullptr;
    char *pos2 = nullptr;
    if(isRequest)
    {
    	pos1 = strstr(m_req.m_pReqHeaderBuf,"Content-Length");
    }
    else
    {
    	pos1 = strstr(m_resp.m_pRespHeaderBuf,"Content-Length");
    }

    if(pos1 == nullptr)
    {
    	return 0;
    }
    pos2 = strstr(pos1,"\r\n");
    if(pos2 == nullptr)
    {
    	LOG_DEBUG("can not find \r\n");
    	return -1;
    }
    int len = pos2 - (pos1 + 15);
    char contentLength[100];
    memset(contentLength, 0, 100);
    memcpy(contentLength, pos1+15, len);
    if(isRequest)
    {
    	m_req.contentLength = atoi(contentLength);
    	LOG_DEBUG("request content length = %d",m_req.contentLength);
    }
    else
    {
    	m_resp.contentLength = atoi(contentLength);
    	LOG_DEBUG("response content length = %d",m_resp.contentLength);
    }

    return 0;
}

int HttpTask::createServerConnection() {
     struct sockaddr_in server_addr;
     struct hostent *server;
     int sock;

     if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    	 LOG_DEBUG("socket create failed");
         return -1;
     }
     if ((server = gethostbyname(m_serverHostName.c_str())) == NULL)
     {
    	 LOG_DEBUG("m_serverHostName is %s", m_serverHostName.c_str());
    	 LOG_DEBUG("gethostbyname failed");
         errno = EFAULT;
         return -2;
     }
     memset(&server_addr, 0, sizeof(server_addr));
     server_addr.sin_family = AF_INET;
     memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
     server_addr.sin_port = htons(m_serverPort);

     LOG_DEBUG("server->h_addr = %s",server->h_addr);

     if ((m_server_nfd = st_netfd_open_socket(sock)) == NULL)
     {
         close(sock);
         return -2;
     }
     LOG_DEBUG("connect to server");
     if (st_connect(m_server_nfd, (struct sockaddr *)&server_addr,
         sizeof(server_addr), ST_UTIME_NO_TIMEOUT) < 0)
     {
       //  print_sys_error("st_connect");
         st_netfd_close(m_server_nfd);
         return -2;
     }
     m_serverFd= st_netfd_fileno(m_server_nfd);
     LOG_DEBUG("server connection create success");

     return 0;
 }

int HttpTask::processHttpsTransaction()
{
	int ret = send200ConnectSucToClient();
	if(ret < 0)
	{
		LOG_DEBUG("send 200 to client failed");
		return ret;
	}

	m_pServerCTX = SSL_CTX_new(TLS_client_method());
	m_pServerSSL = SSL_new(m_pServerCTX);
	//SSL_set_tlsext_host_name(m_pServerSSL, servername)

	LOG_DEBUG("serverFd = %d",m_serverFd);
	SSL_set_fd(m_pServerSSL, m_serverFd);

	HttpsModule::sslHandWithServer(m_pServerSSL, m_serverFd);

	X509 *fake_x509 = X509_new();
	EVP_PKEY* server_key = EVP_PKEY_new();
	HttpsModule::prepareResignedEndpointCert(fake_x509, server_key, m_pServerSSL);

	m_pClientCTX = SSL_CTX_new(TLSv1_2_server_method());
	if (SSL_CTX_use_certificate(m_pClientCTX, fake_x509) != 1)
	{
		LOG_DEBUG("Certificate error");
		return -1;
	}
	if (SSL_CTX_use_PrivateKey(m_pClientCTX, server_key) != 1)
	{
		LOG_DEBUG("key error");
		return -1;
	}

	if (SSL_CTX_check_private_key(m_pClientCTX) != 1)
	{
		LOG_DEBUG("Private key does not match the certificate public key");
		return -1;
	}


	m_pClientSSL = SSL_new(m_pClientCTX);
	LOG_DEBUG("serverFd = %d",m_clientFd);
	SSL_set_fd(m_pClientSSL, m_clientFd);

	HttpsModule::sslHandWithClient(m_pClientSSL, m_clientFd);

	ret = 0;
	ret = resetClientStatus();
	if(ret < 0)
	{
		LOG_DEBUG("reset client status failed");
		//will terminate this task
		return -1;
	}
	ret = recvReqFromClient(true);
	if(ret < 0)
	{
		LOG_DEBUG("recv from client failed");
		//will terminate this task
		return -1;
	}
	ret = sendReqToServer(true);
	if(ret < 0)
	{
		LOG_DEBUG("send to server failed");
		//will terminate this task
	}
	ret = recvRespFromServer(true);
	if(ret < 0)
	{
		LOG_DEBUG("recv from server failed");
		//will terminate this task
	}
	ret = sendRespToClient(true);
	if(ret < 0)
	{
		LOG_DEBUG("send to client failed");
		//will terminate this task
	}
	m_bTaskOver = true;

}

int HttpTask::send200ConnectSucToClient()
{
	const char * resp = "HTTP/1.1 200 Connection Established\r\n\r\n";
	int len = strlen(resp);
	char buffer[len+1];
	strcpy(buffer,resp);
	if(st_write(m_client_nfd , buffer , len,ST_UTIME_NO_TIMEOUT) < 0)
	{
		LOG_DEBUG("Send http tunnel response  failed\n");
		return -1;
	}
	LOG_DEBUG("send 200 to client ok");
	return 0;
}

int HttpTask::resetClientStatus()
{
	m_bisClientReqRecvFinised = false;
	memset(m_req.m_pReqHeaderBuf, 0 ,m_req.m_reqHeaderBufPos);
	memset(m_req.m_pReqBodyBuf, 0 ,m_req.m_reqBodyBufPos);
	m_req.m_reqHeaderBufPos = 0;
	m_req.m_reqBodyBufPos = 0;
	m_req.m_headerRecvFinished = false;
	m_req.m_bodyRecvFinished = false;
    return 0;
}
