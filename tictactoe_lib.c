#include "tictactoe_lib.h"

int checkwin(char board[ROWS][COLUMNS])
{
    /************************************************************************/
    /* brute force check to see if someone won, or if there is a draw       */
    /* return a 0 if the game is 'over' and return -1 if game should go on  */
    /************************************************************************/
    if (board[0][0] == board[0][1] && board[0][1] == board[0][2]) // row matches
        return 1;

    else if (board[1][0] == board[1][1] && board[1][1] == board[1][2]) // row matches
        return 1;

    else if (board[2][0] == board[2][1] && board[2][1] == board[2][2]) // row matches
        return 1;

    else if (board[0][0] == board[1][0] && board[1][0] == board[2][0]) // column
        return 1;

    else if (board[0][1] == board[1][1] && board[1][1] == board[2][1]) // column
        return 1;

    else if (board[0][2] == board[1][2] && board[1][2] == board[2][2]) // column
        return 1;

    else if (board[0][0] == board[1][1] && board[1][1] == board[2][2]) // diagonal
        return 1;

    else if (board[2][0] == board[1][1] && board[1][1] == board[0][2]) // diagonal
        return 1;

    else if (board[0][0] != '1' && board[0][1] != '2' && board[0][2] != '3' &&
             board[1][0] != '4' && board[1][1] != '5' && board[1][2] != '6' &&
             board[2][0] != '7' && board[2][1] != '8' && board[2][2] != '9')

        return 0; // Return of 0 means game over
    else
        return -1; // return of -1 means keep playing
}

char getMark(int playerNumber){
    // returns the char corresponding to each player
    return (playerNumber == 1) ? 'X' : 'O';
}

int getOpponent(int playerNumber){
    // ugly mod-math to get opponent from current player
    return ((playerNumber + 1) % 2) ? 1 : 2;
}

struct Move getRowColFromMove(int choice){
    //takes move value 1-9 as input, returns a struct contain row and column for 2d array
    struct Move move;

    int row = (int)((choice - 1) / ROWS);
    int column = (choice - 1) % COLUMNS;

    move.row = row;
    move.column = column;

    return move;
}

void print_board(char board[ROWS][COLUMNS])
{
    /*****************************************************************/
    /* brute force print out the board and all the squares/values    */
    /*****************************************************************/

    printf("     |     |     \n");
    printf("  %c  |  %c  |  %c \n", board[0][0], board[0][1], board[0][2]);

    printf("_____|_____|_____\n");
    printf("     |     |     \n");

    printf("  %c  |  %c  |  %c \n", board[1][0], board[1][1], board[1][2]);

    printf("_____|_____|_____\n");
    printf("     |     |     \n");

    printf("  %c  |  %c  |  %c \n", board[2][0], board[2][1], board[2][2]);

    printf("     |     |     \n\n");
}

int initSharedState(char board[ROWS][COLUMNS])
{
    /* this just initializing the shared state aka the board */
    int i, j, count = 1;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
        {
            board[i][j] = count + '0';
            count++;
        }

    return 0;
}

int validateMove(int choice, char board[ROWS][COLUMNS])
{
    /******************************************************************/
    /** little math here. you know the squares are numbered 1-9, but  */
    /* the program is using 3 rows and 3 columns. We have to do some  */
    /* simple math to conver a 1-9 to the right row/column            */
    /******************************************************************/

	//checks if choice is a valid number
    if(choice < 1 || choice > 9){
        return 0;
    }
    int row = (int)((choice - 1) / ROWS);
    int column = (choice - 1) % COLUMNS;

    /* first check to see if the row/column chosen is has a digit in it, if it */
    /* square 8 has and '8' then it is a valid choice                          */

    if (board[row][column] == (choice + '0'))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

struct ParsedMessage parseMessage(char *buf, int length)
{
    // parsing and validation of bufs according to protocol
    struct ParsedMessage temp;

    // length of a valid message is max 40 bytes
    if (length > 40)
    {
        printf("length of a valid message is max 40 bytes\n");
        temp.valid = 0;
        return temp;
    }

    // only works with same correct protocol version for program
    if (buf[0] != VERSION)
    {
        printf("program only works with programs of same protocol version\n");
        temp.valid = 0;
        return temp;
    }

    // command will be 0 for new game, 1 for a move or 2 for a game over
    if(buf[1] < 0 || buf[1] > 2)
    {
        printf("invalid value for command\n");
        temp.valid = 0;
        return temp;
    }
    else
    {
        temp.command = buf[1];
    }

    // check if command is a move
    if (temp.command == 1)
    {
        // check whether move is valid
        if (buf[2] < 1 || buf[2] > 9)
        {
            printf("invalid number value for move\n");
            temp.valid = 0;
            return temp;
        }
        else
        {
            // get move from message
            temp.move = buf[2];
        }
    }

    // if command is not new game, should include current game
    if(temp.command != NEW_GAME){
        temp.currentGame = buf[3];
        // currentGame must be positive
        // it can't be more than 255 because that's the largest number
        // that will fit in one byte
        if(temp.currentGame < 0 || temp.currentGame > 255){
            printf("value for current game # must be between 0 and 255\n");
            temp.valid = 0;
            return temp;
        }
    }

    //if the sequence number for a new game is not zero, there is an error
    temp.seqNum = buf[4];
    if (temp.command == NEW_GAME && temp.seqNum != 0) {
        printf("Invalid sequence number for new game (Not 0)\n");
        temp.valid = 0;
        return temp;
    }

    temp.valid = 1;

    return temp;
}

void buildBuf (struct ParsedMessage parsedMessage, char buf[5]) {
    //convert a parsed message into a datagram for sending
    buf[0] = VERSION;
    buf[1] = parsedMessage.command;
    buf[2] = parsedMessage.move;
    buf[3] = parsedMessage.currentGame;
    buf[4] = parsedMessage.seqNum;
}