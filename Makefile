CC= /usr/bin/gcc

all:	udpclient udpserver
  
udpclient: udpclient.c;
	${CC} udpclient.c -o udpclient

udpserver: udpserver.c;
	${CC} udpserver.c -o udpserver -lm

clean:
	rm udpclient udpserver out.txt
