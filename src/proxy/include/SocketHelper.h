#pragma once
class SocketHelper
{
public:
    static int createProxySocket(const int port);
    static void ProxyListen(int serverSocket);
};