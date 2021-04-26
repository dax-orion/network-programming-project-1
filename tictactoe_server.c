/*
    Author: Dax Hurley and Hunter Culler
    Description: Dgram based Tic-Tac-Toe with multicast server recover.
*/
/* include files go here */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "tictactoe_lib.h"

/* #define section, for now we will define the number of rows and columns */
#define TIMEOUT 30
#define MAXGAMES 10

struct GameState
{
    char board[ROWS][COLUMNS];
    int currentPlayer;
    time_t lastMoveTimestamp;
    int active;
    struct ParsedMessage messages[11];
    unsigned int seqNum;
};

/* C language requires that you predefine all the routines you are writing */
int tictactoe(char board[ROWS][COLUMNS], int sock, struct SocketData sockData, int playerChoice);
int getMove(int playerNumber, char board[ROWS][COLUMNS]);
int getPossibleWin(int playerNumber, char board[ROWS][COLUMNS]);
void makeMove(char board[ROWS][COLUMNS], int playerNumber, int choice);
void initializeGames(struct GameState games[MAXGAMES]);
struct SocketData createServerSocket(int port);

int main(int argc, char *argv[])
{
    // seed the random number generator
    time_t t;
    srand((unsigned)time(&t));

    int rc;
    struct SocketData sockData, MC_sockData;
    int port;
    struct sockaddr_in clientaddr;
    struct GameState games[MAXGAMES];
    initializeGames(games);
    int runningGames = 0;
    int clientSDList[MAXGAMES + 1] = {0};
    fd_set socketFDS;
    int maxSD = 0;
    struct ip_mreq mreq;

    // read in and validate parameters
    if (argc != 2)
    {
        printf("Usage: tictactoe <port>\n");
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);

    // create the socket
    sockData = createServerSocket(port);
    MC_sockData = createMulticastSocket(MC_GROUP, MC_PORT, mreq);
    printf("socket created!\n");
    listen(sockData.sock, 5);
    maxSD = sockData.sock;

    // start accepting messagess
    while (1)
    {
        FD_ZERO(&socketFDS);
        FD_SET(sockData.sock, &socketFDS);
        FD_SET(MC_sockData.sock, &socketFDS);

        // find the max socket
        for (int i = 0; i < MAXGAMES; i++)
        {
            if (clientSDList[i] > 0)
            {
                FD_SET(clientSDList[i], &socketFDS);
                if (clientSDList[i] > maxSD)
                {
                    maxSD = clientSDList[i];
                }
            }
        }

        maxSD = max(maxSD, MC_sockData.sock);

        // select a connection
        rc = select(maxSD + 1, &socketFDS, NULL, NULL, NULL);
        printf("select() returned code: %d\n", rc);
        printf("made it past select\n");
        if (rc == -1)
        {
            perror("error on select()");
        }

        if (FD_ISSET(sockData.sock, &socketFDS))
        {
            // accept the connection
            int connected_sd = accept(sockData.sock, (struct sockaddr *)&clientaddr, &sockData.my_addr_len);
            printf("accepted connection\n");
            int added = 0;
            // locate an empty slot
            for (int i = 0; i < MAXGAMES; i++)
            {
                if (clientSDList[i] == 0)
                {
                    // add the connection to the slot
                    clientSDList[i] = connected_sd;
                    added = 1;
                    break;
                }
            }

            if (added == 0)
            {
                printf("No room for another connection\n");
                continue;
            }
        }

        if (FD_ISSET(MC_sockData.sock, &socketFDS))
        {
            char reconnectMsg[2];
            rc = recvfrom(MC_sockData.sock, reconnectMsg, 3, 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr));
            if (rc < 0)
            {
                perror("Error receiving on multi-cast socket");
                continue;
            }
            if (reconnectMsg[0] != VERSION || reconnectMsg[1] != 0x04)
            {
                printf("Got an invalid message on multi-cast socket\n");
                continue;
            }

            int hasRoom = 0;
            // locate an empty slot
            for (int i = 0; i < MAXGAMES; i++)
            {
                if (games[i].active == 0)
                {
                    // has an empty slot for a game
                    hasRoom = 1;
                    break;
                }
            }
            if (hasRoom == 0)
            {
                printf("No room for another connection\n");
                continue;
            }
            else
            {
                char connectInfo[3];
                connectInfo[0] = VERSION;
                connectInfo[1] = htons(port);
                rc = sendto(MC_SockData.sock, connectInfo, 3, 0, (struct sockaddr *)&clientaddr, sizeof(clientaddr));
            }
        }

        // for every connection
        for (int i = 0; i < MAXGAMES; i++)
        {

            // if connection is active read from that socket
            if (FD_ISSET(clientSDList[i], &socketFDS))
            {
                char buf[5];
                int bytesRead;

                //unsigned int clientLen = sizeof(clientaddr);
                // read in message
                bytesRead = read(clientSDList[i], buf, 5);
                
                printf("bytesRead: %d\n", bytesRead);
                printf("Message from client: ");
                for (int i = 0; i < bytesRead; i++)
                {
                    printf("%d", buf[i]);
                }
                printf("\n");

                if (bytesRead < 1)
                {
                    perror("Error receiving message");
                    close(clientSDList[i]);
                    clientSDList[i] = 0;
                    continue;
                }
                else
                {
                    // let's parse the message
                    struct ParsedMessage message = parseMessage(buf, bytesRead);
                    printf("Sequence number: %d\n", message.seqNum);
                    if (!message.valid)
                    {
                        printf("Got an invalid message.\n");
                        close(clientSDList[i]);
                        clientSDList[i] = 0;
                        continue;
                    }
                    if (runningGames >= MAXGAMES)
                    {
                        printf("Can't accept any more games, queue full.");
                        close(clientSDList[i]);
                        clientSDList[i] = 0;
                        continue;
                    }
                    if (message.seqNum > 0)
                    {
                        printf("Game number: %d\n", message.currentGame);
                    }
                    // got new game command - check if we can accept a new game right now
                    if (message.command == NEW_GAME)
                    {
                        printf("Got new game command\n");
                        int newGameIndex = -1;
                        // find a free slot for the new game
                        for (int i = 0; i < MAXGAMES; i++)
                        {
                            // game is inactive, so we can use this slot
                            if (games[i].active == 0)
                            {
                                newGameIndex = i;
                                break;
                            }
                        }
                        // init the board
                        rc = initSharedState(games[newGameIndex].board);
                        // initial values
                        games[newGameIndex].messages[0] = message;
                        games[newGameIndex].currentPlayer = 1;
                        games[newGameIndex].active = 1;
                        games[newGameIndex].lastMoveTimestamp = time(NULL);
                        games[newGameIndex].seqNum++;

                        //begin constructing the message to be sent to the client
                        struct ParsedMessage newMessage;
                        newMessage.command = MOVE;
                        // get a move from AI
                        newMessage.move = getMove(1, games[newGameIndex].board);
                        newMessage.currentGame = newGameIndex;
                        newMessage.seqNum = games[newGameIndex].seqNum;
                        // update board with move
                        makeMove(games[newGameIndex].board, 1, newMessage.move);
                        char buf[5];
                        buildBuf(newMessage, buf);
                        games[newGameIndex].messages[games[newGameIndex].seqNum] = newMessage;

                        // send the buf
                        rc = send(clientSDList[i], buf, 5, 0);
                        games[newGameIndex].seqNum++;
                        printf("Sent response of move %d for game new game %d.\n", newMessage.move, newGameIndex);

                        // we have a new game, add it to the count
                        runningGames++;
                        printf("Remaining game slots: %d\n", MAXGAMES - runningGames);
                    }
                    // TODO: check if command is resume to handle reading in game board
                    else if(message.command == RESUME)
                    {

                    }
                    else
                    {
                        // check if game is active
                        if (!games[message.currentGame].active)
                        {
                            printf("Got a message for game %d, which is inactive.\n", message.currentGame);
                            close(clientSDList[i]);
                            clientSDList[i] = 0;
                        }
                        // not a new game, let's check for a timeout
                        else if (games[message.currentGame].active && time(NULL) > games[message.currentGame].lastMoveTimestamp + TIMEOUT)
                        {
                            printf("Game %d timed out!\n", message.currentGame);
                            runningGames--;
                            games[message.currentGame].active = 0;
                        }
                        // handle game over
                        else if (message.command == GAME_OVER && message.valid)
                        {
                            // someone won, make this game inactive
                            printf("Game %d over!\n", message.currentGame);
                            runningGames--;
                            games[message.currentGame].active = 0;
                        }
                        // client sent a move for their game
                        else if (message.command == MOVE && message.valid)
                        {
                            int choice = message.move;
                            int gameNum = message.currentGame;
                            games[gameNum].lastMoveTimestamp = time(NULL);

                            // validate the move
                            int validMove = validateMove(choice, games[gameNum].board);

                            if (validMove == 1 && games[gameNum].active)
                            {
                                // move is valid, add it to the board
                                games[gameNum].messages[games[gameNum].seqNum] = message;
                                games[gameNum].seqNum++;
                                makeMove(games[gameNum].board, 2, choice);

                                // has anyone won?
                                int win = checkwin(games[gameNum].board);
                                if (win == -1)
                                {
                                    // nobody won so we need to reply
                                    struct ParsedMessage newMessage;
                                    newMessage.command = MOVE;
                                    newMessage.move = getMove(1, games[gameNum].board);
                                    newMessage.currentGame = gameNum;
                                    newMessage.seqNum = games[gameNum].seqNum;
                                    games[gameNum].messages[games[gameNum].seqNum] = newMessage;
                                    makeMove(games[gameNum].board, 1, newMessage.move);
                                    char buf[5];
                                    buildBuf(newMessage, buf);
                                    rc = send(clientSDList[i], buf, 5, 0);
                                    games[gameNum].seqNum++;

                                    printf("Sent response of move %d for game %d.\n", newMessage.move, gameNum);
                                }
                                else
                                {
                                    printf("Game %d over!\n", gameNum);
                                    // construct the game over message
                                    struct ParsedMessage newMessage;
                                    newMessage.command = GAME_OVER;
                                    newMessage.move = 0;
                                    newMessage.currentGame = gameNum;
                                    newMessage.seqNum = games[gameNum].seqNum;
                                    games[gameNum].messages[games[gameNum].seqNum] = newMessage;
                                    char buf[5];
                                    buildBuf(newMessage, buf);
                                    rc = send(clientSDList[i], buf, 5, 0);
                                    games[gameNum].seqNum++;
                                }
                            }
                            else
                            {
                                printf("Invalid Move on game #%d!\n", gameNum);
                            }
                        }
                        else
                        {
                            printf("Invalid message!\n");
                        }
                    }
                }

                // loop through all games, check if any of them timed out
                for (int i = 0; i < MAXGAMES; i++)
                {
                    if (games[i].active == 1 && time(NULL) > (games[i].lastMoveTimestamp + TIMEOUT))
                    {
                        //if a game times out, end that game
                        printf("Game %d timed out!\n", i);
                        runningGames--;
                        games[i].active = 0;
                    }
                }
            }
        }
    }

    return rc;
}

