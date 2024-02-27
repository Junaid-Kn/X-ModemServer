PORT=y
CFLAGS= -DPORT=\$(PORT) -g -Wall
FLAGS= -Wall -Werror -fsanitize=address -fsanitize=signed-integer-overflow -g
DEPENDENCIES = xmodemserver.h crc16.h

all: server client

server: crc16.o xmodemserver.o
	gcc ${FLAGS} -o $@ $^

client: crc16.o client1.o
	gcc ${FLAGS} -o $@ $^

clean:
	rm -f *.o server client