#include "ServerLog.h"
#include "Daemon.h"

// #define IOBUFSIZE (16*1024)
// #define MAX_HEADER_SIZE 8192
// static const char *prog;                     /* Program name   */

// static int handle_client_request_read(st_netfd_t cli_nfd);

// int read_nbytes(st_netfd_t cli_nfd, char *buf, int len)
// {
//     size_t size = (int) st_read(cli_nfd, buf, len, ST_UTIME_NO_TIMEOUT);
//     return size;
// }

// ssize_t read_line(st_netfd_t cli_nfd, void *buffer, size_t max_size)
// {
//     size_t total_read = 0;
//     size_t num;
//     char ch;
//     char *buf = (char *)buffer;
//     for( ; ; )
//     {
//         num = read_nbytes(cli_nfd ,&ch, 1);
//         if(num == -1)
//         {
//             if(errno == EINTR)
//                 continue;
//             else
//                 return -1; 
//         }
//         else if(num == 0)
//         {
//             if(total_read == 0)
//                 return 0;
//             else
//                 break;
//         }
//         else
//         {
//             if(total_read < max_size - 1)
//             {
//                 total_read++;
//                 *buf++ = ch;
//             }
//             if(ch == '\n')
//                 break;      
//         }
//     }
//     *buf = '\0';
//     return total_read;
// }

// int extract_host(const char *header,char *remote_host, int& remote_port)
// {
//     const char * _p = strstr(header, "CONNECT");//CONNECT example.com:443 HTTP/1.1
//     const char * _p1 = nullptr;
//     const char * _p2 = nullptr;
//     const char * _p3 = nullptr;
//     //HTTP Connect Request
//     if(_p)
//     {
//         _p1 = strstr(_p," ");
//         _p2 = strstr(_p1 + 1 , ":");
//         _p3 = strstr(_p1 + 1, " ");
//         if(_p2)
//         {
//             char s_port[10];
//             bzero(s_port,10);
//             strncpy(remote_host,_p1 + 1, (int)(_p2 - _p1) - 1);
//             strncpy(s_port,_p2 + 1,(int)(_p3 - _p2) - 1);
//             remote_port = atoi(s_port);
//         }
//         else
//         {
//             strncpy(remote_host,_p1 + 1, int(_p2 - _p1));
//             remote_port = 80;
//         }
//         return 0;
//     }
//     //HTTP request
//     //Host: example.com
//     _p = strstr(header, "Host:");
//     if(!_p)
//     {
//         return -1;
//     }
//     _p1 = strchr(_p, '\n');
//     if(!_p1)
//     {
//         return -1;
//     }
//     _p2 = strchr(_p + 5, ':');
//     if(_p2 && _p2 < _p1)
//     {
//         int p_len = (int)(_p1 - _p2 - 1);
//         char s_port[10];
//         bzero(s_port,10);
//         strncpy(s_port, _p2 + 1, p_len);
//         s_port[p_len] = '\0';
//         remote_port = atoi(s_port);

//     }
//     else
//     {
//         int h_len = (int)(_p1 - _p - 5 -1 -1); 
//         strncpy(remote_host,_p + 5 + 1,h_len);
//         remote_host[h_len] = '\0';
//         remote_port = 80;       
//     }

//     return 0;
// }

// int create_connection(const char *remote_host, int &remote_port,st_netfd_t &remote_nfd) {
//     struct sockaddr_in server_addr;
//     struct hostent *server;
//     int sock;

//     if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
//         return -1;
//     }
//     if ((server = gethostbyname(remote_host)) == NULL) 
//     {
//         errno = EFAULT;
//         return -2;
//     }
//     memset(&server_addr, 0, sizeof(server_addr));
//     server_addr.sin_family = PF_INET;
//     memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
//     server_addr.sin_port = htons(remote_port);    

//     if ((remote_nfd = st_netfd_open_socket(sock)) == NULL)
//     {
// //        print_sys_error("st_netfd_open_socket");
//         close(sock);
//     }
//     if (st_connect(remote_nfd, (struct sockaddr *)&server_addr,
//         sizeof(server_addr), ST_UTIME_NO_TIMEOUT) < 0) 
//     {
//       //  print_sys_error("st_connect");
//         st_netfd_close(remote_nfd);
//     }
//     LOG_DEBUG("server connection create success");

//     return 0;
// }

// static int pass(st_netfd_t in, st_netfd_t out)
// {
//   char buf[IOBUFSIZE];
//   int nw, nr;

//   nr = (int) st_read(in, buf, IOBUFSIZE, ST_UTIME_NO_TIMEOUT);
//   if (nr <= 0)
//     return 0;

//   nw = st_write(out, buf, nr, ST_UTIME_NO_TIMEOUT);
//   if (nw != nr)
//     return 0;

//   return 1;
// }

// static int forward_header(st_netfd_t &remote_nfd, char *header_buffer, int len)
// {
//     //int len = strlen(header_buffer);
//     int write_bytes = st_write(remote_nfd, header_buffer, len, ST_UTIME_NO_TIMEOUT);
//     if (write_bytes != len)
//         return -1;
//     return 0; 
// }


// static void *handle_request(void *arg)
// {

//     for ( ; ; ) 
//     {
//         pds[0].revents = 0;
//         if (st_poll(pds, 1, ST_UTIME_NO_TIMEOUT) <= 0) {
//            // print_sys_error("st_poll");
//             break;
//         }
//         if (pds[0].revents & POLLIN) 
//         {
//             LOG_DEBUG("client socket %d has read event",pds[0].fd);
//             memset(header_buffer,0,MAX_HEADER_SIZE);
//             char line_buffer[2048];
//             char * base_ptr = header_buffer;
//             for( ; ;)
//             {
//                 int total_read = read_line(cli_nfd, line_buffer, 2048);
//                 LOG_DEBUG("total_read %d ",total_read);
//                 if(total_read <= 0)
//                 {
//                     LOG_DEBUG("client socket %d has closed",pds[0].fd);
//                     st_netfd_close(cli_nfd);
//                     return nullptr;
//                 }
//                 //防止header缓冲区蛮越界
//                 if(base_ptr + total_read - header_buffer <= MAX_HEADER_SIZE)
//                 {
//                     strncpy(base_ptr,line_buffer,total_read); 
//                     base_ptr += total_read;
//                 } 
//                 else 
//                 {
//                     return nullptr;
//                 }

//                 //读到了空行，http头结束
//                 if(strcmp(line_buffer,"\r\n") == 0 || strcmp(line_buffer,"\n") == 0)
//                 {
//                     break;
//                 }
//             }
//             break;
//         }
//     }
//     char * p = strstr(header_buffer,"CONNECT"); /* 判断是否是http 隧道请求 */
//     if(p) 
//     {
//         is_https_connect = true;
//     }
//     char remote_host[128]; 
//     memset(remote_host,0,128);
//     int remote_port; 

//     int res = extract_host(header_buffer, remote_host, remote_port);
//     st_netfd_t remote_nfd;
//     LOG_DEBUG("log port is %d",remote_port);
//     create_connection(remote_host, remote_port,remote_nfd);

//     if(!is_https_connect)
//     {
//         //HTTP
//         forward_header(remote_nfd,header_buffer,strlen(header_buffer));
//     }

// }


int main()
{
    Daemon::start();
    return EXIT_SUCCESS;
}