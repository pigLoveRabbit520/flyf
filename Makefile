cc = gcc
objects = ftp_client.o common.o encode.o

ftp : $(objects)
		cc -o ftp $(objects)

ftp_client.o : ftp_client.c
common.o : common.h
encode.o : encode.h
clean :
		rm ftp $(objects)