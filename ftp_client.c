#include <netinet/in.h>    // for sockaddr_in
#include <sys/types.h>    // for socket
#include <sys/socket.h>    // for socket
#include <stdio.h>        // for printf
#include <stdlib.h>        // for exit
#include <string.h>        // for bzero
#include <time.h>
#include <stdbool.h>


#define FTP_SERVER_PORT 21 
#define BUFFER_SIZE 1024
#define USER_CMD_BUFFER_SIZE 50
#define FILE_NAME_MAX_SIZE 512

int client_cmd_port = 0;
ushort get_rand_port();
void send_cmd(int client_socket, char* buffer);
bool start_with(const char *pre, const char *str);
char *fgets_wrapper(char *buffer, size_t buflen, FILE *fp);

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
        printf("Create Client Socket Failed!\n");
        exit(1);
    }
    //把客户机的socket和客户机的socket地址结构联系起来
    if(bind(client_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)))
    {
        printf("Client Bind Port Failed!\n"); 
        exit(1);
    }
    printf("Client Bind Port %d Success!\n", client_cmd_port); 
 
    // 设置一个socket地址结构server_addr,代表服务器的internet地址, 端口
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if(inet_aton(argv[1], &server_addr.sin_addr) == 0) //服务器的IP地址来自程序的参数
    {
        printf("Server IP Address Error!\n");
        exit(1);
    }
    server_addr.sin_port = htons(FTP_SERVER_PORT);
    socklen_t server_addr_length = sizeof(server_addr);
    // 向服务器发起连接,连接成功后client_socket代表了客户机和服务器的一个socket连接
    if(connect(client_socket, (struct sockaddr*)&server_addr, server_addr_length) < 0)
    {
        printf("Can Not Connect To %s!\n", argv[1]);
        exit(1);
    }
    char recv_buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];
    bzero(recv_buffer, BUFFER_SIZE);
    bzero(send_buffer, BUFFER_SIZE);
    int length = 0;
    // 接受欢迎命令
    length = get_respond(client_socket, recv_buffer, argv[1]);
	printf("%s\n", recv_buffer);

    sprintf(send_buffer, "USER %s\r\n", "FTP");
    send_cmd(client_socket, send_buffer);

    // 331
    length = get_respond(client_socket, recv_buffer, argv[1]);
    printf("%s\n", recv_buffer);

    sprintf(send_buffer,"PASS %s\r\n", "salamander");
    send_cmd(client_socket, send_buffer);

    // 230
    length = get_respond(client_socket, recv_buffer, argv[1]);
    printf("%s\n", recv_buffer);

    char user_cmd[USER_CMD_BUFFER_SIZE];
    for (;;)
    {
        printf("%s>", "FTP");
        if (fgets_wrapper(user_cmd, USER_CMD_BUFFER_SIZE, stdin) != 0)
        {
            if (start_with(user_cmd, "ls") == true)
            {
                sprintf(send_buffer, "LIST %s\r\n", "");
                send_cmd(client_socket, send_buffer);

                // 230
                length = get_respond(client_socket, recv_buffer, argv[1]);
                printf("%s\n", recv_buffer);
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

bool start_with(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}