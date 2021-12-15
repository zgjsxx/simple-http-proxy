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

#include "HttpTask.h"
#include "TaskIOHelper.h"
#include "ServerLog.h"
#include "st.h"

HttpTask::HttpTask()
{
    m_size = 2;
    for(int i = 0; i < m_size; i++)
    {
        m_socketArray[i] = -1;
        m_sockAddEvent[i] = 0;
    }
    m_serverPort = 80;

    m_bisClientReqRecvFinised = false;
    m_bisClientReqSendFinished = false;
    m_bisServerReqRecvFinised = false;
    m_bisServerRespSendFinised = false;
    m_headerBufSize = 8192;
    //request
    m_headerBufPos = 0;
    m_headerBuf = (char *) malloc(m_headerBufSize);

    //response
    m_pRespHeaderBuf = (char *) malloc(m_headerBufSize);
    m_respHeaderBufPos = 0;

    m_pRespBodyBuf = (char *) malloc(m_headerBufSize * 10);
    m_respHeaderBodyBufPos = 0;

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
	//recv request

	while(!m_bisClientReqRecvFinised)
	{
		char buf[8192];
		int size = st_read(m_client_nfd, buf, 8192,ST_UTIME_NO_TIMEOUT);
		memcpy(m_headerBuf + m_headerBufPos, buf, size);
		m_headerBufPos += size;
		char *ret = nullptr;
		ret = strstr(m_headerBuf,"\r\n\r\n");
		if(ret != nullptr)
		{
			LOG_DEBUG("header recv is finished");
			m_bisClientReqRecvFinised = true;
			int ret = extractHostFromHeader();
			if(ret < 0)
			{
				LOG_DEBUG("extract from header failed");
				return -1;
			}
			LOG_DEBUG("host name is %s, port is %d",m_serverHostName.c_str(),m_serverPort);
			if(m_serverPort == 443)
			{
				//TO DO temp block https
				m_bTaskOver = true;
				return -1;
			}
			ret = createServerConnection();
			if(ret < 0)
			{
				LOG_DEBUG("create server connection failed");
				return -1;
			}
		}
		else
		{
			continue;
		}
	}
	if(!m_bisClientReqSendFinished)
	{
		LOG_DEBUG("totol size = %d",m_headerBufPos);
		int writtenBytes = 0;
		int leftBytes = m_headerBufPos;
		char *ptr = m_headerBuf;
		while(leftBytes > 0)
		{
			writtenBytes = st_write(m_server_nfd, ptr, m_headerBufPos,ST_UTIME_NO_TIMEOUT);
			LOG_DEBUG("writtenBytes = %d",writtenBytes);
			leftBytes -= writtenBytes;
			ptr += writtenBytes;
		}
		LOG_DEBUG("write to server finish");
		m_bisClientReqSendFinished = true;
	}

	while(!m_bisServerReqRecvFinised)
	{
		char buf[8192];
		int size = st_read(m_server_nfd, buf, 8192,ST_UTIME_NO_TIMEOUT);
		LOG_DEBUG("recv size = %d",size);
		memcpy(m_pRespHeaderBuf + m_respHeaderBufPos, buf, size);
		m_respHeaderBufPos += size;
		char *ret = nullptr;
		ret = strstr(m_pRespHeaderBuf,"\r\n\r\n");
		if(ret != nullptr)
		{
			LOG_DEBUG("header recv is finished");
			m_bisServerReqRecvFinised = true;

			if(m_respHeaderBufPos > int(ret - m_pRespHeaderBuf))
			{
				LOG_DEBUG("response body size = %d",m_respHeaderBufPos);
				LOG_DEBUG("ret - m_pRespHeaderBuf = %d",int(ret - m_pRespHeaderBuf));
				LOG_DEBUG("recv contains body");
				int bodyPos = int(ret - m_pRespHeaderBuf) + 4;
				//memcpy(m_pRespBodyBuf, m_pRespHeaderBuf, m_respHeaderBufPos - bodyPos);
				LOG_DEBUG("response body size = %d",m_respHeaderBufPos - bodyPos);
			}


		}
		else
		{
			continue;
		}
	}
	while(!m_bisServerRespSendFinised)
	{
		LOG_DEBUG("totol size = %d",m_respHeaderBufPos);
		int writtenBytes = 0;
		int leftBytes = m_respHeaderBufPos;
		char *ptr = m_pRespHeaderBuf;
		while(leftBytes > 0)
		{
			writtenBytes = st_write(m_client_nfd, ptr, m_respHeaderBufPos,ST_UTIME_NO_TIMEOUT);
			LOG_DEBUG("writtenBytes = %d",writtenBytes);
			leftBytes -= writtenBytes;
			ptr += writtenBytes;
		}
		LOG_DEBUG("write to client finish");
		m_bisServerRespSendFinised = true;
	}


	m_bTaskOver = true;
    return 0;
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
    const char * _p = strstr(m_headerBuf, "CONNECT");//CONNECT example.com:443 HTTP/1.1
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
        return 0;
    }
    //HTTP request
    //Host: example.com
    _p = strstr(m_headerBuf, "Host:");
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

 int HttpTask::createServerConnection() {
     struct sockaddr_in server_addr;
     struct hostent *server;
     int sock;

     if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
         return -1;
     }
     if ((server = gethostbyname(m_serverHostName.c_str())) == NULL)
     {
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
     LOG_DEBUG("server connection create success");

     return 0;
 }
