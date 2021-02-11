#include <netdb.h>
#include "cmds.h"

const char* getServerIpByHostname(const char* name)
{
    struct hostent *hptr;
    if((hptr = gethostbyname(name)) == NULL)
    {
        return NULL;
    }
    // gethostbyname may return static memory pointer
    const char* server_ip = inet_ntoa(*(struct in_addr*)hptr->h_addr_list[0]);
    return server_ip;
}

// 被动模式
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: %s [server ip address] [port]\n", argv[0]);
        exit(1);
    }
    char *hostname = argv[1];
    const char *server_ip = NULL;
    unsigned int server_port = FTP_SERVER_PORT;

    if ((server_ip = getServerIpByHostname(hostname)) == NULL)
    {
        printf("gethostbyname error: %s\n", hostname);
        exit(1);
    }
    if (argc >= 3)
    {
        const char* port_str = argv[2];
        unsigned int port = (unsigned int)atoi(port_str);
        if (port == 0)
        {
            printf("port is valid: %s\n", port_str);
            exit(1);
        }
        server_port = port;
    }

    if (get_server_connected_socket(server_ip, get_rand_port(), server_port) >= 0)
    {
        server_connected = true;
        signal(SIGPIPE, SIG_IGN);
        int res = user_login();
        if (res == ERR_DISCONNECTED)
        {
            server_connected = false;
        }
    }

    struct command* cmd;
    for (;;)
    {
        printf("FTP> ");
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
            if (server_connected && is_server_disconnected())
            {
                close_cmd_socket();
                server_connected = false;
                printf("not connected\n");
                continue;
            }
            //printcommand(cmd);
            switch(cmd->id)
            {
                case LS:
                ls();
                break;
                case LLS:
                lls();
                break;
                case GET:
                get(cmd);
                break;
                case PUT:
                put(cmd);
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
                create_dir(cmd);
                break;
                case OPEN:
                open_cmd(cmd);
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
    close_cmd_socket();
    return 0;
}
