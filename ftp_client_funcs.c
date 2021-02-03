#include "ftp_client_funcs.h"


char recv_buffer[BUFFER_SIZE];

static char send_buffer[BUFFER_SIZE];
static int client_cmd_socket = -1;
static unsigned int client_cmd_port = 0;
static const char *server_ip = NULL;

void empty_buffer()
{
    set0(recv_buffer, BUFFER_SIZE);
    set0(send_buffer, BUFFER_SIZE);
}

void close_cmd_socket()
{
    close(client_cmd_socket);
}

const char* get_server_ip()
{
    return server_ip;
}

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

int send_cmd(const char* format, ...)
{
    va_list argp;
    va_start(argp, format);
    vsprintf(send_buffer, format, argp);
    va_end(argp);
    int len = send(client_cmd_socket, send_buffer, strlen(send_buffer), 0);
    bzero(send_buffer, BUFFER_SIZE);
    if (len <= 0)
    {
        close(client_cmd_socket);
        return -1;
    }
    return len;
}

static bool match_regex(const char* pattern, const char* str)
{
    bool result = false;
    regex_t regex;
    regcomp(&regex, pattern, REG_EXTENDED);
    int reti = regexec(&regex, str, 0, NULL, 0);
    if (reti == 0) result = true;
    regfree( &regex );
    return result;
}

static bool is_multi_response(char *str)
{
    return match_regex("^[0-9]{3}-", str);
}

static bool is_multi_response_end(const char *buffer, const char *code)
{
    char pattern[10];
    sprintf(pattern, "%s End\r\n", code);
    return match_regex(pattern, buffer);
}

// 多行错误
// 例如550-The system cannot find the file specified. 它用-表示下面还有内容
int get_response()
{
    bzero(recv_buffer, BUFFER_SIZE);
    ssize_t length = recv(client_cmd_socket, recv_buffer, BUFFER_SIZE, 0);
    if (length <= 0) return length;
    if (is_multi_response(recv_buffer))
    {
        char code[4];
        bzero(code, 4);
        memcpy(code, recv_buffer, 3);
        while(!is_multi_response_end(recv_buffer, code))
        {
            char anotherBuff[BUFFER_SIZE];
            ssize_t len = recv(client_cmd_socket, anotherBuff, BUFFER_SIZE, 0);
            if (len <= 0) return len;
            memcpy(recv_buffer + length, anotherBuff, len);
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
int enter_passvie_mode()
{
    // 被动模式
    int client_data_socket = get_binded_socket(client_cmd_port + 1);
    if (client_data_socket < 0)
    {
        return ERR_CREATE_BINDED_SOCKTED;
    }

    if (send_cmd("PASV\r\n") <= 0) {
        close(client_data_socket);
        printf("send [PASV] command failed\n");
        return ERR_DISCONNECTED;
    }
     // 227
    if (get_response() <= 0)
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

int get_server_connected_socket(const char *ip, unsigned int client_port, unsigned int server_port)
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
        printf("client bind port failed!\n");
        return -1;
    }
    if (connect_server(client_socket, ip, server_port) < 0)
    {
        return -1;
    } else {
        server_ip = ip;
        client_cmd_port = client_port;
        client_cmd_socket = client_socket;
        return 1;
    }
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

bool is_server_disconnected()
{
    int client_socket = client_cmd_socket;
    // 非阻塞
    set_flag(client_socket, O_NONBLOCK);
    char buffer[10];
    ssize_t length = recv(client_socket, buffer, 10, 0);
    clr_flag(client_socket, O_NONBLOCK);
    return length == 0;
}