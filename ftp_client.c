#include "ftp_client.h"

// 被动模式
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s Server IP Address\n", argv[0]);
        exit(1);
    }
    // 检查ip合法
    if (!check_server_ip(argv[1]))
    {
        perror("Server IP address error!");
        exit(1);
    }
    server_ip = argv[1];
    client_cmd_port = get_rand_port();
    int client_cmd_socket = get_server_connected_socket(server_ip, client_cmd_port, FTP_SERVER_PORT);
    if (client_cmd_socket < 0)
        exit(1);
    server_connected = true;
    int length = 0;
    //set_keepalive(client_cmd_socket);
    char recv_buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];
    bzero(recv_buffer, BUFFER_SIZE);
    bzero(send_buffer, BUFFER_SIZE);
    signal(SIGPIPE, SIG_IGN);

    for (;;)
    {
        int res = user_login(client_cmd_socket, recv_buffer, send_buffer);
        if (res == ERR_DISCONNECTED)
        {
            close(client_cmd_socket);
            exit(1);
        } else if (res == ERR_READ_FAILED || res == ERR_INCORRECT_CODE)
        {
            printf("login failed\n");
        } else
            break;
    }

    struct command* cmd;
    for (;;)
    {
        printf("FTP>");
        if (fgets_wrapper(cmd_read, CMD_READ_BUFFER_SIZE, stdin) != 0)
        {
            cmd = userinputtocommand(cmd_read);
            if(!cmd)
                continue;
            if (!server_connected && cmd->id != OPEN && cmd->id != EXIT)
            {
                printf("not connected\n");
                continue;
            }
            if (server_connected && is_server_disconnected(client_cmd_socket))
            {
                close(client_cmd_socket);
                server_connected = false;
                printf("not connected\n");
                continue;
            }
            //printcommand(cmd);
            switch(cmd->id)
            {
                case LS:
                {
                    int client_data_socket = enter_passvie_mode(client_cmd_socket, client_cmd_port + 1, send_buffer, recv_buffer);
                    if (client_data_socket == ERR_DISCONNECTED)
                    {
                        close(client_cmd_socket);
                        server_connected = false;
                        continue;
                    }
                    else if (client_data_socket == ERR_CREATE_BINDED_SOCKTED || client_data_socket == ERR_CONNECT_SERVER_FAIL || client_data_socket == ERR_INCORRECT_CODE)
                    {
                        continue;
                    }
                    sprintf(send_buffer, "LIST \r\n");
                    if (send_cmd(client_cmd_socket, send_buffer) <= 0)
                    {
                        close(client_data_socket);
                        close(client_cmd_socket);
                        printf("send [LIST] command failed\n");
                        continue;
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
                        continue;
                    }
                    if (!respond_with_code(recv_buffer, 125) && !respond_with_code(recv_buffer, 150))
                    {
                        close(client_data_socket);
                        printf("%s", recv_buffer);
                        continue;
                    }
                    // 注意发送了LIST命令后，可能数据端口发送很快
                    // 测试发现这里接收数据时，可能会已经收到了226 Transfer complete.了
                    // 非阻塞
                    set_flag(client_data_socket, O_NONBLOCK);

                    pid_t pid;
                    if ((pid = fork()) < 0) {
                        printf("fork error");
                        continue;
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
                                continue;
                            }
                            if (!respond_with_code(recv_buffer, 226))
                            {
                                printf("LIST end failed\n");
                                continue;
                            }
                        }
                        int status = 0;
                        waitpid(pid, &status, 0);
                    }
                }
                break;
                case LLS:
                {
                    pid_t pid;
                    if ((pid = fork()) < 0) {
                        printf("fork error");
                        continue;
                    } else if (pid == 0) {
                        if (execl("/bin/ls", "ls", "-l", "-a", NULL) < 0) {  
                            perror("error on exec");  
                            exit(0);
                        }
                    } else {
                        wait(NULL);
                    }
                }
                break;
                case GET:
                {
                    if (!cmd->paths)
                    {
                        printf("please input the file\n");
                        continue;
                    }
                    char *filename = cmd->paths[0];
                    int client_data_socket = enter_passvie_mode(client_cmd_socket, client_cmd_port + 1, send_buffer, recv_buffer);
                    if (client_data_socket == ERR_DISCONNECTED)
                    {
                        close(client_cmd_socket);
                        server_connected = false;
                        continue;
                    }
                    else if (client_data_socket == ERR_CREATE_BINDED_SOCKTED || client_data_socket == ERR_CONNECT_SERVER_FAIL || client_data_socket == ERR_INCORRECT_CODE)
                    {
                        continue;
                    } 
                    sprintf(send_buffer, "SIZE %s\r\n", filename);
                    if (send_cmd(client_cmd_socket, send_buffer) <= 0)
                    {
                        close(client_data_socket);
                        close(client_cmd_socket);
                        printf("send [SIZE] command failed\n");
                        continue;
                    }
                    if (get_response(client_cmd_socket, recv_buffer) <= 0)
                    {
                        close(client_data_socket);
                        close(client_cmd_socket);
                        printf("Recieve [SIZE] command info from server %s failed!\n", server_ip);
                        continue;
                    }
                    if (!respond_with_code(recv_buffer, 213))
                    {
                        close(client_data_socket);
                        printf("%s\n", recv_buffer);
                        continue;
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
                        continue;
                    }
                    if (get_response(client_cmd_socket, recv_buffer) <= 0)
                    {
                        close(client_data_socket);
                        close(client_cmd_socket);
                        printf("Recieve [RETR] command info from server %s failed!\n", server_ip);
                        continue;
                    }
                    if (!respond_with_code(recv_buffer, 125) && !respond_with_code(recv_buffer, 150))
                    {
                        close(client_data_socket);
                        printf("%s", recv_buffer);
                        continue;
                    }

                    // 非阻塞
                    set_flag(client_data_socket, O_NONBLOCK);
                    pid_t pid;
                    if ((pid = fork()) < 0) {
                        printf("fork error");
                        continue;
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
                        char *ptr = "";
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
                            fwrite(data_buffer, char_size, length, fp);
                        }
                        fclose(fp);
                        exit(0);
                    } else {
                        if (get_response(client_cmd_socket, recv_buffer) <= 0)
                        {
                            printf("[GET] command recieve data from server %s failed!\n", server_ip);
                            continue;
                        }
                        if (!respond_with_code(recv_buffer, 226))
                        {
                            printf("GET end failed\n");
                            continue;
                        }
                        printf("%s", recv_buffer);
                        int status = 0;
                        waitpid(pid, &status, 0);
                    }
                }
                break;
                case PUT:
                {
                    if (!cmd->paths)
                    {
                        printf("please input the filename\n");
                        continue;
                    }
                    char *filename = cmd->paths[0];
                    int client_data_socket = enter_passvie_mode(client_cmd_socket, client_cmd_port + 1, send_buffer, recv_buffer);
                    if (client_data_socket == ERR_DISCONNECTED)
                    {
                        close(client_cmd_socket);
                        server_connected = false;
                        continue;
                    }
                    else if (client_data_socket == ERR_CREATE_BINDED_SOCKTED || client_data_socket == ERR_CONNECT_SERVER_FAIL || client_data_socket == ERR_INCORRECT_CODE)
                    {
                        continue;
                    }
                    sprintf(send_buffer, "STOR %s\r\n", filename);
                    if (send_cmd(client_cmd_socket, send_buffer) <= 0)
                    {
                        close(client_data_socket);
                        close(client_cmd_socket);
                        printf("send [STOR] command failed\n");
                        continue;
                    }
                    if (get_response(client_cmd_socket, recv_buffer) <= 0)
                    {
                        close(client_data_socket);
                        close(client_cmd_socket);
                        printf("Recieve [STOR] command info from server %s failed!\n", server_ip);
                        continue;
                    }
                    if (!respond_with_code(recv_buffer, 125) && !respond_with_code(recv_buffer, 150))
                    {
                        close(client_data_socket);
                        printf("%s", recv_buffer);
                        continue;
                    }

                    // 非阻塞
                    set_flag(client_data_socket, O_NONBLOCK);
                    pid_t pid;
                    if ((pid = fork()) < 0) {
                        printf("fork error");
                        continue;
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
                            numread = fread(data_buffer, char_size, FILE_READ_BUFFER_SIZE, fp);
                            bzero(data_buffer, FILE_READ_BUFFER_SIZE);
                            int length = send(client_data_socket, data_buffer, FILE_READ_BUFFER_SIZE, 0);
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
                                printf("[PUT] command send data failed\n");
                                exit(1);
                            }
                            if (numread == FILE_READ_BUFFER_SIZE) continue;
                            else if (numread < FILE_READ_BUFFER_SIZE && numread > 0)
                                break;
                            else
                            {
                                printf("read file failed\n");
                                break;
                            }
                        }
                        fclose(fp);
                        exit(0);
                    } else {
                        int status = 0;
                        waitpid(pid, &status, 0);
                        if (status == 0)
                            printf("send file %s complete.\n", filename);
                        else 
                            printf("send file %s failed.\n", filename);
                    }

                }
                break;
                case CD:
                {
                    if (!cmd->paths)
                    {
                        printf("please input the path\n");
                        continue;
                    }
                    sprintf(send_buffer, "CWD %s\r\n", cmd->paths[0]);
                    send_cmd(client_cmd_socket, send_buffer);
                    length = get_response(client_cmd_socket, recv_buffer);
                    printf("%s", recv_buffer);
                }
                break;
                case LCD:
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
                break;
                case PWD:
                {
                    sprintf(send_buffer, "PWD\r\n");
                    send_cmd(client_cmd_socket, send_buffer);
                     // 227
                    length = get_response(client_cmd_socket, recv_buffer);
                    printf("%s", recv_buffer);
                }
                break;
                case LPWD:
                {
                    char buf[80];
                    getcwd(buf, sizeof(buf));
                    printf("current working directory : %s\n", buf);
                }
                break;
                case ASCII:
                {
                    sprintf(send_buffer, "TYPE A\r\n");
                    send_cmd(client_cmd_socket, send_buffer);
                     // 227
                    length = get_response(client_cmd_socket, recv_buffer);
                    printf("%s", recv_buffer);
                }
                break;
                case BINARY:
                {
                    sprintf(send_buffer, "TYPE I\r\n");
                    send_cmd(client_cmd_socket, send_buffer);
                     // 227
                    length = get_response(client_cmd_socket, recv_buffer);
                    printf("%s", recv_buffer);
                }
                break;
                case DELETE:
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
                break;
                case MKDIR:
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
                break;
                case OPEN:
                {
                    if (!cmd->paths)
                    {
                        printf("please input the host\n");
                        continue;
                    }
                    server_ip = cmd->paths[0];
                    client_cmd_port = get_rand_port();
                    client_cmd_socket = get_server_connected_socket(server_ip, client_cmd_port, FTP_SERVER_PORT);
                    if (client_cmd_socket < 0)
                        continue;
                    server_connected = true;
                    int res = user_login(client_cmd_socket, recv_buffer, send_buffer);
                    if (res == ERR_DISCONNECTED)
                    {
                        close(client_cmd_socket);
                        continue;
                    } else if (res == ERR_READ_FAILED || res == ERR_INCORRECT_CODE)
                    {
                        printf("try login again: please use [open] command\n");
                    }
                }
                break;
                case QUIT:
                case EXIT:
                {
                    sprintf(send_buffer, "QUIT\r\n");
                    send_cmd(client_cmd_socket, send_buffer);
                    printf("Goodbye!\n");
                    exit(0);
                }
                break;
            }
            freecommand(cmd);
        }
    }
    //关闭socket
    close(client_cmd_socket);
    return 0;
}
