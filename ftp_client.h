#ifndef _FTP_CLIENT_H
#define _FTP_CLIENT_H


#include <netinet/in.h>    // for sockaddr_in
#include <sys/types.h>    // for socket
#include <sys/socket.h>    // for socket
#include <stdio.h>        // for printf
#include <stdlib.h>        // for exit
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <arpa/inet.h>
#include "encode.h"
#include "common.h"

#define FTP_SERVER_PORT 21 
#define BUFFER_SIZE 1024
#define CMD_READ_BUFFER_SIZE 30
#define FILE_NAME_MAX_SIZE 512
#define ERR_DISCONNECTED   -503
#define ERR_READ_FAILED    -454
#define ERR_INCORRECT_CODE -465

ushort get_rand_port();
int send_cmd(int client_socket, char* buffer);
int get_respond(int client_socket, char* buffer);
bool start_with(const char *pre, const char *str);
char *fgets_wrapper(char *buffer, size_t buflen, FILE *fp);
bool respond_with_code(const char *respond, int code); // 是否返回正确响应码
unsigned int cal_data_port(const char *recv_buffer);
int get_client_data_socket(unsigned int client_cmd_port);
bool is_connected(int socket_fd);
int user_login(int client_cmd_socket, char *recv_buffer, char *send_buffer);
bool check_server_ip(const char *server_ip);
int connect_server(int socket, const char *server_ip, unsigned int server_port);
int get_server_connected_socket(char *server_ip, unsigned int client_port, unsigned int server_port);
void set_flag(int, int);
void clr_flag(int, int);
bool is_server_disconnected(int client_socket); // server端是否断开连接


extern int errno;

unsigned int client_cmd_port = 0;
char *server_ip = NULL;
bool server_connected = false;
int client_cmd_socket;
char cmd_read[CMD_READ_BUFFER_SIZE];

int set_keepalive(int socket)
{
    int optval = 1;
    if(setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
       perror("setsockopt()");
       close(socket);
       return -1;
    }
    return 0;
}

// 客户端打开任意的本地端口 (N > 1024) 
ushort get_rand_port()
{
    srand((unsigned) time(NULL));
    int number = rand() % 101 + 1; // 产生1-101的随机数
    return number + 1024;
}

int send_cmd(int client_socket, char* buffer)
{
    int len = send(client_socket, buffer, strlen(buffer), 0);
    bzero(buffer, BUFFER_SIZE);
    return len;
}

int get_respond(int client_socket, char* buffer)
{
    bzero(buffer, BUFFER_SIZE);
    int length = recv(client_socket, buffer, BUFFER_SIZE, 0);
    return length;
}

char *fgets_wrapper(char *buffer, size_t buflen, FILE *fp)
{
    if (fgets(buffer, buflen, fp) != 0)
    {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n')
            buffer[len-1] = '\0';
        return buffer;
    }
    return 0;
}

bool start_with(const char *str, const char *pre)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

bool respond_with_code(const char *respond, int code)
{
    char code_str[4];
    sprintf(code_str, "%d", code);
    return start_with(respond, code_str);
}

unsigned int cal_data_port(const char *recv_buffer)
{
    char *pos1 = strchr(recv_buffer, '(');
    char *pos2 = strchr(recv_buffer, ')');
    char passive_res[25];
    strncpy(passive_res, pos1 + 1, pos2 - pos1 - 1);
    char *token;
    const char delim[2] = ",";
    token = strtok(passive_res, delim);
    int token_idx = 0;
    int p1, p2;
    while(token != NULL)
    {
        if (token_idx == 4)
        {
            p1 = atoi(token);
        } else if (token_idx == 5)
        {
            p2 = atoi(token);
            break;
        }
        token = strtok(NULL, delim);
        token_idx++;
    }
    return p1 * 256 + p2;
}

int get_client_data_socket(unsigned int client_cmd_port)
{
    struct sockaddr_in client_addr;
    bzero(&client_addr, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htons(INADDR_ANY);
    client_addr.sin_port = client_cmd_port + 1;
    int client_data_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_data_socket < 0)
    {
        perror("Create client data socket failed!\n");
    }
    // 端口复用
    int opt = 1;
    if (setsockopt(client_data_socket, SOL_SOCKET,SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        return -1; 
    }
    if(bind(client_data_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)))
    {
        perror("Client data socket bind port failed!\n");
        return -1;
    }
    return client_data_socket;
}

bool check_server_ip(const char *ip_addr)
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    return inet_pton(AF_INET, ip_addr, &(addr.sin_addr)) != 0;
}

