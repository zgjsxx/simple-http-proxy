#include "SocketHelper.h"
#include "Daemon.h"
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
int SocketHelper::createProxySocket(const int port)
{
    int sock;
    struct sockaddr_in proxy_addr;
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons(port);
    proxy_addr.sin_addr.s_addr = INADDR_ANY;
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        return -1;
    }

    int n = 1;

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&n, sizeof(n)) < 0)
    {
        return -1;
    }  
    if (bind(sock, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
        return -1;
    }        
    return sock;
}

void SocketHelper::ProxyListen(int serverSocket)
{
    listen(serverSocket, 128);
}