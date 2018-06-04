#include <netinet/in.h>    // for sockaddr_in
#include <sys/types.h>    // for socket
#include <sys/socket.h>    // for socket
#include <stdio.h>        // for printf
#include <stdlib.h>        // for exit
#include <string.h>        // for bzero
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <pwd.h>


#define FTP_SERVER_PORT 21 
#define BUFFER_SIZE 1024
#define CMD_READ_BUFFER_SIZE 30
#define FILE_NAME_MAX_SIZE 512

int client_cmd_port = 0;
ushort get_rand_port();
void send_cmd(int client_socket, char* buffer);
bool start_with(const char *pre, const char *str);
char *fgets_wrapper(char *buffer, size_t buflen, FILE *fp);
bool is_correct_respond(const char *respond, int code); // 是否返回正确响应码

// 被动模式
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: ./%s ServerIPAddress\n", argv[0]);
        exit(1);
    }
 
    // 设置一个socket地址结构client_addr,代表客户机internet地址, 端口
    struct sockaddr_in client_addr;
    bzero(&client_addr, sizeof(client_addr)); // 把一段内存区的内容全部设置为0
    client_addr.sin_family = AF_INET;    // internet协议族
    client_addr.sin_addr.s_addr = htons(INADDR_ANY);// INADDR_ANY表示自动获取本机地址
    client_cmd_port = get_rand_port();
    client_addr.sin_port = htons(client_cmd_port);    // 0表示让系统自动分配一个空闲端口
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket < 0)
    {
        printf("create client socket failed!\n");
        exit(1);
    }
    //把客户机的socket和客户机的socket地址结构联系起来
    if(bind(client_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)))
    {
        printf("client bind port failed!\n"); 
        exit(1);
    }
 
    // 设置一个socket地址结构server_addr,代表服务器的internet地址, 端口
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if(inet_aton(argv[1], &server_addr.sin_addr) == 0) //服务器的IP地址来自程序的参数
    {
        printf("server IP address error!\n");
        exit(1);
    }
    server_addr.sin_port = htons(FTP_SERVER_PORT);
    socklen_t server_addr_length = sizeof(server_addr);
    // 向服务器发起连接,连接成功后client_socket代表了客户机和服务器的一个socket连接
    if(connect(client_socket, (struct sockaddr*)&server_addr, server_addr_length) < 0)
    {
        printf("can not connect To %s!\n", argv[1]);
        exit(1);
    }
    char recv_buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];
    bzero(recv_buffer, BUFFER_SIZE);
    bzero(send_buffer, BUFFER_SIZE);
    int length = 0;
    // 接受欢迎命令
    length = get_respond(client_socket, recv_buffer, argv[1]);
	printf("%s", recv_buffer);

    char cmd_read[CMD_READ_BUFFER_SIZE];
    struct passwd *pws;
    pws = getpwuid(geteuid());
    // 输入用户名
    printf("Name(%s:%s):", argv[1], pws->pw_name);
    if (fgets_wrapper(cmd_read, CMD_READ_BUFFER_SIZE, stdin) == 0) 
    {
        printf("read name failed\n");
        exit(1);
    }

    sprintf(send_buffer, "USER %s\r\n", cmd_read);
    send_cmd(client_socket, send_buffer);

    // 331
    length = get_respond(client_socket, recv_buffer, argv[1]);
    printf("%s", recv_buffer);
    if (!is_correct_respond(recv_buffer, 331))
    {
        exit(1);
    }

    bzero(cmd_read, CMD_READ_BUFFER_SIZE);
    // 输入密码
    printf("Password:");
    if (fgets_wrapper(cmd_read, CMD_READ_BUFFER_SIZE, stdin) == 0) 
    {
        printf("read password failed\n");
        exit(1);
    }
    sprintf(send_buffer,"PASS %s\r\n", cmd_read);
    send_cmd(client_socket, send_buffer);

    // 230
    length = get_respond(client_socket, recv_buffer, argv[1]);
    printf("%s", recv_buffer);
    if (!is_correct_respond(recv_buffer, 230))
    {
        exit(1);
    }
    // 被动模式
    struct sockaddr_in client_data_addr;
    bzero(&client_data_addr, sizeof(client_data_addr)); // 把一段内存区的内容全部设置为0
    client_data_addr.sin_family = AF_INET;    // internet协议族
    client_data_addr.sin_addr.s_addr = htons(INADDR_ANY);
    client_data_addr.sin_port = client_cmd_port + 1;
    int client_data_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_data_socket < 0)
    {
        printf("create client data socket failed!\n");
        exit(1);
    }
    if(bind(client_data_socket, (struct sockaddr*)&client_data_addr, sizeof(client_data_addr)))
    {
        printf("client for data bind port failed!\n"); 
        exit(1);
    }

    sprintf(send_buffer, "PASV\r\n");
    send_cmd(client_socket, send_buffer);
     // 227
    length = get_respond(client_socket, recv_buffer, argv[1]);
    printf("%s", recv_buffer);
    if (!is_correct_respond(recv_buffer, 227))
    {
        exit(1);
    }

    for (;;)
    {
        printf("FTP>");
        if (fgets_wrapper(cmd_read, CMD_READ_BUFFER_SIZE, stdin) != 0)
        {
            if (start_with(cmd_read, "ls"))
            {
                sprintf(send_buffer, "LIST %s\r\n", "");
                send_cmd(client_socket, send_buffer);

                // 230
                length = get_respond(client_socket, recv_buffer, argv[1]);
                printf("%s\n", recv_buffer);
            } else if (start_with(cmd_read, "exit"))
            {
                printf("Goodbye!\n");
                exit(0);
            }
        }
    }
 
//     char file_name[FILE_NAME_MAX_SIZE+1];
//     bzero(file_name, FILE_NAME_MAX_SIZE+1);
//     printf("Please Input File Name On Server:\t");
//     scanf("%s", file_name);
     
//     char buffer[BUFFER_SIZE];
//     bzero(buffer,BUFFER_SIZE);
//     strncpy(buffer, file_name, strlen(file_name)>BUFFER_SIZE?BUFFER_SIZE:strlen(file_name));
//     //向服务器发送buffer中的数据
//     send(client_socket, buffer, BUFFER_SIZE, 0);
 
// //    int fp = open(file_name, O_WRONLY|O_CREAT);
// //    if( fp < 0 )
//     FILE * fp = fopen(file_name,"w");
//     if(NULL == fp )
//     {
//         printf("File:\t%s Can Not Open To Write\n", file_name);
//         exit(1);
//     }
     

    //关闭socket
    close(client_socket);
    return 0;
}

// 客户端打开任意的本地端口 (N > 1024) 
ushort get_rand_port()
{
    srand((unsigned) time(NULL));
    int number = rand() % 101 + 1; // 产生1-101的随机数
    return number + 1024;
}

void send_cmd(int client_socket, char* buffer)
{
    send(client_socket, buffer, BUFFER_SIZE, 0);
    bzero(buffer, BUFFER_SIZE);
}

int get_respond(int client_socket, char* buffer, char* server_ip)
{
    bzero(buffer, BUFFER_SIZE);
    int length = 0;
    // 接受欢迎命令
    length = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if(length < 0)
    {
        printf("Recieve Data From Server %s Failed!\n", server_ip);
        exit(1);
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

bool is_correct_respond(const char *respond, int code)
{
    char code_str[4];
    sprintf(code_str, "%d", code);
    return start_with(respond, code_str);
}