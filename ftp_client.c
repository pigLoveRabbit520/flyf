#include "ftp_client.h"

char recv_buffer[BUFFER_SIZE];
char send_buffer[BUFFER_SIZE];

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

    set0(recv_buffer, BUFFER_SIZE);
    set0(send_buffer, BUFFER_SIZE);
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
                ls(client_cmd_port, client_cmd_socket);
                break;
                case LLS:
                lls();
                break;
                case GET:
                get();
                break;
                case PUT:
                put();
                break;
                case CD:
                cd(cmd);
                break;
                case LCD:
                lcd(cmd);
                break;
                case PWD:
                pwd(cmd);
                break;
                case LPWD:
                lpwd(cmd);
                break;
                case ASCII:
                ascii();
                break;
                case BINARY:
                binary();
                break;
                case DELETE:
                delete_cmd(cmd);
                break;
                case MKDIR:
                mkdir(cmd);
                break;
                case OPEN:
                open_cmd(cmd, client_cmd_socket)
                break;
                case HELP:
                help();
                break;
                case QUIT:
                case EXIT:
                exit_cmd();
                break;
            }
            freecommand(cmd);
        }
    }
    //关闭socket
    close(client_cmd_socket);
    return 0;
}
