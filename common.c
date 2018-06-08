#include "common.h"

static const char commandlist[NCOMMANDS][10] =
	{
		"get",
		"put",

		"mget",
		"mput",
		
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
		
		"exit"
	};

void set0(struct packet* p)
{
	memset(p, 0, sizeof(struct packet));
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

struct command* userinputtocommand(char s[LENUSERINPUT])
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
		fprintf(stderr, "\tError parsing command\n");
		return NULL;
	}
}