int connect_server(int socket, const char *server_ip, unsigned int server_port)
{
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_aton(server_ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(server_port);
    // 向服务器发起连接,连接成功后socket代表了客户机和服务器的一个socket连接
    if(connect(socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Can not connect to %s on port %d\n", server_ip, server_port);
        perror("");
        return -1;
    }
    return 0;
}

int get_server_connected_socket(char *server_ip, unsigned int client_port, unsigned int server_port)
{
    // 设置一个socket地址结构client_addr,代表客户机internet地址, 端口
    struct sockaddr_in client_addr;
    bzero(&client_addr, sizeof(client_addr)); // 把一段内存区的内容全部设置为0
    client_addr.sin_family = AF_INET;    // internet协议族
    client_addr.sin_addr.s_addr = htons(INADDR_ANY);// INADDR_ANY表示自动获取本机地址
    client_addr.sin_port = htons(client_port);    // 0表示让系统自动分配一个空闲端口
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket < 0)
    {
        printf("Create client socket failed!\n");
        return -1;
    }
    //把客户机的socket和客户机的socket地址结构联系起来
    if(bind(client_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)))
    {
        printf("Client bind port failed!\n"); 
        return -1;
    }
    return connect_server(client_socket, server_ip, server_port) < 0 ? -1 : client_socket;
}

bool is_connected(int socket_fd)
{
    int error = 0;
    socklen_t len = sizeof (error);
    int retval = getsockopt (socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if (retval != 0) return false;
    if (error != 0) return false;
    return true;
}

int user_login(int client_cmd_socket, char *recv_buffer, char *send_buffer)
{
    int length = 0;
    // 接受欢迎命令
    length = get_respond(client_cmd_socket, recv_buffer);
    if (length < 0)
    {
        printf("Recieve welcome info from server %s failed!\n", server_ip);
        return ERR_DISCONNECTED;
    }
    printf("%s", recv_buffer);

    struct passwd *pws;
    pws = getpwuid(geteuid());
    // 输入用户名
    printf("Name(%s:%s):", server_ip, pws->pw_name);
    if (fgets_wrapper(cmd_read, CMD_READ_BUFFER_SIZE, stdin) == 0) 
    {
        printf("read name failed\n");
        return ERR_READ_FAILED;
    }

    sprintf(send_buffer, "USER %s\r\n", cmd_read);
    if (send_cmd(client_cmd_socket, send_buffer) < 0)
    {
        printf("send [User] command failed\n");
        return ERR_DISCONNECTED;
    }

    // 331
    length = get_respond(client_cmd_socket, recv_buffer);
    if (length < 0)
    {
        printf("Recieve [User] command info from server %s failed!\n", server_ip);
        return ERR_DISCONNECTED;
    }
    printf("%s", recv_buffer);
    if (!respond_with_code(recv_buffer, 331))
    {
        return ERR_INCORRECT_CODE;
    }

    bzero(cmd_read, CMD_READ_BUFFER_SIZE);
    // 输入密码
    char *passwd = getpass("Password:");
    sprintf(send_buffer,"PASS %s\r\n", passwd);
    if (send_cmd(client_cmd_socket, send_buffer) < 0)
    {
        printf("send [PASS] command failed\n");
        return ERR_DISCONNECTED;
    }
    
    // 230
    length = get_respond(client_cmd_socket, recv_buffer);
    if (length < 0)
    {
        printf("Recieve [PASS] command info from server %s failed!\n", server_ip);
        return ERR_DISCONNECTED;
    }
    printf("%s", recv_buffer);
    if (!respond_with_code(recv_buffer, 230))
    {
        return ERR_INCORRECT_CODE;
    }
    return 0;
}

void set_flag(int fd, int flags)
{
    int val;
    val = fcntl(fd, F_GETFL, 0);
    if (val == -1)
        printf("fcntl get flag error");
    val |= flags;
    if (fcntl(fd, F_SETFL, val) < 0)
        printf("fcntl set flag error");
}

void clr_flag(int fd, int flags)
{
    int val;
    val = fcntl(fd, F_GETFL, 0);
    if (val == -1)
        printf("fcntl get flag error");
    val &= ~flags;
    if (fcntl(fd, F_SETFL, val) < 0)
        printf("fcntl clear flag error");
}

bool is_server_disconnected(int client_socket)
{
    // 非阻塞
    set_flag(client_socket, O_NONBLOCK);
    char buffer[10];
    int length = recv(client_socket, buffer, 10, 0);
    clr_flag(client_socket, O_NONBLOCK);
    return length == 0;
}


#endif