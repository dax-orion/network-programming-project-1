# Final Project - Network AI Server Tic-Tac-Toe with multi-cast
A Tic-Tac-Toe Game game between a client and an AI server that can handle multiple clients and a client capable of recovering using multi-cast.

## Created by:
Dax Hurley, Hunter Culler
## Usage

    tictactoe_server <port>
    tictactoe_client <port> <ip>

Once the game starts you will be prompted to enter a number correspond to your desired move. The computer will send back a move. After 25 seconds client will timeout while waiting for a response.

## Compilation
To compile on Linux use:

    make server
    make client

To delete the compiled program use:

    make clean