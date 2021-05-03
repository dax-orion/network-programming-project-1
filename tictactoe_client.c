/*
    Author: Dax Hurley and Hunter Culler
    Description: Dgram based Tic-Tac-Toe with multicast server recover.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include "tictactoe_lib.h"

#define FALLBACK_TIMEOUT 10

int tictactoe(char board[ROWS][COLUMNS], struct SocketData sockData, int playerChoice, struct ParsedMessage messages[11]);
struct SocketData findNewServer(char boardState[9], int currentSeqNum);
struct SocketData createClientSocket(char ipAddr[15], int port);
struct SocketData findServerFromFile(char boardState[9], int currentSeqNum);

int main(int argc, char *argv[])
{
    int rc;
    char board[ROWS][COLUMNS];
    struct SocketData sockData;
    int port;
    int playerChoice = 2;
    char *ipAddr = malloc(16);
    signal(SIGPIPE, SIG_IGN);

    if (argc != 3)
    {
        printf("Usage: tictactoe <port> <ip>\n");
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);
    strcpy(ipAddr, argv[2]);

    // loops until user decides not to start another game
    do
    {
        struct ParsedMessage messages[11];
        sockData = createClientSocket(ipAddr, port);

        // start the game
        struct ParsedMessage newMessage;
        newMessage.command = NEW_GAME;
        newMessage.move = 0x00;
        newMessage.currentGame = 0x00;
        newMessage.seqNum = 0x00;
        char buf[5];
        buildBuf(newMessage, buf);
        // add message to history
        messages[0] = newMessage;

        // send new game message we just created
        rc = send(sockData.sock, buf, 5, 0);
        printf("Send return code: %d\n", rc);
        if (rc == -1)
        {
            perror("error sending new game command");
            close(sockData.sock);
        }
        else
        {
            // command sent, start a new game
            // Initialize the 'game' board
            rc = initSharedState(board); 
            // call the 'game'
            rc = tictactoe(board, sockData, playerChoice, messages);
        }

        // input info for next game
        printf("Enter new port number: ");
        scanf("%d", &port);
        printf("Enter new IP address: ");
        scanf("%s", ipAddr);
    } while (strcmp(ipAddr, "exit") != 0);
    return rc;
}

int tictactoe(char board[ROWS][COLUMNS], struct SocketData sockData, int playerChoice, struct ParsedMessage messages[11])
{
    /* this is the meat of the game, you'll look here for how to change it up */
    // player 1 always goes first
    int player = 1;       // keep track of whose turn it is
    int choice; // used for keeping track of choice user makes
    int rc;
    char mark; // either an 'x' or an 'o'
    int currentGame = -1;
    int seqCount = 1;
    int winState = -1;

    /* loop, first print the board, then ask player 'n' to make a move */

    printf("Socket: %d\n", sockData.sock);

    printf("\nStarting new game. Good luck! \n\n");
    do
    {
        // print out the board
        print_board(board);
        // depending on who the player is, either us x or o
        mark = getMark(player);
        int valid = 0;

        // check if it's player or opponent's turn
        if (player == playerChoice)
        {
            // print out player
            // using scanf to get move choice
            printf("Player %d, enter a number: ", player);
            do
            {
                scanf("%d", &choice);

                // check if move is valid
                valid = validateMove(choice, board);

                // if it's valid set the board, otherwise undo it
                if (valid == 1)
                {
                    struct Move move = getRowColFromMove(choice);
                    board[move.row][move.column] = mark;
                }
                else
                {
                    printf("Invalid move\n");
                    printf("Player %d, enter a number: ", player);
                    getchar();
                }

            } while (valid == 0);

            // once we have a valid move, send it over the socket

            // check if current game has been set
            if (currentGame == -1)
            {
                printf("Game # must be set by server before we can send a move.");
                close(sockData.sock);
                break;
            }

            // build the move message
            struct ParsedMessage newMessage;
            newMessage.command = MOVE; // send move command
            // this works since these variables will always be 1-15,
            // if it were a large int we would lose data
            unsigned char hexMove = (char)choice;
            newMessage.move = hexMove;
            unsigned char hexGame = (char)currentGame;
            newMessage.currentGame = hexGame;
            newMessage.seqNum = seqCount;
            char buf[5];
            buildBuf(newMessage, buf);
            // add this message to our message history
            messages[seqCount] = newMessage;

            
            rc = send(sockData.sock, buf, 5, 0);
            seqCount++;
            if (rc == -1)
            {
                perror("error sending move");
            }
        }
        else
        {
            // read in the opponent's move
            printf("Waiting for server...\n");
            char buf[5];
            int bytesRead;
            bytesRead = read(sockData.sock, buf, 5);

            printf("Bytesread: %d\n", bytesRead);
            printf("Message from server: ");
            for (int i = 0; i < bytesRead; i++)
            {
                printf("%d", buf[i]);
            }
            printf("\n");
            if (bytesRead == -1)
            {
                perror("error receiving move");
                close(sockData.sock);
                exit(0);
            }
            else if (bytesRead == 0){
                // Close existing socket
                printf("Server disconnected\n");
                int rc = close(sockData.sock);

                // On successful close of socket
                if (rc == 0) {
                    // Transform state of game board from 2D to 1D
                    char boardState[9];
                    getBoardState(board, boardState);
                    int currentSeqNum = seqCount - 1;

                    // Call function to find new server
                    sockData = findNewServer(boardState, currentSeqNum);
                }
                continue;
            }

            struct ParsedMessage message = parseMessage(buf, bytesRead);

            if (!message.valid)
            {
                // if there's an invalid message, we just ignore it
                printf("Got an invalid message!\n");
                continue;
            }

            if (message.command == MOVE)
            {
                currentGame = message.currentGame;

                printf("Got move from server: '%d'\n", message.move);
                choice = message.move;
                valid = validateMove(choice, board);

                // validate the opponent's move
                if (valid == 1)
                {
                    // add message to history
                    messages[seqCount] = message;
                    // if move is valid, add it to the board
                    seqCount++;
                    struct Move move = getRowColFromMove(choice);
                    board[move.row][move.column] = mark;
                }
                else
                {
                    // if move is invalid, disconnect
                    printf("Invalid move from opponent.\n");
                    // if there's an invalid move, we just ignore it
                    continue;
                }
                // after a move, check to see if someone won! (or if there is a draw)
                winState = checkwin(board);
                // server won (or draw), send a game over command
                if (winState != -1)
                {
                    struct ParsedMessage newMessage;
                    newMessage.command = GAME_OVER;
                    newMessage.currentGame = currentGame;
                    newMessage.seqNum = seqCount;
                    char buf[5];
                    buildBuf(newMessage, buf);
                    messages[seqCount] = newMessage;

                    
                    rc = sendto(sockData.sock, buf, 5, 0, (struct sockaddr *) &sockData.my_addr, sockData.my_addr_len);
                }
            }
            // handle a game over command from server
            else if (message.command == GAME_OVER)
            {
                printf("Got confirmation of game over from server!\n");
                winState = 1;
            }
            // Mod math to figure out who the player is
        }
        if (winState == -1)
            player = getOpponent(player);
    } while (winState == -1); // -1 means no one won

    /* print out the board again */
    print_board(board);

    printf("Game over!\n");

    // end the current game
    return 0;
}

