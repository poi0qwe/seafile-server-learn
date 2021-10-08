/* 网络相关 */

#ifndef CCNET_NET_H
#define CCNET_NET_H

#ifdef WIN32
    #include <inttypes.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #define UNUSED 
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <sys/un.h>
    #include <net/if.h>
    #include <netinet/tcp.h>
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <event2/util.h>
#else
#include <evutil.h>
#endif

#ifdef WIN32
    /* #define ECONNREFUSED WSAECONNREFUSED */
    /* #define ECONNRESET   WSAECONNRESET */
    /* #define EHOSTUNREACH WSAEHOSTUNREACH */
    /* #define EINPROGRESS  WSAEINPROGRESS */
    /* #define ENOTCONN     WSAENOTCONN */
    /* #define EWOULDBLOCK  WSAEWOULDBLOCK */
    #define sockerrno WSAGetLastError( )
#else
    #include <errno.h>
    #define sockerrno errno
#endif

#ifdef WIN32
extern int inet_aton(const char *string, struct in_addr *addr);
extern const char *inet_ntop(int af, const void *src, char *dst, size_t size);
extern int inet_pton(int af, const char *src, void *dst);
#endif

/* socket地址结构体如下： */
// struct sockaddr {
//   u_short sa_family; char sa_data[14];
// }
// struct sockaddr_in {
//   short int sin_family;        /* Address family */
//   unsigned short int sin_port; /* Port number */
//   struct in_addr sin_addr;     /* Internet address */
//   unsigned char sin_zero[8];   /* Same size as struct sockaddr */
// };
evutil_socket_t ccnet_net_open_tcp (const struct sockaddr *sa, int nonblock); // 作为tcp客户端，连接服务端，nonblock表非阻塞式；返回socket号
evutil_socket_t ccnet_net_bind_tcp (int port, int nonblock); // 创建tcp服务端，绑定端口；返回socket号
evutil_socket_t ccnet_net_accept (evutil_socket_t b, 
                                  struct sockaddr_storage *cliaddr,
                                  socklen_t *len, int nonblock); // tcp服务端接收连接；其中客户端地址信息被存储到cliaddr下

int ccnet_net_make_socket_blocking (evutil_socket_t fd); // 使socket成为阻塞式

/* bind to an IPv4 address, if (*port == 0) the port number will be returned */
evutil_socket_t ccnet_net_bind_v4 (const char *ipaddr, int *port); //作为tcp客户端，绑定ipv4地址；若port=0则获取端口并赋值给port；返回socket号

int  ccnet_netSetTOS   ( evutil_socket_t s, int tos ); // 设置ipv4的ToS

char *sock_ntop(const struct sockaddr *sa, socklen_t salen); // 从socketaddr中提取ip地址，以字符串的形式返回
uint16_t sock_port (const struct sockaddr *sa); // 从socketaddr中提取端口，以int16的形式返回

/* return 1 if addr_str is a valid ipv4 or ipv6 address */
int is_valid_ipaddr (const char *addr_str); // 判断ip是否有效


/* return 0 if success, -1 if error */
int sock_pton (const char *addr_str, uint16_t port, 
               struct sockaddr_storage *sa); // 通过ip地址和端口得到sockaddr_storage

evutil_socket_t udp_client (const char *host, const char *serv,
                struct sockaddr **saptr, socklen_t *lenp); // 作为udp客户端，绑定一个udp端口

int mcast_set_loop(evutil_socket_t sockfd, int onoff); // 多播循环开关

evutil_socket_t create_multicast_sock (struct sockaddr *sasend, socklen_t salen); // 作为多播客户端，连接服务端

#endif
