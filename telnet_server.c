/**
 * @file telnet_server.c
 * @brief Telnet服务器
 * @date liuliang 2026-01-25
 * 
 * 本文件包含Telnet服务器的实现
 */

#include "telnet_server.h"

// 全局变量，用于信号处理
// static volatile sig_atomic_t signal_received = 0;

// 信号处理函数
// static void signal_handler(int sig) {
//     signal_received = sig;
// }


// 创建服务器实例
telnet_server_t *telnet_server_init(int port) 
{
    telnet_server_t *server = (telnet_server_t *)malloc(sizeof(telnet_server_t));
    if (!server) 
    {
        perror("Failed to allocate server memory");
        return NULL;
    }
    
    // 初始化服务器结构
    memset(server, 0, sizeof(telnet_server_t));
    server->port = port;
    server->max_clients = TELNET_MAX_CLIENTS;
    server->running = 1;
    server->listen_sockfd = -1;
    server->max_fd = 0;
    
    // 初始化客户端数组
    for (int i = 0; i < TELNET_MAX_CLIENTS; i++) 
    {
        server->clients[i] = NULL;
    }
    
    // 设置信号处理
    // signal(SIGINT, signal_handler);
    // signal(SIGTERM, signal_handler);
    
    return server;
}

// 启动服务器
int telnet_server_start(telnet_server_t *server) 
{
    struct sockaddr_in server_addr;
    int opt = 1;
    
    // 创建socket
    server->listen_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_sockfd < 0) 
    {
        perror("Socket creation failed");
        return -1;
    }

    // 设置监听 socket 为非阻塞模式
    if (set_tcp_nonblocking(server->listen_sockfd) < 0) 
    {
        perror("Failed to set non-blocking on listening socket");
        close(server->listen_sockfd);
        return -1;
    }

    // 设置socket选项，允许地址重用
    if (setsockopt(server->listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
    {
        perror("Setsockopt failed");
        close(server->listen_sockfd);
        return -1;
    }
    
    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server->port);
    
    // 绑定socket
    if (bind(server->listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("Bind failed");
        close(server->listen_sockfd);
        return -1;
    }
    
    // 开始监听
    if (listen(server->listen_sockfd, 5) < 0) 
    {
        perror("Listen failed");
        close(server->listen_sockfd);
        return -1;
    }
    
    printf("Telnet server started on port %d\n", server->port);
    printf("Max clients: %d\n", server->max_clients);
    printf("Idle timeout: %d seconds\n", TELNET_IDLE_TIMEOUT);
    
    // 设置最大文件描述符
    server->max_fd = server->listen_sockfd;
    
    // 主服务器循环
    while (server->running) 
    {
        struct timeval timeout;
        fd_set read_fds;
        int activity;
        
        // 清空描述符集
        FD_ZERO(&read_fds);
        
        // 添加监听socket到描述符集
        FD_SET(server->listen_sockfd, &read_fds);
        
        // 添加所有客户端socket到描述符集
        for (int i = 0; i < server->max_clients; i++) 
        {
            if (server->clients[i] != NULL) 
            {
                int sockfd = server->clients[i]->sockfd;
                FD_SET(sockfd, &read_fds);
                
                // 更新最大描述符值
                if (sockfd > server->max_fd) 
                {
                    server->max_fd = sockfd;
                }
            }
        }
        
        // 设置select超时时间为1秒
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        // 使用select监视socket活动
        activity = select(server->max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) 
        {
            perror("Select error");
            continue;
        }
        
        // 检查是否有新连接
        if (FD_ISSET(server->listen_sockfd, &read_fds)) 
        {
            telnets_handle_new_connection(server);
        }
        
        // 检查客户端socket活动
        for (int i = 0; i < server->max_clients; i++) 
        {
            if (server->clients[i] != NULL) 
            {
                int sockfd = server->clients[i]->sockfd;
                
                if (FD_ISSET(sockfd, &read_fds)) 
                {
                    telnets_recv_data_proc(server, i);
                }
            }
        }
        
        // 清理超时客户端
        telnets_cleanup_clients(server);
    }
    
    return 0;
}

// 停止服务器
void telnet_server_stop(telnet_server_t *server) 
{
    server->running = 0;
}

// 销毁服务器资源
void telnet_server_destroy(telnet_server_t *server) 
{
    if (!server) 
        return;
    
    // 关闭所有客户端连接
    for (int i = 0; i < server->max_clients; i++) 
    {
        if (server->clients[i] != NULL) 
        {
            close(server->clients[i]->sockfd);
            free(server->clients[i]);
            server->clients[i] = NULL;
        }
    }
    
    // 关闭监听socket
    if (server->listen_sockfd >= 0) 
    {
        close(server->listen_sockfd);
    }
    
    free(server);
}


// 发送欢迎消息
void telnets_welcome(int sockfd) 
{
    const char *welcome = 
        "\r\n"
        "========================================\r\n"
        "   Welcome to WK Telnet Server\r\n"
        "========================================\r\n"
        "\r\n"
        "Available commands:\r\n"
        "  help     - Show this help message\r\n"
        "  time     - Show current time\r\n"
        "  echo <msg> - Echo back the message\r\n"
        "  clear    - Clear the screen\r\n"
        "  quit     - Disconnect\r\n"
        "\r\n";
    
    send(sockfd, welcome, strlen(welcome), 0);
}

// 发送提示符
void telnets_send_prompt(int sockfd) 
{
    const char *prompt = "\rwktx:##>";
    send(sockfd, prompt, strlen(prompt), 0);
}



// 工具函数

// 查找客户端索引
int telnets_find_client_index(telnet_server_t *server, int sockfd) {
    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i] != NULL && server->clients[i]->sockfd == sockfd) {
            return i;
        }
    }
    return -1;
}

// 查找可用槽位
int telnets_find_available_slot(telnet_server_t *server) 
{
    for (int i = 0; i < server->max_clients; i++) 
    {
        if (server->clients[i] == NULL) {
            return i;
        }
    }
    return -1;
}

// 获取当前时间
time_t get_current_time(void) 
{
    return time(NULL);
}

// 检查客户端是否超时
int is_telnet_client_timeout(telnet_client_t *client) 
{
    if (!client) return 1;
    
    time_t current_time = get_current_time();
    time_t idle_time = current_time - client->last_active;
    
    return (idle_time >= TELNET_IDLE_TIMEOUT);
}

// 去除换行符
void telnets_trim_newline(char *str) {
    int len = strlen(str);
    if (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
    }
    if (len > 1 && (str[len-2] == '\r')) {
        str[len-2] = '\0';
    }
}