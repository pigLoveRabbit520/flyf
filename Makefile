cc = gcc
objects = ftp_client.o cmds.o common.o encode.o

ftp : $(objects)
		cc -o ftp $(objects)

ftp_client.o : ftp_client.h cmds.h common.h encode.h
cmds.o : cmds.h ftp_client.h
common.o : common.h
encode.o : encode.h
clean :
		rm ftp $(objects)