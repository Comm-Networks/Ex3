CFLAGS=-Wall -g

all: tftp-server

clean:
	-rm -f tftp-server.o tftp-server 

tftp-server: tftp-server.o
	gcc -o tftp-server tftp-server.o $(CFLAGS)

tftp-server.o: tftp-server.c
	gcc -c $(CFLAGS) tftp-server.c