int getMove(int playerNumber, char board[ROWS][COLUMNS])
{
    // later we might have this calculate the best possible move
    // for now it randomly picks a move unless computer can win or an opponent can win
    int opponent = getOpponent(playerNumber);

    // can I win?
    int playerWin = getPossibleWin(playerNumber, board);
    if (playerWin != -1)
    {
        return playerWin;
    }

    // can my opponent win? We should block them
    int opponentWin = getPossibleWin(opponent, board);
    if (opponentWin != -1)
    {
        return opponentWin;
    }

    // otherwise return a random valid move

    // count number of possible moves
    int possibleMovesCnt = 0;
    for (int i = 1; i <= 9; i++)
    {
        struct Move move = getRowColFromMove(i);
        if (board[move.row][move.column] == '0' + i)
        {
            possibleMovesCnt++;
        }
    }

    // put all possible moves in an array
    int *possibleMoves = malloc(possibleMovesCnt * sizeof(int));
    int index = 0;
    for (int i = 1; i <= 9; i++)
    {
        struct Move move = getRowColFromMove(i);
        if (board[move.row][move.column] == '0' + i)
        {
            possibleMoves[index] = i;
            index++;
        }
    }

    // return a random index in the array of valid moves
    int choice = possibleMoves[rand() % possibleMovesCnt];
    free(possibleMoves);

    return choice;
}

