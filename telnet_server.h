/**
 * @file telnet_server.h
 * @brief Telnet服务器头文件
 * @date liuliang 2026-01-25
 * 
 * 本文件包含Telnet服务器相关的宏定义和结构体声明
 */

#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/select.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>

#include <fcntl.h>  // 需要添加这个头文件

// 常量定义
#define TELNET_MAX_CLIENTS 5           // 最大客户端数量
#define TELNET_BUFFER_SIZE 1024         // 缓冲区大小
#define TELNET_IDLE_TIMEOUT 600         // 空闲超时时间（秒）- 10分钟
#define TELNET_DEFAULT_PORT 8899          // 默认端口号

// Telnet命令定义
#define TELNET_IAC  255          // 解释为命令
#define TELNET_DONT 254          // 禁止选项
#define TELNET_DO   253          // 允许选项
#define TELNET_WONT 252          // 不会选项
#define TELNET_WILL 251          // 将要选项
#define TELNET_SB   250          // 子协商开始
#define TELNET_SE   240          // 子协商结束
#define TELNET_ECHO 1            // 回显选项

// 客户端状态结构体
typedef struct {
    int sockfd;                     // 客户端socket描述符
    struct sockaddr_in addr;        // 客户端地址信息
    char buffer[TELNET_BUFFER_SIZE];       // 数据缓冲区
    int buffer_len;                 // 缓冲区数据长度
    time_t last_active;             // 最后活动时间
    int authenticated;              // 认证状态（简单示例）
    char username[32];              // 用户名
    int telnet_state;               // Telnet协议状态机状态

    int closed;                     // 连接关闭标志
} telnet_client_t;

// 服务器状态结构体
typedef struct {
    int listen_sockfd;              // 监听socket描述符
    int port;                       // 监听端口
    telnet_client_t *clients[TELNET_MAX_CLIENTS]; // 客户端数组
    int max_clients;                // 最大客户端数
    int running;                    // 服务器运行标志
    fd_set read_fds;                // 用于select的读描述符集
    int max_fd;                     // 最大描述符值
} telnet_server_t;

// 函数声明

// 服务器管理函数
telnet_server_t *telnet_server_init(int port);
int telnet_server_start(telnet_server_t *server);
void telnet_server_stop(telnet_server_t *server);
void telnet_server_destroy(telnet_server_t *server);

// 客户端管理函数
int telnets_add_client(telnet_server_t *server, int sockfd, struct sockaddr_in *addr);
void telnets_remove_client(telnet_server_t *server, int client_index);
void telnets_cleanup_clients(telnet_server_t *server);

// 网络处理函数
void telnets_handle_new_connection(telnet_server_t *server);
void telnets_recv_data_proc(telnet_server_t *server, int client_index);
void telnets_handle_commands(telnet_client_t *client, const char *data, int len);
void telnets_welcome(int sockfd);
void telnets_send_prompt(int sockfd);

// 工具函数
int telnets_find_client_index(telnet_server_t *server, int sockfd);
int telnets_find_available_slot(telnet_server_t *server);
time_t get_current_time(void);
int is_telnet_client_timeout(telnet_client_t *client);
void telnets_trim_newline(char *str);
void telnets_command_proc(telnet_client_t *client, const char *command);
int set_tcp_nonblocking(int sockfd);



#endif // TELNET_SERVER_H