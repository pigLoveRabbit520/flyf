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
#include <regex.h>
#include <arpa/inet.h>
#include "encode.h"
#include "common.h"

#define FTP_SERVER_PORT            21 
#define BUFFER_SIZE                1024
#define FILE_READ_BUFFER_SIZE      200
#define CMD_READ_BUFFER_SIZE       30
#define FILE_NAME_MAX_SIZE         512
#define ERR_DISCONNECTED          -503
#define ERR_CREATE_BINDED_SOCKTED -523
#define ERR_CONNECT_SERVER_FAIL   -500
#define ERR_READ_FAILED           -454
#define ERR_INCORRECT_CODE        -465


char *fgets_wrapper(char *buffer, size_t buflen, FILE *fp);
int send_cmd(int client_socket, char* buffer);
int get_response(int client_socket, char* buffer);
bool start_with(const char *pre, const char *str);
bool respond_with_code(const char *respond, int code); // 是否返回正确响应码
bool respond_exists_code(const char *respond, int code); // 接收了多行返回是否返回正确响应码
unsigned int cal_data_port(const char *recv_buffer);
bool is_connected(int socket_fd);
int user_login(int client_cmd_socket, char *recv_buffer, char *send_buffer);
bool check_server_ip(const char *server_ip);
int connect_server(int socket, const char *server_ip, unsigned int server_port);
int enter_passvie_mode(int client_cmd_socket, int cmd_port, char *recv_buffer, char *send_buffer);
int get_server_connected_socket(char *server_ip, unsigned int client_port, unsigned int server_port);
void set_flag(int, int);
void clr_flag(int, int);
bool is_server_disconnected(int client_socket); // server端是否断开连接
void print_help();
void ls();
void lls();


extern int errno;

unsigned int client_cmd_port = 0;
unsigned short login_time = 0;
char *server_ip = NULL;
bool server_connected = false;
unsigned int client_cmd_socket;
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

int send_cmd(int client_socket, char* buffer)
{
    int len = send(client_socket, buffer, strlen(buffer), 0);
    bzero(buffer, BUFFER_SIZE);
    return len;
}

bool match_regex(const char* pattern, const char* str)
{
    bool result = false;
    regex_t regex;
    regcomp(&regex, pattern, REG_EXTENDED);
    int reti = regexec(&regex, str, 0, NULL, 0);
    if (reti == 0) result = true;
    regfree( &regex );
    return result;
}

bool is_multi_response(char *str)
{
    return match_regex("^[0-9]{3}-", str);
}

bool is_multi_response_end(const char *buffer, const char *code)
{
    char pattern[10];
    sprintf(pattern, "%s End\r\n", code);
    return match_regex(pattern, buffer);
}

