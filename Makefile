cc = gcc
objects = ftp_client.o ftp_client_funcs.o cmds.o common.o encode.o

ftp : $(objects)
		cc -o ftp $(objects)

ftp_client.o : cmds.h
ftp_client_funcs.o : ftp_client_funcs.h common.h
cmds.o : cmds.h
common.o : common.h
encode.o : encode.h
clean :
		rm -rf ftp $(objects)