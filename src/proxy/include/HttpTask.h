#pragma once
#include <string>

#include "openssl/ssl.h"
#include "st.h"

class HttpTask
{
public:
    HttpTask();
    ~HttpTask();
public:    
    void taskRun();
    int extractHostFromHeader();
    int createServerConnection();
    int getContentLengthFromHeader(bool isRequest);
    int getTransEncodingChunkedFromHeader();
    int getSocketCareEvent(pollfd* ev);
    int handleIOEvent();
    int read_nbytes(st_netfd_t cli_nfd, char *buf, int len);
    ssize_t read_line(st_netfd_t cli_nfd, void *buffer, size_t max_size);
    int processHttpsTransaction();
    int send200ConnectSucToClient();
    int resetClientStatus();

    //http transaction
    int recvReqFromClient(bool useHttpsRead);
    int sendReqToServer(bool useHttpsRead);
    int recvRespFromServer(bool useHttpsRead);
    int sendRespToClient(bool useHttpsRead);
    int checkIPOrDomain(std::string str);
    bool checkNeedsToTunnel();
    int processHttpsTunnel();

public:
    int m_clientFd;
    int m_serverFd;
    st_netfd_t m_client_nfd;
    st_netfd_t m_server_nfd;
    bool m_bTaskOver;
    int m_socketArray[2];
    short m_sockAddEvent[2];
    int m_size;

    SSL_CTX * m_pClientCTX;
    SSL_CTX * m_pServerCTX;
    SSL* m_pClientSSL;
    SSL* m_pServerSSL;

    //request
    struct httpRequest{
    	int m_reqHeaderBufSize;
        char* m_pReqHeaderBuf;
        int bodyBufSize;
        char* m_pReqBodyBuf;
        int m_reqHeaderBufPos;
        int m_reqBodyBufPos;
        int m_reqBodyFileFd;
        int contentLength;
        bool m_headerRecvFinished;
        bool m_bodyRecvFinished;
    }m_req;


    //response
    struct httpResponse{
    	int m_respHeaderBufSize;
    	int BodyBufSize;
    	int m_headerBufSize;
		char* m_pRespHeaderBuf;
		char* m_pRespBodyBuf;
		int m_respHeaderBufPos;
		int m_respBodyBufPos;
		int m_respBodyFileFd;
		int contentLength;
        bool m_headerRecvFinished;
        bool m_bodyRecvFinished;
        bool isChunked;
    }m_resp;

    std::string m_serverHostName;
    int m_serverPort;

    //http transaction
    //step1: recv request from client
    //step2: send request to server
    //step3: recv response from server
    //step4: send response to client
    bool m_bisClientReqRecvFinised;
    bool m_bisClientReqSendFinished;
    bool m_bisServerRespRecvFinised;
    bool m_bisServerRespSendFinised;



    //https transaction
    bool m_bisTunnelOKSend;
    bool m_bisHandShakeWithServerFinished;
    bool m_bisHandShakeWithClientFinished;

    bool m_isHttps;
};
