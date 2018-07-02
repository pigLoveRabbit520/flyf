#ifndef _FTP_CLIENT_FUNCS_H
#define _FTP_CLIENT_FUNCS_H


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
#include "stdarg.h"
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <regex.h>
#include <arpa/inet.h>
#include "encode.h"
#include "common.h"

extern char recv_buffer[BUFFER_SIZE];

char *fgets_wrapper(char *buffer, size_t buflen, FILE *fp);
void empty_buffer();
void close_cmd_socket();
int send_cmd(const char* format, ...);
int get_response();
bool start_with(const char *pre, const char *str);
bool respond_with_code(const char *respond, int code); // 是否返回正确响应码
bool respond_exists_code(const char *respond, int code); // 接收了多行返回是否返回正确响应码
unsigned int cal_data_port(const char *recv_buffer);
bool is_connected(int socket_fd);
bool check_server_ip(const char *server_ip);
int connect_server(int socket, const char *server_ip, unsigned int server_port);
int enter_passvie_mode();
int get_server_connected_socket(const char *server_ip, unsigned int client_port, unsigned int server_port);
void set_flag(int, int);
void clr_flag(int, int);
bool is_server_disconnected(); // server端是否断开连接
int set_keepalive(int socket);
const char* get_server_ip();

#endif