// win上会多行错误
// 例如550-The system cannot find the file specified. 它用-表示下面还有内容
// recv有时候会一次性接受完数据，有时候需要多次
int get_response(int client_socket, char* buffer)
{
    bzero(buffer, BUFFER_SIZE);
    int length = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (length <= 0) return length;
    if (is_multi_response(buffer))
    {
        char code[4];
        bzero(code, 4);
        memcpy(code, buffer, 3);
        while(!is_multi_response_end(buffer, code))
        {
            char anotherBuff[BUFFER_SIZE];
            int len = recv(client_socket, anotherBuff, BUFFER_SIZE, 0);
            if (len <= 0) return len;
            memcpy(buffer + length, anotherBuff, len);
            length += len;
        }
    }
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

bool respond_with_code(const char *response, int code)
{
    char code_str[4];
    sprintf(code_str, "%d", code);
    return start_with(response, code_str);
}

bool respond_exists_code(const char *response, int code)
{
    char pattern[6];
    sprintf(pattern, "\r\n%d", code);
    match_regex(pattern, response);
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

bool check_server_ip(const char *ip_addr)
{
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    return inet_pton(AF_INET, ip_addr, &(addr.sin_addr)) != 0;
}

int get_binded_socket(unsigned int local_port)
{
    struct sockaddr_in client_addr;
    bzero(&client_addr, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htons(INADDR_ANY);
    client_addr.sin_port = local_port;
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket < 0)
    {
        perror("Create socket failed!");
    }
    // 端口复用
    int opt = 1;
    if (setsockopt(client_socket, SOL_SOCKET,SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("Setsockopt failed");
        return -1; 
    }
    if(bind(client_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)))
    {
        perror("Client socket bind port failed!\n");
        return -1;
    }
    return client_socket;
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

// 进入被动模式
int enter_passvie_mode(int client_cmd_socket, int cmd_port, char *recv_buffer, char *send_buffer)
{
    // 被动模式
    int client_data_socket = get_binded_socket(cmd_port + 1);
    if (client_data_socket < 0)
    {
        return ERR_CREATE_BINDED_SOCKTED;
    }

    sprintf(send_buffer, "PASV\r\n");
    if (send_cmd(client_cmd_socket, send_buffer) <= 0) {
        close(client_data_socket);
        printf("send [PASV] command failed\n");
        return ERR_DISCONNECTED;
    }
     // 227
    if (get_response(client_cmd_socket, recv_buffer) <= 0)
    {
        close(client_data_socket);
        printf("Recieve [PASV] command info from server %s failed!\n", server_ip);
        return ERR_DISCONNECTED;
    }
    if (!respond_with_code(recv_buffer, 227))
    {
        close(client_data_socket);
        printf("%s\n", recv_buffer);
        return ERR_INCORRECT_CODE;
    }
    // 计算数据端口
    unsigned int server_data_port = cal_data_port(recv_buffer);
    if (connect_server(client_data_socket, server_ip, server_data_port) < 0)
    {
        close(client_data_socket);
        return ERR_CONNECT_SERVER_FAIL;
    }
    
    return client_data_socket;
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
    if (login_time == 0)
    {
        // 接受欢迎命令
        if (get_response(client_cmd_socket, recv_buffer) <= 0)
        {
            printf("Recieve welcome info from server %s failed!\n", server_ip);
            return ERR_DISCONNECTED;
        }
        printf("%s", recv_buffer);
        login_time++;
    }

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
    length = get_response(client_cmd_socket, recv_buffer);
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
    length = get_response(client_cmd_socket, recv_buffer);
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

void print_help()
{
    printf("\nls\n  displays contents of remote current working directory.\n");
    printf("\nlls\n  displays contents of local current working directory.\n");
    printf("\npwd\n  displays path of remote current working directory.\n");
    printf("\nlpwd\n  displays path of local current working directory.\n");
    printf("\ncd <path>\n  changes the remote current working directory to the specified <path>.\n");
    printf("\nlcd <path>\n  changes the local current working directory to the specified <path>.\n");
    printf("\nget <src> [dest]\n  downloads the remote <src> to local current working directory by the name of [dest] if specified.\n");
    printf("\nput <src> [dest]\n  uploads the local <src> file to remote current working directory by the name of [dest] if specified.\n");
    printf("\nhelp\n  displays this message.\n");
    printf("\nexit\n  terminates this program.\n");
}

void ls(unsigned int client_cmd_port, int client_cmd_socket)
{
    int client_data_socket = enter_passvie_mode(client_cmd_socket, client_cmd_port + 1, send_buffer, recv_buffer);
    if (client_data_socket == ERR_DISCONNECTED)
    {
        close(client_cmd_socket);
        server_connected = false;
        return;
    }
    else if (client_data_socket == ERR_CREATE_BINDED_SOCKTED || client_data_socket == ERR_CONNECT_SERVER_FAIL || client_data_socket == ERR_INCORRECT_CODE)
    {
        return;
    }
    sprintf(send_buffer, "LIST \r\n");
    if (send_cmd(client_cmd_socket, send_buffer) <= 0)
    {
        close(client_data_socket);
        close(client_cmd_socket);
        printf("send [LIST] command failed\n");
        return;
    }

    // 125打开数据连接，开始传输 226表明完成
    // 150打开连接
    // linux vsftpd 发送150
    // windows FTP Sever 是125
    if (get_response(client_cmd_socket, recv_buffer) <= 0)
    {
        close(client_data_socket);
        close(client_cmd_socket);
        printf("Recieve [LIST] command info from server %s failed!\n", server_ip);
        return;
    }
    if (!respond_with_code(recv_buffer, 125) && !respond_with_code(recv_buffer, 150))
    {
        close(client_data_socket);
        printf("%s", recv_buffer);
        return;
    }
    // 注意发送了LIST命令后，可能数据端口发送很快
    // 测试发现这里接收数据时，可能会已经收到了226 Transfer complete.了
    // 非阻塞
    set_flag(client_data_socket, O_NONBLOCK);

    pid_t pid;
    if ((pid = fork()) < 0) {
        printf("fork error");
        return;
    } else if (pid == 0) {
        char data_buffer[BUFFER_SIZE];
        char *ptr = "";
        int data_len = 0;
        int pre_len = 0;
        for (;;)
        {
            bzero(data_buffer, BUFFER_SIZE);
            int length = recv(client_data_socket, data_buffer, BUFFER_SIZE, 0);
            if (length == 0)
            {
                close(client_data_socket);
                break;
            }
            else if (length < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                {
                    continue;
                }
                close(client_data_socket);
                printf("get data failed\n");
                exit(1);
            }
            pre_len = data_len;
            data_len += length;
            char *tmp_ptr = (char *)calloc(data_len, sizeof(char));
            memcpy(tmp_ptr, ptr, pre_len);
            memcpy(tmp_ptr + pre_len, data_buffer, length);
            if (pre_len > 0) free(ptr);
            ptr = tmp_ptr;
        }
        char *tmp_ptr = (char *)calloc(data_len * 2, sizeof(char));
        g2u(ptr, data_len, tmp_ptr, data_len * 2);
        printf("%s", tmp_ptr);
        // 要接收目录列表是否为空
        if (data_len > 0)
        {
            free(ptr);
            free(tmp_ptr);
        }
        exit(0);
    } else {
        // 可能会已经收到了226 Transfer complete.了
        if (!respond_exists_code(recv_buffer, 226))
        {
            if (get_response(client_cmd_socket, recv_buffer) <= 0)
            {
                printf("Recieve data from server %s failed!\n", server_ip);
                return;
            }
            if (!respond_with_code(recv_buffer, 226))
            {
                printf("LIST end failed\n");
                return;
            }
        }
        int status = 0;
        waitpid(pid, &status, 0);
    }
}

void lls()
{
    pid_t pid;
    if ((pid = fork()) < 0) {
        printf("fork error");
        return;
    } else if (pid == 0) {
        if (execl("/bin/ls", "ls", "-l", "-a", NULL) < 0) {  
            perror("error on exec");  
            exit(0);
        }
    } else {
        wait(NULL);
    }
}

void get(struct command* cmd, int client_cmd_socket, unsigned int client_cmd_port)
{
    if (!cmd->paths)
    {
        printf("please input the file\n");
        return;
    }
    char *filename = cmd->paths[0];
    int client_data_socket = enter_passvie_mode(client_cmd_socket, client_cmd_port + 1, send_buffer, recv_buffer);
    if (client_data_socket == ERR_DISCONNECTED)
    {
        close(client_cmd_socket);
        server_connected = false;
        return;
    }
    else if (client_data_socket == ERR_CREATE_BINDED_SOCKTED || client_data_socket == ERR_CONNECT_SERVER_FAIL || client_data_socket == ERR_INCORRECT_CODE)
    {
        return;
    } 
    sprintf(send_buffer, "SIZE %s\r\n", filename);
    if (send_cmd(client_cmd_socket, send_buffer) <= 0)
    {
        close(client_data_socket);
        close(client_cmd_socket);
        printf("send [SIZE] command failed\n");
        return;
    }
    if (get_response(client_cmd_socket, recv_buffer) <= 0)
    {
        close(client_data_socket);
        close(client_cmd_socket);
        printf("Recieve [SIZE] command info from server %s failed!\n", server_ip);
        return;
    }
    if (!respond_with_code(recv_buffer, 213))
    {
        close(client_data_socket);
        printf("%s\n", recv_buffer);
        return;
    }
    char *filesizeRes = (char *)calloc(strlen(recv_buffer) - 6, sizeof(char));
    strncpy(filesizeRes, recv_buffer + 4, strlen(recv_buffer) - 6);
    // eg:213 228\r\n
    printf("file size is %sB\n", filesizeRes);
    free(filesizeRes);
    sprintf(send_buffer, "RETR %s\r\n", filename);
    if (send_cmd(client_cmd_socket, send_buffer) <= 0)
    {
        close(client_data_socket);
        close(client_cmd_socket);
        printf("send [RETR] command failed\n");
        return;
    }
    if (get_response(client_cmd_socket, recv_buffer) <= 0)
    {
        close(client_data_socket);
        close(client_cmd_socket);
        printf("Recieve [RETR] command info from server %s failed!\n", server_ip);
        return;
    }
    if (!respond_with_code(recv_buffer, 125) && !respond_with_code(recv_buffer, 150))
    {
        close(client_data_socket);
        printf("%s", recv_buffer);
        return;
    }

    // 非阻塞
    set_flag(client_data_socket, O_NONBLOCK);
    pid_t pid;
    if ((pid = fork()) < 0) {
        printf("fork error");
        return;
    } else if (pid == 0) {
        FILE *fp;
        if ((fp = fopen(filename, "wb")) == NULL)
        {
            close(client_data_socket);
            printf("open file failed\n");
            exit(1);
        }
        size_t char_size = sizeof(char);
        char data_buffer[BUFFER_SIZE];
        for (;;)
        {
            set0(data_buffer, BUFFER_SIZE);
            int length = recv(client_data_socket, data_buffer, BUFFER_SIZE, 0);
            if (length == 0)
            {
                close(client_data_socket);
                break;
            }
            else if (length < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                {
                    continue;
                }
                close(client_data_socket);
                printf("get data failed\n");
                exit(1);
            }
            fwrite(data_buffer, char_size, length, fp);
        }
        fclose(fp);
        exit(0);
    } else {
        if (get_response(client_cmd_socket, recv_buffer) <= 0)
        {
            printf("[GET] command recieve data from server %s failed!\n", server_ip);
            return;
        }
        if (!respond_with_code(recv_buffer, 226))
        {
            printf("GET end failed\n");
            return;
        }
        printf("%s", recv_buffer);
        int status = 0;
        waitpid(pid, &status, 0);
    }
}

void put(struct command* cmd, int client_cmd_socket, unsigned int client_cmd_port)
{
    if (!cmd->paths)
    {
        printf("please input the filename\n");
        return;
    }
    char *filename = cmd->paths[0];
    int client_data_socket = enter_passvie_mode(client_cmd_socket, client_cmd_port + 1, send_buffer, recv_buffer);
    if (client_data_socket == ERR_DISCONNECTED)
    {
        close(client_cmd_socket);
        server_connected = false;
        return;
    }
    else if (client_data_socket == ERR_CREATE_BINDED_SOCKTED || client_data_socket == ERR_CONNECT_SERVER_FAIL || client_data_socket == ERR_INCORRECT_CODE)
    {
        return;
    }
    sprintf(send_buffer, "STOR %s\r\n", filename);
    if (send_cmd(client_cmd_socket, send_buffer) <= 0)
    {
        close(client_data_socket);
        close(client_cmd_socket);
        printf("send [STOR] command failed\n");
        return;
    }
    if (get_response(client_cmd_socket, recv_buffer) <= 0)
    {
        close(client_data_socket);
        close(client_cmd_socket);
        printf("Recieve [STOR] command info from server %s failed!\n", server_ip);
        return;
    }
    if (!respond_with_code(recv_buffer, 125) && !respond_with_code(recv_buffer, 150))
    {
        close(client_data_socket);
        printf("%s", recv_buffer);
        return;
    }

    // 非阻塞
    set_flag(client_data_socket, O_NONBLOCK);
    pid_t pid;
    if ((pid = fork()) < 0) {
        printf("fork error");
        return;
    } else if (pid == 0) {
        FILE *fp;
        if ((fp = fopen(filename, "rb")) == NULL)
        {
            close(client_data_socket);
            printf("open file failed\n");
            exit(1);
        }
        size_t char_size = sizeof(char);
        char data_buffer[FILE_READ_BUFFER_SIZE];
        int numread;
        for (;;)
        {
            bzero(data_buffer, FILE_READ_BUFFER_SIZE);
            numread = fread(data_buffer, char_size, FILE_READ_BUFFER_SIZE, fp);
            if (numread < 0)
            {
                printf("read file failed\n");
                break;
            } 
            else if (numread > 0)
            {
                int length = send(client_data_socket, data_buffer, numread, 0);
                if (length == 0)
                {
                    break;
                }
                else if (length < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    {
                        continue;
                    }
                    printf("[PUT] command send data failed\n");
                    exit(1);
                }
            }
            if (numread == FILE_READ_BUFFER_SIZE) continue;
            else {
                break;
            }
        }
        // 只有当某个sockfd的引用计数为0，close 才会发送FIN段，否则只是将引用计数减1而已
        shutdown(client_data_socket, SHUT_WR);
        fclose(fp);
        exit(0);
    } else {
        close(client_data_socket);
        int status = 0;
        waitpid(pid, &status, 0);
        if (status != 0)
        {
            printf("send file %s failed.\n", filename);
        }
        if (get_response(client_cmd_socket, recv_buffer) <= 0)
        {
            close(client_cmd_socket);
            printf("Recieve Upload file end info from server %s failed!\n", server_ip);
            return;
        }
        printf("%s", recv_buffer);
    }
}

void cd(struct command* cmd)
{
    if (!cmd->paths)
    {
        printf("please input the path\n");
        return;
    }
    sprintf(send_buffer, "CWD %s\r\n", cmd->paths[0]);
    send_cmd(client_cmd_socket, send_buffer);
    length = get_response(client_cmd_socket, recv_buffer);
    printf("%s", recv_buffer);
}

void lcd(struct command* cmd)
{
    if (!cmd->paths)
    {
        printf("please input the path\n");
        continue;
    }
    char *path = cmd->paths[0];
    if (chdir(path) != 0) perror("change local working directory failed");
    else
    {
        printf("successfully change local working directory\n");
    }
}

void pwd(struct command* cmd)
{
    sprintf(send_buffer, "PWD\r\n");
    send_cmd(client_cmd_socket, send_buffer);
     // 227
    length = get_response(client_cmd_socket, recv_buffer);
    printf("%s", recv_buffer);
}

void lpwd(struct command* cmd)
{
    char buf[80];
    getcwd(buf, sizeof(buf));
    printf("current working directory : %s\n", buf);
}

void ascii()
{
    sprintf(send_buffer, "TYPE A\r\n");
    send_cmd(client_cmd_socket, send_buffer);
     // 227
    length = get_response(client_cmd_socket, recv_buffer);
    printf("%s", recv_buffer);
}

void binary()
{
    sprintf(send_buffer, "TYPE I\r\n");
    send_cmd(client_cmd_socket, send_buffer);
     // 227
    length = get_response(client_cmd_socket, recv_buffer);
    printf("%s", recv_buffer);
}

void delete_cmd(struct command* cmd)
{
    if (!cmd->paths)
    {
        printf("please select the file\n");
        continue;
    }
    sprintf(send_buffer, "DELE %s\r\n", cmd->paths[0]);
    send_cmd(client_cmd_socket, send_buffer);
    length = get_response(client_cmd_socket, recv_buffer);
    printf("%s", recv_buffer);
}

void mkdir(struct command* cmd)
{
    if (!cmd->paths)
    {
        printf("please input the host\n");
        continue;
    }
    char *dir_name = cmd->paths[0];
    sprintf(send_buffer, "MKD %s\r\n", dir_name);
    send_cmd(client_cmd_socket, send_buffer);
    length = get_response(client_cmd_socket, recv_buffer);
    printf("%s", recv_buffer);
}

void open_cmd(struct command* cmd, int client_cmd_socket)
{
    if (!cmd->paths)
    {
        printf("please input the host\n");
        return;
    }
    server_ip = cmd->paths[0];
    client_cmd_port = get_rand_port();
    client_cmd_socket = get_server_connected_socket(server_ip, client_cmd_port, FTP_SERVER_PORT);
    if (client_cmd_socket < 0)
        return;
    server_connected = true;
    int res = user_login(client_cmd_socket, recv_buffer, send_buffer);
    if (res == ERR_DISCONNECTED)
    {
        close(client_cmd_socket);
        return;
    } else if (res == ERR_READ_FAILED || res == ERR_INCORRECT_CODE)
    {
        printf("try login again: please use [open] command\n");
    }
}

void help()
{
    print_help();
}

void exit_cmd()
{
    sprintf(send_buffer, "QUIT\r\n");
    send_cmd(client_cmd_socket, send_buffer);
    printf("Goodbye!\n");
    exit(0);
}

#endif