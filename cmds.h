#ifndef _CMDS_H
#define _CMDS_H

#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include "common.h"
#include "ftp_client_funcs.h"

extern int errno;
extern char recv_buffer[BUFFER_SIZE];
extern char send_buffer[BUFFER_SIZE];
extern int client_cmd_socket;
extern unsigned int client_cmd_port;


void ls();
void lls();
void get(struct command* cmd);
void put(struct command* cmd);
void cd(struct command* cmd);
void lcd(struct command* cmd);
void pwd(struct command* cmd);
void lpwd(struct command* cmd);
void ascii();
void binary();
void delete_cmd(struct command* cmd);
void mkdir(struct command* cmd);
void open_cmd(struct command* cmd);
void help();
void exit_cmd();

#endif