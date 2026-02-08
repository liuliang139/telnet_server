#include "telnet_server.h"

// 显示使用帮助
void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -p PORT     Port to listen on (default: 23)\n");
    printf("  -h          Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s -p 2323     # Start server on port 2323\n", program_name);
    printf("  %s             # Start server on default port 23\n", program_name);
}

//./telnet_server -p 8899
//telnet localhost 2323


int main(int argc, char *argv[]) {
    int port = TELNET_DEFAULT_PORT;
    int opt;
    
    // 解析命令行参数
    while ((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    printf("Starting Telnet server on port %d...\n", port);
    printf("Press Ctrl+C to stop the server.\n\n");
    
    // telnet服务器初始化
    telnet_server_t *server = telnet_server_init(port);
    if (!server)
    {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    
    // 启动服务器
    if (telnet_server_start(server) < 0) 
    {
        fprintf(stderr, "Failed to start server\n");
        telnet_server_destroy(server);
        return 1;
    }
    
    // 清理
    telnet_server_stop(server);
    telnet_server_destroy(server);
    
    printf("\nServer stopped.\n");
    return 0;
}