#ifndef _CMDS_H
#define _CMDS_H

#include "ftp_client.h"

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