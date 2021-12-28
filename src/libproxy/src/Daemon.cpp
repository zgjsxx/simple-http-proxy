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
#include <vector>
#include <netdb.h>

#include "st.h"
#include "ServerLog.h"
#include "SocketHelper.h"
#include "HttpTask.h"
#include "HttpsModule.h"

std::vector<HttpTask*> g_httpTaskVec;
HttpTask* g_currentTask = nullptr;

static void* handleRequest(void* arg)
{
    HttpTask* task = static_cast<HttpTask*>(arg);
    task->taskRun();
    delete task;
    g_currentTask = nullptr;
    LOG_DEBUG("task is over");
}

void Daemon::start()
{
    printf("start proxy ...\n");
    daemonize();
    initStateThread();
    HttpsModule::initOpenssl();
    HttpsModule::prepareResignCA();

    int proxySocket = SocketHelper::createProxySocket(80);
    if(proxySocket < 0)
    {
        LOG_DEBUG("server socket create failed");
        exit(1);
    }
    st_netfd_t client_nfd, proxy_nfd; 
    SocketHelper::ProxyListen(proxySocket);
    //st_netfd_open_socket sets a non-blocking flag on the underlying OS socket
    if ((proxy_nfd = st_netfd_open_socket(proxySocket)) == NULL) {
        LOG_DEBUG("st_netfd_open");
        exit(1);
    }
    struct sockaddr_in client_addr;
    for( ; ; ){
        int n = sizeof(client_addr);
        LOG_DEBUG("st_accept");
        client_nfd = st_accept(proxy_nfd, (struct sockaddr *)&client_addr, &n,
            ST_UTIME_NO_TIMEOUT);
        if (client_nfd == NULL) 
        {
            LOG_DEBUG("client_nfd == null");
            continue;
        }
        LOG_DEBUG("handleRequest");
        HttpTask *task = new HttpTask();
        g_currentTask = task;
        g_httpTaskVec.push_back(task);
        task->m_client_nfd = client_nfd;
        task->m_clientFd= st_netfd_fileno(client_nfd);
        LOG_DEBUG("new socket client %d is in", st_netfd_fileno(client_nfd));
        if (st_thread_create(handleRequest, task, 0, 0) == NULL) 
        {
            LOG_DEBUG("st_thread_create failed");
            continue;
        }
        LOG_DEBUG("create task suc");
    }

    while(1)
    {
        sleep(5);
    }
}


void Daemon::daemonize()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    //chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
    /* Open the log file */
}

void Daemon::initStateThread()
{
    if(st_init() < 0)
    {
        LOG_DEBUG("statethread lib init failed");
        exit(1);
    }
}

