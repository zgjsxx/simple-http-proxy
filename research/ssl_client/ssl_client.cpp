#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <srs_app_utility.hpp>
#include <srs_core.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_threads.hpp>
#include <srs_app_config.hpp>
#include <srs_app_hybrid.hpp>
#include <srs_app_server.hpp>
#include <srs_protocol_log.hpp>
#include <srs_app_log.hpp>
#include <srs_app_policy.hpp>
#include <srs_app_access_log.hpp>
#define MAXBUF 1024

// @global log and context.
ISrsLog* _srs_log = NULL;
// It SHOULD be thread-safe, because it use thread-local thread private data.
ISrsContext* _srs_context = NULL;
// @global config object for app module.
SrsConfig* _srs_config = NULL;
// @global policy object for app module
SrsPolicy* _srs_policy = NULL;
// @global notification object for app module
SrsNotification* _srs_notification = NULL;

SrsAccessLog* _srs_access_log = NULL;
SrsCircuitBreaker* _srs_circuit_breaker = NULL;
void ShowCerts(SSL * ssl)
{
    X509 *cert;
    char *line;

    cert = SSL_get_peer_certificate(ssl);
    // SSL_get_verify_result()是重点，SSL_CTX_set_verify()只是配置启不启用并没有执行认证，调用该函数才会真证进行证书认证
    // 如果验证不通过，那么程序抛出异常中止连接
    if(SSL_get_verify_result(ssl) == X509_V_OK){
        printf("证书验证通过\n");
    }
    if (cert != NULL) {
        printf("数字证书信息:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("证书: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("颁发者: %s\n", line);
        free(line);
        X509_free(cert);
    } else
        printf("无证书信息！\n");
}


int main(int argc, char **argv)
{
    int sockfd, len;
    struct sockaddr_in dest;
    char buffer[MAXBUF + 1];
    SSL_CTX *ctx;
    SSL *ssl;
    _srs_context = new SrsThreadContext();
    _srs_log = new SrsFileLog();
    _srs_log->initialize();

    // if (argc != 5) {
    //     printf("参数格式错误！正确用法如下：\n\t\t%s IP地址 端口\n\t比如:\t%s 127.0.0.1 80\n此程序用来从某个"
    //          "IP 地址的服务器某个端口接收最多 MAXBUF 个字节的消息",
    //          argv[0], argv[0]);
    //     exit(0);
    // }

    /* SSL 库初始化，参看 ssl-server.c 代码 */
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ctx = SSL_CTX_new(SSLv23_client_method());
    if (ctx == NULL) {
        ERR_print_errors_fp(stdout);
        exit(1);
    }

    // // 双向验证
    // // SSL_VERIFY_PEER---要求对证书进行认证，没有证书也会放行
    // // SSL_VERIFY_FAIL_IF_NO_PEER_CERT---要求客户端需要提供证书，但验证发现单独使用没有证书也会放行
    // SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    // // 设置信任根证书
    // if (SSL_CTX_load_verify_locations(ctx, "ca.crt",NULL)<=0){
    //     ERR_print_errors_fp(stdout);
    //     exit(1);
    // }

    // /* 载入用户的数字证书， 此证书用来发送给客户端。 证书里包含有公钥 */
    // if (SSL_CTX_use_certificate_file(ctx, argv[3], SSL_FILETYPE_PEM) <= 0) {
    //     ERR_print_errors_fp(stdout);
    //     exit(1);
    // }
    // /* 载入用户私钥 */
    // if (SSL_CTX_use_PrivateKey_file(ctx, argv[4], SSL_FILETYPE_PEM) <= 0) {
    //     ERR_print_errors_fp(stdout);
    //     exit(1);
    // }
    // /* 检查用户私钥是否正确 */
    // if (!SSL_CTX_check_private_key(ctx)) {
    //     ERR_print_errors_fp(stdout);
    //     exit(1);
    // }

    /* 创建一个 socket 用于 tcp 通信 */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket");
        exit(errno);
    }
    printf("socket created\n");

    /* 初始化服务器端（对方）的地址和端口信息 */
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(atoi(argv[2]));
    if (inet_aton(argv[1], (struct in_addr *) &dest.sin_addr.s_addr) == 0) {
        perror(argv[1]);
        exit(errno);
    }
    printf("address created\n");

    /* 连接服务器 */
    if (connect(sockfd, (struct sockaddr *) &dest, sizeof(dest)) != 0) {
        perror("Connect ");
        exit(errno);
    }
    printf("server connected\n");

    /* 基于 ctx 产生一个新的 SSL */
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    /* 建立 SSL 连接 */
    if (SSL_connect(ssl) == -1)
        ERR_print_errors_fp(stderr);
    else {
        printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
        ShowCerts(ssl);
    }

    srs_update_proc_stat();
    SrsProcSelfStat* u = srs_get_self_proc_stat();
    // Resident Set Size: number of pages the process has in real memory.
    int memory = (int)(u->rss * 4 / 1024);

    printf("Process: cpu=%.2f%%,%dMB\n", u->percent * 100, memory);

    bzero(buffer, MAXBUF + 1);
    strcpy(buffer, "from client->server");
    /* 发消息给服务器 */
    len = SSL_write(ssl, buffer, strlen(buffer));
    if (len < 0)
        printf
            ("消息'%s'发送失败！错误代码是%d，错误信息是'%s'\n",
             buffer, errno, strerror(errno));
    else
        printf("消息'%s'发送成功，共发送了%d个字节！\n",
               buffer, len);


    /* 接收对方发过来的消息，最多接收 MAXBUF 个字节 */
    bzero(buffer, MAXBUF + 1);
    /* 接收服务器来的消息 */
    len = SSL_read(ssl, buffer, MAXBUF);
    if (len > 0)
        printf("接收消息成功:'%s'，共%d个字节的数据\n",
               buffer, len);
    else {
        printf
            ("消息接收失败！错误代码是%d，错误信息是'%s'\n",
             errno, strerror(errno));
        goto finish;
    }


  finish:
    /* 关闭连接 */
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
    srs_update_proc_stat();
    u = srs_get_self_proc_stat();
    // Resident Set Size: number of pages the process has in real memory.
    memory = (int)(u->rss * 4 / 1024);
    printf("Process: cpu=%.2f%%,%dMB\n", u->percent * 100, memory);
    return 0;
}