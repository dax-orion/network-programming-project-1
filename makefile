#makefile for tictactoe program

CC=gcc
CFLAGS = -g -Wall -Werror

all: client server

client: tictactoe_client.c
	$(CC) $(CFLAGS) -o tictactoe_client tictactoe_client.c tictactoe_lib.c

server: tictactoe_server.c
	$(CC) $(CFLAGS) -o tictactoe_server tictactoe_server.c tictactoe_lib.c

clean:
	rm tictactoe_client; rm tictactoe_server
