/**
 * @file telnet_server.c
 * @brief Telnet服务器
 * @date liuliang 2026-01-25
 * 
 * 本文件包含Telnet服务器的实现
 */

#include "telnet_server.h"



// 处理客户端命令
void telnets_command_proc(telnet_client_t *client, const char *command) 
{
    char cmd[128];
    char arg[TELNET_BUFFER_SIZE];
    
    // 解析命令和参数
    int parsed = sscanf(command, "%s %[^\n]", cmd, arg);
    
    if (parsed == 0) {
        return;
    }
    
    // 转换为小写以便比较
    for (int i = 0; cmd[i]; i++) 
    {
        cmd[i] = tolower(cmd[i]);
    }
    
    // 处理命令
    if (strcmp(cmd, "help") == 0) 
    {
        const char *help_msg = 
            "\r\nAvailable commands:\r\n"
            "  help     - Show this help message\r\n"
            "  time     - Show current time\r\n"
            "  echo <msg> - Echo back the message\r\n"
            "  clear    - Clear the screen\r\n"
            "  quit     - Disconnect\r\n"
            "  clients  - Show connected clients\r\n"
            "  stats    - Show server statistics\r\n";
        send(client->sockfd, help_msg, strlen(help_msg), 0);
    }
    else if (strcmp(cmd, "time") == 0) 
    {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        char response[128];
        snprintf(response, sizeof(response), "\r\nCurrent time: %s\r\n", time_str);
        send(client->sockfd, response, strlen(response), 0);
    }
    else if (strcmp(cmd, "echo") == 0) 
    {
        if (strlen(arg) > 0) {
            char response[TELNET_BUFFER_SIZE + 64];
            snprintf(response, sizeof(response), "\r\nEcho: %s\r\n", arg);
            send(client->sockfd, response, strlen(response), 0);
        } else {
            const char *error_msg = "\r\nUsage: echo <message>\r\n";
            send(client->sockfd, error_msg, strlen(error_msg), 0);
        }
    }
    else if (strcmp(cmd, "clear") == 0) {
        // 发送ANSI清屏序列
        const char *clear_screen = "\033[2J\033[H";
        send(client->sockfd, clear_screen, strlen(clear_screen), 0);
    }
    else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
        const char *bye_msg = "\r\nGoodbye!\r\n";
        send(client->sockfd, bye_msg, strlen(bye_msg), 0);

        // 客户端将在下次循环中被移除
        //telnets_remove_client(telnet_server_t *server, int client_index) 
        client->closed = 1;

    }
    else if (strcmp(cmd, "clients") == 0) {
        // 这里可以添加显示连接客户端的功能
        const char *msg = "\r\nClient list functionality not implemented yet.\r\n";
        send(client->sockfd, msg, strlen(msg), 0);
    }
    else if (strcmp(cmd, "stats") == 0) {
        time_t uptime = get_current_time() - client->last_active;
        char response[256];
        snprintf(response, sizeof(response), 
                "\r\nClient statistics:\r\n"
                "  IP: %s\r\n"
                "  Port: %d\r\n"
                "  Connected for: %ld seconds\r\n",
                inet_ntoa(client->addr.sin_addr),
                ntohs(client->addr.sin_port),
                uptime);
        send(client->sockfd, response, strlen(response), 0);
    }
    else {
        char response[256];
        snprintf(response, sizeof(response), "\r\nUnknown command: %s\r\n", cmd);
        send(client->sockfd, response, strlen(response), 0);
        
        const char *help_hint = "Type 'help' for available commands.\r\n";
        send(client->sockfd, help_hint, strlen(help_hint), 0);
    }
}



// 处理Telnet命令
void telnets_handle_commands(telnet_client_t *client, const char *data, int len) 
{
    for (int i = 0; i < len; i++) 
    {
        unsigned char c = data[i];
        
        switch (client->telnet_state) 
        {
            case 0:  // 正常状态
                if (c == TELNET_IAC) 
                {
                    client->telnet_state = 1;  // 进入命令状态
                }
                break;
                
            case 1:  // 接收到IAC
                if (c == TELNET_WILL || c == TELNET_WONT || 
                    c == TELNET_DO || c == TELNET_DONT) 
                {
                    client->telnet_state = 2;  // 需要读取选项
                } 
                else if (c == TELNET_SB) 
                {
                    client->telnet_state = 3;  // 子协商开始
                } 
                else if (c == TELNET_IAC) 
                {
                    // 双IAC，表示数据0xFF
                    client->telnet_state = 0;
                } 
                else 
                {
                    client->telnet_state = 0;  // 其他命令，返回正常状态
                }
                break;
                
            case 2:  // 读取选项
                // 这里可以添加选项处理逻辑
                client->telnet_state = 0;
                break;
                
            case 3:  // 子协商
                if (c == TELNET_IAC) 
                {
                    client->telnet_state = 4;  // 可能子协商结束
                }
                break;
                
            case 4:  // 子协商中的IAC
                if (c == TELNET_SE) 
                {
                    client->telnet_state = 0;  // 子协商结束
                } 
                else if (c == TELNET_IAC)
                {
                    client->telnet_state = 3;  // 双IAC，继续子协商
                } 
                else 
                {
                    client->telnet_state = 3;  // 其他情况
                }
                break;
        }
    }
}



// 处理客户端数据
void telnets_recv_data_proc(telnet_server_t *server, int client_index) 
{
    telnet_client_t *client = server->clients[client_index];
    if (!client) 
    {
        return;
    }
    
    char buffer[TELNET_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    // 接收数据
    int bytes_received = recv(client->sockfd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) 
    {
        // 连接关闭或错误
        if (bytes_received == 0) 
        {
            printf("Client %s:%d disconnected (slot %d)\n",
                   inet_ntoa(client->addr.sin_addr),
                   ntohs(client->addr.sin_port),
                   client_index);
        } 
        else 
        {
            perror("Recv error");
        }
        
        telnets_remove_client(server, client_index);
        return;
    }
    
    // 更新最后活动时间
    client->last_active = get_current_time();
    
    // 处理Telnet命令
    telnets_handle_commands(client, buffer, bytes_received);
    
    // 处理实际数据
    for (int i = 0; i < bytes_received; i++) 
    {
        // 忽略Telnet命令序列
        if (client->telnet_state != 0) 
        {
            continue;
        }
        
        char c = buffer[i];
        
        // 处理回退键
        if (c == 127 || c == 8) 
        {  
            // Backspace or Delete
            if (client->buffer_len > 0) 
            {
                client->buffer_len--;

                // 发送退格序列
                const char *backspace = "\b \b";
                send(client->sockfd, backspace, strlen(backspace), 0);
            }
            continue;
        }
        
        // 处理回车换行
        if (c == '\r' || c == '\n') 
        {
            if (client->buffer_len > 0) 
            {
                client->buffer[client->buffer_len] = '\0';
                
                // 回显命令
                send(client->sockfd, "\r\n", 2, 0);
                
                // 处理命令
                telnets_command_proc(client, client->buffer);
                
                // 重置缓冲区
                memset(client->buffer, 0, sizeof(client->buffer));
                client->buffer_len = 0;
            } 
            else 
            {
                // 空行，只发送新提示符
                telnets_send_prompt(client->sockfd);
            }

            if(client->closed) {
                telnets_remove_client(server, client_index);
                return;
            }
            continue;
        }
        
        // 处理普通字符
        if (client->buffer_len < (int)sizeof(client->buffer) - 1 && isprint(c)) 
        {
            client->buffer[client->buffer_len++] = c;
            // 回显字符
            send(client->sockfd, &c, 1, 0);
        }
    }
}


