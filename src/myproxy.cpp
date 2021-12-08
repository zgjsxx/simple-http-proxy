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

#include "st.h"
#include "ServerLog.h"

#define IOBUFSIZE (16*1024)
#define MAX_HEADER_SIZE 8192
static const char *prog;                     /* Program name   */

static void print_sys_error(const char *msg);
static void start_daemon(void);
static int handle_client_request_read(st_netfd_t cli_nfd);

int read_nbytes(st_netfd_t cli_nfd, char *buf, int len)
{
    size_t size = (int) st_read(cli_nfd, buf, len, ST_UTIME_NO_TIMEOUT);
    return size;
}

ssize_t read_line(st_netfd_t cli_nfd, void *buffer, size_t max_size)
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

int extract_host(const char *header,char *remote_host, int& remote_port)
{
    const char * _p = strstr(header, "CONNECT");//CONNECT example.com:443 HTTP/1.1
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
        }
        else
        {
            strncpy(remote_host,_p1 + 1, int(_p2 - _p1));
            remote_port = 80;
        }
        return 0;
    }
    //HTTP request
    //Host: example.com
    _p = strstr(header, "Host:");
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

    }
    else
    {
        int h_len = (int)(_p1 - _p - 5 -1 -1); 
        strncpy(remote_host,_p + 5 + 1,h_len);
        remote_host[h_len] = '\0';
        remote_port = 80;       
    }

    return 0;
}

