#ifndef _TICTACTOE_LIB_H
#define _TICTACTOE_LIB_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define ROWS 3
#define COLUMNS 3
#define VERSION 0x06

#define MC_PORT 1818
#define MC_GROUP "239.0.0.1"

enum command {NEW_GAME = 0x00, MOVE = 0x01, GAME_OVER = 0x02};

struct SocketData {
    int sock;
    struct sockaddr_in my_addr;
    socklen_t my_addr_len;
};

struct ParsedMessage {
    enum command command;
    unsigned int move;
    int valid;
    int currentGame;
    unsigned int seqNum;
};

struct Move {
    int row;
    int column;
};

int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS]);
int validateMove(int choice, char board[ROWS][COLUMNS]);
char getMark(int playerNumber);
int getOpponent(int playerNumber);
struct ParsedMessage parseMessage(char *buf, int length);
void buildBuf(struct ParsedMessage parsedMessage, char buf[5]);
struct Move getRowColFromMove(int choice);
int initSharedState(char board[ROWS][COLUMNS]);

#endif