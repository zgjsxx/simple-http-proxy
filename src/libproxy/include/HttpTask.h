#pragma once
#include <string>
#include "st.h"

class HttpTask
{
public:
    HttpTask();
public:    
    void taskRun();
    int extractHostFromHeader();
    int createServerConnection();
    int getSocketCareEvent(pollfd* ev);
    int handleIOEvent();
    int read_nbytes(st_netfd_t cli_nfd, char *buf, int len);
    ssize_t read_line(st_netfd_t cli_nfd, void *buffer, size_t max_size);
public:
    st_netfd_t m_client_nfd;
    st_netfd_t m_server_nfd;
    bool m_bTaskOver;
    int m_socketArray[2];
    short m_sockAddEvent[2];
    int m_size;


    //request
    int m_headerBufPos;
    int m_headerBufSize;
    char* m_headerBuf;
    //response
    char* m_pRespHeaderBuf;
    char* m_pRespBodyBuf;
    int m_respHeaderBufPos;
    int m_respHeaderBodyBufPos;

    std::string m_serverHostName;
    int m_serverPort;
    bool m_bisClientReqRecvFinised;
    bool m_bisClientReqSendFinished;
    bool m_bisServerReqRecvFinised;
    bool m_bisServerRespSendFinised;
};