struct SocketData findNewServer(char boardState[9], int currentSeqNum){
    // Uses multicast to look for a new server
    printf("Finding new server\n");
    int rc;
    struct SocketData MC_SockData;
    struct SocketData sockData;

    MC_SockData.sock = socket(AF_INET, SOCK_DGRAM, 0);

    // Sets timeout for the multicast socket
    struct timeval tv;
    tv.tv_sec = FALLBACK_TIMEOUT;
    setsockopt(MC_SockData.sock, SOL_SOCKET, SO_RCVTIMEO,(char*)&tv,sizeof(tv));

    // Zero out information for the MC socket and rebuilds it
    bzero((char *)&MC_SockData.my_addr, sizeof(MC_SockData.my_addr));
    MC_SockData.my_addr.sin_family = AF_INET;
    MC_SockData.my_addr.sin_port = htons(MC_PORT);
    MC_SockData.my_addr.sin_addr.s_addr = inet_addr(MC_GROUP);
    sockData.my_addr_len = sizeof(struct sockaddr_in);
    MC_SockData.my_addr_len = sockData.my_addr_len;

    // Build message for requesting a resumed game and send it
    char newServerMsg[2];
    newServerMsg[0] = VERSION;
    newServerMsg[1] = RESUME;
    
    rc = sendto(MC_SockData.sock, newServerMsg, 2, 0, (struct sockaddr *) &MC_SockData.my_addr, sizeof(MC_SockData.my_addr));

