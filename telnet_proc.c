/**
 * @file telnet_server.c
 * @brief Telnet服务器处理
 * @date liuliang 2026-01-25
 * 
 * 本文件包含Telnet服务器的实现
 */

#include "telnet_server.h"


// 设置 socket 为非阻塞模式的函数
int set_tcp_nonblocking(int sockfd) 
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) 
    {
        perror("fcntl F_GETFL");
        return -1;
    }
    
    flags |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) == -1) 
    {
        perror("fcntl F_SETFL");
        return -1;
    }
    
    return 0;
}


// 处理新客户端连接
void telnets_handle_new_connection(telnet_server_t *server) 
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_sockfd;
    
    // 接受新连接
    new_sockfd = accept(server->listen_sockfd, (struct sockaddr *)&client_addr, &addr_len);
    if (new_sockfd < 0) 
    {
        perror("Accept failed");
        return;
    }

    // 设置客户端 socket 为非阻塞模式
    if (set_tcp_nonblocking(new_sockfd) < 0) 
    {
        perror("Failed to set non-blocking on client socket");
        close(new_sockfd);
        return;
    }
    
    // 查找可用的客户端槽位
    int client_index = telnets_find_available_slot(server);
    if (client_index < 0) 
    {
        printf("Max clients reached. Rejecting connection from %s\n", 
               inet_ntoa(client_addr.sin_addr));
        close(new_sockfd);
        return;
    }
    
    // 添加新客户端
    if (telnets_add_client(server, new_sockfd, &client_addr) < 0) 
    {
        close(new_sockfd);
        return;
    }
    
    printf("New client connected: %s:%d (slot %d)\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port),
           client_index);
    
    // 发送欢迎消息
    telnets_welcome(new_sockfd);
    telnets_send_prompt(new_sockfd);
}


// 添加新客户端
int telnets_add_client(telnet_server_t *server, int sockfd, struct sockaddr_in *addr) 
{
    int index = telnets_find_available_slot(server);
    if (index < 0) {
        return -1;
    }
    
    // 分配客户端结构
    telnet_client_t *client = (telnet_client_t *)malloc(sizeof(telnet_client_t));
    if (!client) {
        perror("Failed to allocate client memory");
        return -1;
    }
    
    // 初始化客户端结构
    memset(client, 0, sizeof(telnet_client_t));
    client->sockfd = sockfd;
    memcpy(&client->addr, addr, sizeof(struct sockaddr_in));
    client->last_active = get_current_time();
    client->authenticated = 0;
    client->telnet_state = 0;
    memset(client->username, 0, sizeof(client->username));
    memset(client->buffer, 0, sizeof(client->buffer));
    client->buffer_len = 0;
    
    // 保存到服务器
    server->clients[index] = client;
    
    // 更新最大描述符
    if (sockfd > server->max_fd) {
        server->max_fd = sockfd;
    }
    
    return 0;
}


// 移除客户端
void telnets_remove_client(telnet_server_t *server, int client_index) 
{
    if (client_index < 0 || client_index >= server->max_clients) 
    {
        return;
    }
    
    telnet_client_t *client = server->clients[client_index];
    if (!client) {
        return;
    }
    
    printf("Client disconnected: %s:%d (slot %d)\n",
           inet_ntoa(client->addr.sin_addr),
           ntohs(client->addr.sin_port),
           client_index);
    
    // 关闭socket
    close(client->sockfd);
    
    // 释放内存
    free(client);
    server->clients[client_index] = NULL;
}


// 清理超时客户端
void telnets_cleanup_clients(telnet_server_t *server) 
{
    for (int i = 0; i < server->max_clients; i++) 
    {
        if (server->clients[i] != NULL) {
            if (is_telnet_client_timeout(server->clients[i])) 
            {
                printf("Client %s:%d timed out (slot %d)\n",
                       inet_ntoa(server->clients[i]->addr.sin_addr),
                       ntohs(server->clients[i]->addr.sin_port),
                       i);
                
                // 发送超时消息
                const char *timeout_msg = "\r\nConnection timed out due to inactivity.\r\n";
                send(server->clients[i]->sockfd, timeout_msg, strlen(timeout_msg), 0);
                
                // 移除客户端
                telnets_remove_client(server, i);
            }
        }
    }
}