int getPossibleWin(int playerNumber, char board[ROWS][COLUMNS])
{
    // brute force check to see if a player can win this move
    // returns the winning move for playerNumber or -1
    char mark = getMark(playerNumber);

    // for all possible moves
    for (int i = 1; i <= 9; i++)
    {
        // get position
        struct Move move = getRowColFromMove(i);

        // check if it's a valid move
        if (board[move.row][move.column] == '0' + i)
        {

            // put the move on the board
            board[move.row][move.column] = mark;

            // check if the move is a winning move
            if (checkwin(board) == 1)
            {

                // rewind the board state and return the move
                board[move.row][move.column] = '0' + i;
                return i;
            }

            // rewind the board state and keep going
            board[move.row][move.column] = '0' + i;
        }
    }

    return -1;
}

void makeMove(char board[ROWS][COLUMNS], int playerNumber, int choice)
{
    // modifies the board by player number and move value 1-9
    struct Move move = getRowColFromMove(choice);
    char mark = getMark(playerNumber);

    board[move.row][move.column] = mark;
}

void initializeGames(struct GameState games[MAXGAMES])
{
    // sets initial values for the array of game states
    for (int i = 0; i < MAXGAMES; i++)
    {
        initSharedState(games[i].board);
        games[i].currentPlayer = 1;
        games[i].lastMoveTimestamp = 0;
        games[i].active = 0;
        games[i].seqNum = 0;
    }
}

struct SocketData createServerSocket(int port)
{
    // function to simply creating a server socket, can create server or client socket
    // char[] - the ip address in dotted decimal format specify "any" for any
    // int port - the port
    // int clientFlag - indicates whether the socket is for a client (1) or server (0)

    int sock;
    struct sockaddr_in addr;

    printf("Port: %d\n", port);

    // build the address
    sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    // if we're in server mode we bind
    int bindResp;

    printf("Binding...\n");
    bindResp = bind(
        sock,
        (struct sockaddr *)&addr,
        sizeof(struct sockaddr_in));

    if (bindResp < 0)
    {
        close(sock);
        perror("error binding stream socket");
        exit(EXIT_FAILURE);
    }

    // return info about the newly created socket
    struct SocketData sockData;
    sockData.sock = sock;
    sockData.my_addr = addr;
    sockData.my_addr_len = (socklen_t)sizeof(addr);

    return sockData;
}
