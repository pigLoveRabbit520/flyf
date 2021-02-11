#include "common.h"

static const char commandlist[NCOMMANDS][10] =
{
	"get",
	"put",

	"mget",
	"mput",
	"delete",
	
	"cd",
	"lcd",
	
	"mgetwild",
	"mputwild",
	
	"dir",
	"ldir",

	"ls",
	"lls",
	
	"mkdir",
	"lmkdir",

	"rget",
	"rput",
	
	"pwd",
	"lpwd",
	"ascii",
	"binary",
	"open",
	"help",
	"quit",
	"exit"
};


unsigned short login_time = 0;
bool server_connected = false;
char cmd_read[CMD_READ_BUFFER_SIZE];

void set0(char *p, size_t size)
{
	memset(p, 0, size);
}


static void append_path(struct command* c, char* s)
{
	c->npaths++;
	char** temppaths = (char**) malloc(c->npaths * sizeof(char*));
	if(c->npaths > 1)
		memcpy(temppaths, c->paths, (c->npaths - 1) * sizeof(char*));

	char* temps = (char*) malloc((strlen(s) + 1) * sizeof(char));
	int i;
	for(i = 0; *(temps + i) = *(s + i) == ':' ? ' ' : *(s + i); i++)
		;

	*(temppaths + c->npaths - 1) = temps;

	c->paths = temppaths;
}

struct command*  userinputtocommand(char s[LENUSERINPUT])
{
	struct command* cmd = (struct command*) malloc(sizeof(struct command));
	cmd->id = -1;
	cmd->npaths = 0;
	cmd->paths = NULL;
	char* savestate;
	char* token;
	int i, j;
	for(i = 0; ; i++, s = NULL)
	{
		token = strtok_r(s, " \t\n", &savestate);
		if(token == NULL)
			break;
		if(cmd->id == -1)
			for(j = 0; j < NCOMMANDS; j++)
			{	
				if(!strcmp(token, commandlist[j]))
				{
					cmd->id = j;
					break;
				}
			}// ommitting braces for the "for loop" here is \
			 disastrous because the else below gets \
			 associated with the "if inside the for loop". \
			 #BUGFIX
		else
			append_path(cmd, token);
	}
	if(cmd->id == MGET && !strcmp(*cmd->paths, "*"))
		cmd->id = MGETWILD;
	else if(cmd->id == MPUT && !strcmp(*cmd->paths, "*"))
		cmd->id = MPUTWILD;
	if(cmd->id != -1)
		return cmd;
	else
	{
		return NULL;
	}
}


void printcommand(struct command* c)
{
	
	printf("\t\tPrinting contents of the above command...\n");
	printf("\t\tid = %d\n", c->id);
	printf("\t\tnpaths = %d\n", c->npaths);
	printf("\t\tpaths =\n");
	int i;
	for(i = 0; i < c->npaths; i++)
		printf("\t\t\t%s\n", c->paths[i]);
	printf("\n");
}

void freecommand(struct command* c)
{
	if (c->npaths > 0)
	{
		int i;
		// free strings
		for(i = 0; i < c->npaths; i++)
			free(c->paths[i]);
		free(c->paths);
	}
	free(c);
}

// 客户端打开任意的本地端口 (N > 1024) 
unsigned short get_rand_port()
{
    srand((unsigned) time(NULL));
    unsigned int number = rand() % 101 + 1; // 产生1-101的随机数
    return number + 1024;
}