    if (rc < 0) {
        perror("sendto error: ");
        printf("MC_SockData.sock: %d\n", MC_SockData.sock);
        for (int i = 0; i < 2; i++){
            printf("newServerMsg: %d\n", newServerMsg[i]);
        }
        exit(1);
    }

    // Get response from the multicast group
    char serverResp[3];
    rc = recvfrom(MC_SockData.sock, serverResp, 3, 0, (struct sockaddr *) &sockData.my_addr, &sockData.my_addr_len);
    if (rc < 0) {
        perror("error receiving new server info");
        sockData = findServerFromFile(boardState, currentSeqNum);
    }
    else {
        printf("Got new server info\n");

        // Re-combine the two bytes of the transmitted port and set the new socket to that port
        u_int16_t leftMost = (serverResp[1] & 0xFFFF) << 8;
        u_int16_t rightMost = (serverResp[2] & 0xFFFF);
        u_int16_t newPort = leftMost | (unsigned char) rightMost;
        sockData.my_addr.sin_port = htons(newPort);
    }
    
    // Connect to new socket
    sockData.sock = socket(AF_INET, SOCK_STREAM, 0);
    rc = connect(sockData.sock, (struct sockaddr *) &sockData.my_addr, sockData.my_addr_len);

    if (rc == -1){
        perror("Error connecting to new server");
        exit(0);
    }

    // Build and send game state message to the new server
    char resumeMsg[13];
    resumeMsg[0] = VERSION;
    resumeMsg[1] = RESUME;
    resumeMsg[2] = 0x00;
    resumeMsg[3] = 0x00;
    resumeMsg[4] = currentSeqNum;

    // Fills the resumeMsg with the characters for the board state
    for (int i = 5; i < 14; i++){
        resumeMsg[i] = boardState[i - 5];
    }

    rc = send(sockData.sock, resumeMsg, 14, 0);

    if (rc < 1){
        perror("Error sending existing game board");
    }

    return sockData;
}

struct SocketData createClientSocket(char ipAddr[15], int port)
{
    // function to simply creating a client socket, can create server or client socket
    // char[] - the ip address in dotted decimal format specify "any" for any
    // int port - the port

    int sock;
    struct sockaddr_in addr;

    printf("Ip: %s\n", ipAddr);
    printf("Port: %d\n", port);

    // build the address
    sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ipAddr);

    // connect to the server
    int connectResp;
    printf("Connecting...\n");
    connectResp = connect(
        sock,
        (struct sockaddr *)&addr,
        sizeof(struct sockaddr_in)
    );

    if (connectResp < 0)
    {
        close(sock);
        perror("error connecting to server");
        exit(EXIT_FAILURE);
    }

    // return info about the newly created socket
    struct SocketData sockData;
    sockData.sock = sock;
    sockData.my_addr = addr;
    sockData.my_addr_len = (socklen_t) sizeof(addr);

    return sockData;
}

// Function for reading IP address and port number from a file in case the 
// multicast request fails
struct SocketData findServerFromFile(char boardState[9], int currentSeqNum){
    char* filename = "./fallback.txt";
    FILE *file;
    struct SocketData sockData;

    file = fopen(filename, "r");

    if(file == NULL){
        perror("Error opening file");
        exit(0);
    }

    char ipAddr[256];
    if (fgets(ipAddr, 256, file) == NULL){
        perror("Error reading from file");
    }

    printf("Ip to connect to: %s\n", ipAddr);

    char newPort[256];
    if (fgets(newPort, 256, file) == NULL){
        perror("Error reading from file");
    }

    fclose(file);

    int portNum = atoi(newPort);

    printf("Port to connect to: %s\n", newPort);

    sockData.my_addr.sin_family = AF_INET;
    sockData.my_addr.sin_port = htons(portNum);
    sockData.my_addr.sin_addr.s_addr = inet_addr(ipAddr);
    sockData.my_addr_len = sizeof(sockData.my_addr);

    printf("Attempting to connect to fallback server from file\n");

    return sockData;
}