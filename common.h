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

enum TYPE
	{
		REQU,
		DONE,
		INFO,
		TERM,
		DATA,
		EOT
	};

#define	NP		0
#define	HP		1

#define	er(e, x)					\
	do						\
	{						\
		perror("ERROR IN: " #e "\n");		\
		fprintf(stderr, "%d\n", x);		\
		exit(-1);				\
	}						\
	while(0)

#define	LENBUFFER	504		// so as to make the whole packet well-rounded ( = 512 bytes)
#define LENUSERINPUT	1024

struct command
{
	short int id;
	int npaths;
	char** paths;
};

struct command* userinputtocommand(char [LENUSERINPUT]);
void printcommand(struct command* c);
void freecommand(struct command* c);
void set0(char *p, size_t size);
unsigned short get_rand_port();

#define NCOMMANDS 25
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

#endif