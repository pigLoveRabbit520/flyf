#ifndef _COMMON_H
#define _COMMON_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>

enum COMMAND
{
	GET,
	PUT,
	MGET,
	MPUT,
	DELETE,
	CD,
	LCD,
	MGETWILD,
	MPUTWILD,
	DIR_,		// _ to avoid conflict with directory pointer DIR
	LDIR,
	LS,
	LLS,
	MKDIR,
	LMKDIR,
	RGET,
	RPUT,
	PWD,
	LPWD,
	ASCII,
	BINARY,
	OPEN,
	HELP,
	QUIT,
	EXIT
};


#define	NP		                   0
#define	HP		                   1
#define	LENBUFFER	               504		// so as to make the whole packet well-rounded ( = 512 bytes)
#define LENUSERINPUT	           1024
#define NCOMMANDS                  25
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


struct command
{
	short int id;
	int npaths;
	char** paths;
};

unsigned short login_time = 0;
char *server_ip = NULL;
bool server_connected = false;
char cmd_read[CMD_READ_BUFFER_SIZE];


struct command* userinputtocommand(char [LENUSERINPUT]);
void printcommand(struct command* c);
void freecommand(struct command* c);
void set0(char *p, size_t size);
unsigned short get_rand_port();

#endif