int create_connection(const char *remote_host, int &remote_port,st_netfd_t &remote_nfd) {
    struct sockaddr_in server_addr;
    struct hostent *server;
    int sock;

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    if ((server = gethostbyname(remote_host)) == NULL) 
    {
        errno = EFAULT;
        return -2;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = PF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(remote_port);    

    if ((remote_nfd = st_netfd_open_socket(sock)) == NULL)
    {
        print_sys_error("st_netfd_open_socket");
        close(sock);
    }
    if (st_connect(remote_nfd, (struct sockaddr *)&server_addr,
        sizeof(server_addr), ST_UTIME_NO_TIMEOUT) < 0) 
    {
        print_sys_error("st_connect");
        st_netfd_close(remote_nfd);
    }
    LOG_DEBUG("server connection create success");

    return 0;
}

static int pass(st_netfd_t in, st_netfd_t out)
{
  char buf[IOBUFSIZE];
  int nw, nr;

  nr = (int) st_read(in, buf, IOBUFSIZE, ST_UTIME_NO_TIMEOUT);
  if (nr <= 0)
    return 0;

  nw = st_write(out, buf, nr, ST_UTIME_NO_TIMEOUT);
  if (nw != nr)
    return 0;

  return 1;
}

static int forward_header(st_netfd_t &remote_nfd, char *header_buffer, int len)
{
    //int len = strlen(header_buffer);
    int write_bytes = st_write(remote_nfd, header_buffer, len, ST_UTIME_NO_TIMEOUT);
    if (write_bytes != len)
        return -1;
    return 0; 
}

static void *handle_request(void *arg)
{
    LOG_DEBUG("new socket client");
    st_netfd_t cli_nfd, rmt_nfd;
    cli_nfd = (st_netfd_t) arg;
    LOG_DEBUG("new socket client %d", st_netfd_fileno(cli_nfd));
    struct pollfd pds[1];
    
    int sock;
    char * header_buffer; //save all of the header items
    bool is_https_connect;

    header_buffer = (char *) malloc(MAX_HEADER_SIZE);

    
    pds[0].fd = st_netfd_fileno(cli_nfd);
    pds[0].events = POLLIN;
    LOG_DEBUG("client socket is %d",pds[0].fd);
    for ( ; ; ) 
    {
        pds[0].revents = 0;
        if (st_poll(pds, 1, ST_UTIME_NO_TIMEOUT) <= 0) {
            print_sys_error("st_poll");
            break;
        }
        if (pds[0].revents & POLLIN) 
        {
            LOG_DEBUG("client socket %d has read event",pds[0].fd);
            memset(header_buffer,0,MAX_HEADER_SIZE);
            char line_buffer[2048];
            char * base_ptr = header_buffer;
            for( ; ;)
            {
                int total_read = read_line(cli_nfd, line_buffer, 2048);
                LOG_DEBUG("total_read %d ",total_read);
                if(total_read <= 0)
                {
                    LOG_DEBUG("client socket %d has closed",pds[0].fd);
                    st_netfd_close(cli_nfd);
                    return nullptr;
                }
                //防止header缓冲区蛮越界
                if(base_ptr + total_read - header_buffer <= MAX_HEADER_SIZE)
                {
                    strncpy(base_ptr,line_buffer,total_read); 
                    base_ptr += total_read;
                } 
                else 
                {
                    return nullptr;
                }

                //读到了空行，http头结束
                if(strcmp(line_buffer,"\r\n") == 0 || strcmp(line_buffer,"\n") == 0)
                {
                    break;
                }
            }
            //syslog(LOG_NOTICE,"client socket %d http header: %s",pds[0].fd ,header_buffer);
            break;
        }
    }
    char * p = strstr(header_buffer,"CONNECT"); /* 判断是否是http 隧道请求 */
    if(p) 
    {
        //syslog(LOG_NOTICE,"receive CONNECT request");
        is_https_connect = true;
    }
    char remote_host[128]; 
    memset(remote_host,0,128);
    int remote_port; 
    //syslog(LOG_NOTICE,"before: %s",remote_host);

    int res = extract_host(header_buffer, remote_host, remote_port);
    //syslog(LOG_NOTICE,"remote host: %s, remote port: %d",remote_host,remote_port);
    st_netfd_t remote_nfd;
    create_connection(remote_host, remote_port,remote_nfd);

    if(!is_https_connect)
    {
        forward_header(remote_nfd,header_buffer,strlen(header_buffer));
    }
    else
    {
        char * resp = "HTTP/1.1 200 Connection Established\r\n\r\n";
        int len = strlen(resp);
        char buffer[len+1];
        strcpy(buffer,resp);
        if(st_write(cli_nfd, buffer, len, ST_UTIME_NO_TIMEOUT) < 0)
        {
            //syslog(LOG_NOTICE,"Send http tunnel response  failed\n");
            return nullptr;
        }       
    }
    
    //http
    struct pollfd pds2[2];

    pds2[0].fd = st_netfd_fileno(cli_nfd);
    pds2[0].events = POLLIN;
    pds2[1].fd = st_netfd_fileno(remote_nfd);
    pds2[1].events = POLLIN;
    for( ; ; )
    {
        pds2[0].revents = 0;
        pds2[1].revents = 0;

        if (st_poll(pds2, 2, ST_UTIME_NO_TIMEOUT) <= 0) 
        {
            print_sys_error("st_poll");
            break;
        }

        if (pds2[0].revents & POLLIN) {
            if (!pass(cli_nfd, remote_nfd))
                break;
        }

        if (pds2[1].revents & POLLIN) {
            if (!pass(remote_nfd, cli_nfd))
                break;
        }
    } 
    st_netfd_close(remote_nfd);
    st_netfd_close(remote_nfd);     
    return nullptr;

}


int main()
{
    prog = "myproxy";
    int sock, n;
    st_netfd_t cli_nfd, srv_nfd;
    struct sockaddr_in local_addr, cli_addr;
    const int port = 8888;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    
    start_daemon();
    if(st_init() < 0)
    {
        print_sys_error("st_init");
        exit(1);
    }
    LOG_DEBUG("start daemon");
    if((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        print_sys_error("socket");
        exit(1);
    }
//    LOG_DEBUG("hello,log4cxx");
    n = 1;

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&n, sizeof(n)) < 0)
    {
        print_sys_error("bind");
        exit(1);
    }

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        print_sys_error("bind");
        exit(1);
    }   

    listen(sock, 128);
    if ((srv_nfd = st_netfd_open_socket(sock)) == NULL) {
        print_sys_error("st_netfd_open");
        exit(1);
    }
    for( ; ; ){
        n = sizeof(cli_addr);
        cli_nfd = st_accept(srv_nfd, (struct sockaddr *)&cli_addr, &n,
            ST_UTIME_NO_TIMEOUT);
        if (cli_nfd == NULL) {
            print_sys_error("st_accept");
            exit(1);
        }
        if (st_thread_create(handle_request, cli_nfd, 0, 0) == NULL) {
            print_sys_error("st_thread_create");
            exit(1);
        }
    }

    while(1)
    {
        //syslog (LOG_NOTICE, "First daemon started.");
        sleep(5);
    }

    //closelog();
    return EXIT_SUCCESS;
}

static void print_sys_error(const char *msg)
{
    //syslog(LOG_NOTICE, "%s: %s: %s\n", prog, msg, strerror(errno));
}


static void start_daemon()
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