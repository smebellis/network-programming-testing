/* 
NAME: Ryan Ellis
CLASS: CSCI 3800
STUDENT NUMBER: 109576156
ASSIGNMENT: Final Project
*/

/* include files go here */
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <time.h>
#include <memory.h>

/* Define Terms for Protocol*/
#define ROWS 3
#define COLUMNS 3
#define SOCKETERROR -1
#define TIMETOWAIT 10
#define VERSION 5
#define NEWGAME 0
#define CONTINUEGAME 1
#define GAMEOVER 2
#define RESUME 3
#define PACKETSIZE 5

/* Define terms for multicast */
#define MC_PORT 1818
#define MC_GROUP "239.0.0.1"

struct gamePacket
{
  char version;
  char command;
  char move;
  char game;
  char sequence;
};

struct multiGamePacket
{
  char version;
  char command;
  char move;
  char game;
  char sequence;
  char board[ROWS][COLUMNS];
};

struct lostConnection
{
  char version;
  char command;
};

struct multicastPacket
{
  char version;
  short portNumber;
};

/***********************************************************/
/*                 Declare Functions                       */
/***********************************************************/

int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS]);
int tictactoe(char board[ROWS][COLUMNS], int Socket, int playerNumber, struct sockaddr_in *toAddress);
int initSharedState(char board[ROWS][COLUMNS]);
int checkErrors(int exp, const char *msg);
void wargames();
int isSquareOccupied(int choice, char board[ROWS][COLUMNS]);
int sendMoveToServer(int Socket, struct sockaddr_in *toAddress, struct gamePacket *packetOut, char board[ROWS][COLUMNS]);
int recvMoveFromServer(int Socket, struct sockaddr_in *toAddress, struct gamePacket *packetIn, struct gamePacket *packetOut, char board[ROWS][COLUMNS]);
int createSocket(char *ipAddress, int portno, struct sockaddr_in *toAddress, int *sock);
int createMulticastSocket(struct sockaddr_in *toAddress, char board[ROWS][COLUMNS], struct gamePacket *packetOut);
int readIPAddrFromFile(struct sockaddr_in *toAddress, int *sock);

int main(int argc, char *argv[])
{
  int clientSocket;
  char board[ROWS][COLUMNS];
  struct sockaddr_in toAddress;
  int port;
  int playerNumber = 2;

  if (argc < 3)
  {
    printf("Usage is: ./tictactoeClient <ipaddress> <port number> \n");
    exit(1);
  }

  port = atoi(argv[2]);

  /***********************************************************/
  /*                 Create the socket                       */
  /***********************************************************/

  checkErrors(createSocket(argv[1], port, &toAddress, &clientSocket), "Cannot establish Socket");

  /***********************************************************/
  /*               Start the game                            */
  /***********************************************************/
  printf("Starting the game...\n\n");

  initSharedState(board); // Initialize the 'game' board

  tictactoe(board, clientSocket, playerNumber, &toAddress); // call the 'game'

  return 0;
}

int checkErrors(int exp, const char *msg)
{
  if (exp == SOCKETERROR)
  {
    perror(msg);
    exit(1);
  }
  return exp;
}

int tictactoe(char board[ROWS][COLUMNS], int Socket, int playerNumber, struct sockaddr_in *toAddress)
{

  int player = 1; // keep track of whose turn it is
  int i;
  unsigned int choice; // used for keeping track of choice user makes
  int row, column;
  int rc;                                // Return Code for Error Checking
  char mark;                             // either an 'x' or an 'o'
  char junk[100];                        //For storing invalid input from user
  struct gamePacket packetIn, packetOut; //Stores game info from client and server
  int currentGame, sequenceNum;
  size_t packetLength = sizeof(struct gamePacket);

  /******************************************************************/
  /*                 Initialize Packout                             */
  /*                 to start New game                              */
  /******************************************************************/

  packetOut.version = VERSION;
  packetOut.command = NEWGAME;
  packetOut.move = 0;
  packetOut.game = 0;
  packetOut.sequence = 0;

  rc = write(Socket, &packetOut, packetLength);
  if (rc <= 0)
  {
    perror("[TICTACTOE]\n Error sending 1st packet");
    exit(2);
  }

  printf("###NEW GAME REQUEST###\n[CLIENT]\nVersion: %x\nCommand: %x\nMove: %x\nGame Number: %x\nSequence Number: %x\n", packetOut.version, packetOut.command, packetOut.move, packetOut.game, packetOut.sequence);

  /******************************************************************/
  /*               Gameplay: this allows a player to chose          */
  /*           its move and it sends/receives moves to server       */
  /******************************************************************/

  do
  {
    print_board(board);            // call function to print the board on the screen
    player = (player % 2) ? 1 : 2; // Mod math to figure out who the player is
    if (player == playerNumber)    //If it is player 2's turn then send their move to Player 1
    {
      do
      {
        printf("Player %d, enter a number:  ", player); // Get input from player 2 to start their turn
        rc = scanf("%d", &choice);                      //Store their move
        if (rc == 0)
        {
          choice = 0; //reset choice to get rid of bad input
          printf("Error: Bad input....\n");
          rc = scanf("%s", junk); //Store the bad input if characters were entered

          if (rc == 0) //If rc == 0 then the user entered more than 100 bad characters
          {
            printf("Error: Garbage input");
            return 1;
          }
        }
      } while ((choice < 1) || (choice > 9) || (isSquareOccupied(choice, board))); //Checks to make sure player entered valid input, if not requires new input

      packetOut.version = VERSION;
      packetOut.command = CONTINUEGAME;
      packetOut.move = (choice);
      packetOut.game = currentGame;
      packetOut.sequence = sequenceNum;

      sendMoveToServer(Socket, toAddress, &packetOut, board);
    }
    else
    {

      choice = recvMoveFromServer(Socket, toAddress, &packetIn, &packetOut, board);
      currentGame = packetIn.game;
      sequenceNum = packetIn.sequence; //sets sequence number to the squence from the servers packet
      sequenceNum++;                   //Increments the sequence number for the packet to be sent
    }

    mark = (player == 1) ? 'X' : 'O'; //depending on who the player is, use X for player 1 or O for player 2

    /******************************************************************/
    /** little math here. you know the squares are numbered 1-9, but  */
    /* the program is using 3 rows and 3 columns. We have to do some  */
    /* simple math to conver a 1-9 to the right row/column            */
    /******************************************************************/

    row = (int)((choice - 1) / ROWS);
    column = (choice - 1) % COLUMNS;

    /* first check to see if the row/column chosen has a digit in it */
    /* if square 8 has an '8' then it is a valid choice              */

    if (board[row][column] == (choice + '0'))
    {
      board[row][column] = mark;
    }

    /* after a move, check to see if someone won! (or if there is a draw */
    i = checkwin(board);

    player++;
  } while (i == -1); // -1 means no one won

  /* print out the board again */
  //print_board(board);

  if (i == 1)
  { // means a player won!! congratulate them
    printf("==>\aPlayer %d wins\n\n", --player);

    if (player == 2)
    {
      packetOut.command = GAMEOVER;
      printf("[CLIENT]\nWaiting for GAMEOVER from SERVER\n\n");
      recvMoveFromServer(Socket, toAddress, &packetIn, &packetOut, board);
    }
    else
    {
      packetOut.command = GAMEOVER;
      packetOut.sequence = packetOut.sequence + 1;
      printf("[CLIENT]\nSending GAMEOVER to Server\n\n");
      sendMoveToServer(Socket, toAddress, &packetOut, board);
    }
  }
  else
  {
    printf("==>\aGame draw\n"); // ran out of squares, it is a draw
    sleep(1);
    system("clear");
    printf("Greetings Professor Falken\n\nA strange game.\nThe only winning move is\nnot to play\n\n");
    char buffer[] = {"How about a nice game of Chess?"};
    for (int i = 0; i < sizeof(buffer); i++)
    {
      printf("%c", buffer[i]);
    }
    printf("\n");
  }
  return 0;
}

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

void print_board(char board[ROWS][COLUMNS])
{
  /*****************************************************************/
  /* brute force print out the board and all the squares/values    */
  /*****************************************************************/

  printf("\n\n\n\tCurrent TicTacToe Game\n\n");

  printf("Player 1 (X)  -  Player 2 (O)\n\n\n");

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
  /*********************************/
  /* Initializes the game board    */
  /*********************************/

  int i, j, count = 1;
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
    {
      board[i][j] = count + '0';
      count++;
    }

  return 0;
}

int isSquareOccupied(int choice, char board[ROWS][COLUMNS])
{
  /******************************************************************/
  /*               Checks if a square is occupied                   */
  /******************************************************************/

  int row, column;

  row = (int)((choice - 1) / ROWS);
  column = (choice - 1) % COLUMNS;

  if (board[row][column] == (choice + '0'))
  {
    return 0;
  }
  else
  {
    printf("Error: Invalid move\n");
    return 1;
  }
}

void wargames()
{
  /******************************************************************/
  /*               Easter Egg:  Function is not used                */
  /******************************************************************/
  system("clear");
  printf("\n\n\nShall We Play a Game?\n\n");
  printf("\n\n\nBridge\nCheckers\nChess\nPoker\nFighter Combat\nGuerrilla Engagement\nDesert Warfare\nAir-To-Ground Actions\nTheaterwide Tactical Warfare\nTheaterwide BioToxic and Chemical Warfare\n\nGlobal Thermonuclear War\n\n");
  sleep(3);
}

int sendMoveToServer(int Socket, struct sockaddr_in *toAddress, struct gamePacket *packetOut, char board[ROWS][COLUMNS])
{
  /******************************************************************/
  /*               Sends the packet to the server                   */
  /******************************************************************/
  int rc;
  size_t packetLength = sizeof(struct gamePacket);

  rc = write(Socket, packetOut, packetLength);
  if (rc <= 0)
  {
    perror("[SEND]\n Error sending packet");
    createMulticastSocket(toAddress, board, packetOut);
    //exit(2);
  }

  printf("[CLIENT]\nVersion: %x\nCommand: %x\nMove: %x\nGame Number: %x\nSequence Number: %x\n\n", packetOut->version, packetOut->command, packetOut->move, packetOut->game, packetOut->sequence);

  return 0;
}

int recvMoveFromServer(int Socket, struct sockaddr_in *toAddress, struct gamePacket *packetIn, struct gamePacket *packetOut, char board[ROWS][COLUMNS])
{
  /******************************************************************/
  /*               Receives the packet to the server                */
  /******************************************************************/

  int rc, serverMove;
  size_t packetLength = sizeof(struct gamePacket);

  rc = read(Socket, packetIn, packetLength);
  if (rc <= 0)
  {
    //perror("[RECV]\n Error receiving packet");
    printf("[CONNECTION]\nLost Connection with Server\n\n");
    createMulticastSocket(toAddress, board, packetOut);
    //exit(1);
  }

  /***********************************************************************/
  /*            Checks the packet for the correct                        */
  /*                           elements                                  */
  /***********************************************************************/

  if (packetIn->version != VERSION) //Checks for correct version, if not, terminate connection
  {
    printf("Incorrect Version Number...Terminating Connection\n");
    exit(1);
  }
  else if (packetIn->command == GAMEOVER)
  {
    printf("[SERVER]\nGAME OVER...Exiting Game\n");
    packetOut->command = GAMEOVER;
    write(Socket, packetOut, packetLength);
    exit(2);
  }
  else if (packetIn->command != CONTINUEGAME) //Checks if the command is a move command, if not terminate connection
  {
    printf("Expected a move command...Terminating Connection\n");
    exit(3);
  }
  else
  {
    printf("[SERVER]\nVersion: %d\nCommand: %d\nMove: %d\nGame: %d\nSequence: %d\n", packetIn->version, packetIn->command, packetIn->move, packetIn->game, packetIn->sequence);
  }

  serverMove = (int)packetIn->move;

  return serverMove;
}

int createSocket(char *ipAddress, int portNumber, struct sockaddr_in *toAddress, int *sock)
{

  /******************************************************************/
  /*               Creates the socket and connects to it            */
  /******************************************************************/

  int rc; //Used for error handling

  //struct sockaddr_in fromAddress;

  socklen_t length = sizeof(struct sockaddr_in);

  if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("[CONNECTION]\n Opening Socket");
    exit(1);
  }

  // fromAddress.sin_family = AF_INET;
  // fromAddress.sin_port = htons(portNumber);
  // fromAddress.sin_addr.s_addr = INADDR_ANY;

  inet_pton(AF_INET, ipAddress, &(toAddress->sin_addr));
  toAddress->sin_family = AF_INET;
  toAddress->sin_port = htons(portNumber);

  rc = connect(*sock, (struct sockaddr *)toAddress, length);
  if (rc < 0)
  {
    perror("[CONNECTION]\n Connection Error");
    exit(1);
  }

  // if (bind(*sock, (struct sockaddr *)&fromAddress, sizeof(fromAddress)) < 0)
  // {
  //   perror("ERROR");
  //   exit(5);
  // }

  return 0;
}

int createMulticastSocket(struct sockaddr_in *toAddress, char board[ROWS][COLUMNS], struct gamePacket *packetOut)
{

  /*********************************************************/
  /*          Creates Multicast and New TCP socket         */
  /*           then connects. Will initiate FAILSAFE       */
  /*             if no connection established              */
  /*********************************************************/

  int multicastSocket, tcpSocket, portNum, rc, cnt, connection;
  struct sockaddr_in multiAddr;
  struct multicastPacket multiPacketIn;
  struct lostConnection resumePacketOut;
  socklen_t addrLen;
  struct ip_mreq mreq;
  char ipAddr[INET_ADDRSTRLEN];
  struct gamePacket gamePacket;
  struct multiGamePacket fullPacket;

  /*****************************/
  /*      Set up Timeout       */
  /*****************************/

  struct timeval tv;
  int count = 0;

  tv.tv_sec = TIMETOWAIT;
  tv.tv_usec = 0;

  /*****************************/
  /*      Create Multicast     */
  /*          socket           */
  /*****************************/

  multicastSocket = socket(AF_INET, SOCK_DGRAM, 0);
  if (multicastSocket < 0)
  {
    perror("multicast socket");
    exit(1);
  }

  /**********************************/
  /*          Create TCP            */
  /*            Socket              */
  /**********************************/

  tcpSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (setsockopt(multicastSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)))
  {
    perror("Error");
    exit(1);
  }

  /*******************************/
  /*            Create           */
  /*       Multicast Address     */
  /*******************************/

  bzero((char *)&multiAddr, sizeof(multiAddr));
  multiAddr.sin_family = AF_INET;
  multiAddr.sin_port = htons(MC_PORT);
  multiAddr.sin_addr.s_addr = inet_addr(MC_GROUP);
  addrLen = sizeof(struct sockaddr_in);

  /**************************/
  /*    Resume Connection   */
  /*         Packet         */
  /**************************/

  resumePacketOut.version = VERSION;
  resumePacketOut.command = RESUME;

  /**********************************/
  /*            Send               */
  /*       multicast packet         */
  /**********************************/

  rc = sendto(multicastSocket, &resumePacketOut, sizeof(resumePacketOut), 0, (struct sockaddr *)&multiAddr, addrLen);
  if (rc <= 0)
  {
    perror("SEND TO");
    readIPAddrFromFile(toAddress, &tcpSocket);
    //exit(1);
  }
  else
  {
    printf("[MULTICAST CLIENT]\nBroadcast to Multicast Group\nVersion: %x\nCommand: %x\n\n", resumePacketOut.version, resumePacketOut.command);
  }

  /**********************************/
  /*           Receive              */
  /*       multicast packet         */
  /**********************************/

  do
  {
    cnt = recvfrom(multicastSocket, &multiPacketIn, sizeof(multiPacketIn), 0, (struct sockaddr *)toAddress, &addrLen);
    if (cnt <= 0)
    {
      printf("[TIMEOUT] No Multicast Servers Available...resending\n\n");
      sendto(multicastSocket, &resumePacketOut, sizeof(resumePacketOut), 0, (struct sockaddr *)&multiAddr, addrLen);
      printf("[MULTICAST CLIENT]\n[RESEND #%d]\nVersion: %x\nCommand: %x\n\n", (count + 1), resumePacketOut.version, resumePacketOut.command);
      count++;
      if (count == 3)
      {
        printf("[FAILSAFE]\nReconnect Attempts EXCEEDED...\n\n");
        connection = readIPAddrFromFile(toAddress, &tcpSocket);
        if (connection == -1)
        {
          printf("[CONNECTION]\n\n**********\nNO AVAILABLE SERVERS\n**********\n");
          exit(1);
        }
        else
        {
          break;
        }
      }
    }
    else
    {
      portNum = ntohs(multiPacketIn.portNumber); //From multicast packet recv
      toAddress->sin_port = htons(portNum);
      inet_ntop(AF_INET, &(toAddress->sin_addr), ipAddr, INET_ADDRSTRLEN);

      printf("[MULTICAST SERVER]\nReceived a Reply from IP: %s\nPort Number: %d\n\n", ipAddr, portNum);

      //exit(1);
    }
  } while (cnt <= 0);

  /***********************/
  /*    Connect to new   */
  /*     Address/Port    */
  /***********************/

  if (connection == 0)
  {
    printf("[TCP CONNECTED]\n");
  }
  else
  {
    rc = connect(tcpSocket, (struct sockaddr *)toAddress, addrLen);
    if (rc < 0)
    {
      perror("Connect");
      exit(1);
    }
  }

  /*****************************/
  /*          Create           */
  /*    Established Packet     */
  /*****************************/

  fullPacket.version = VERSION;
  fullPacket.command = CONTINUEGAME;
  fullPacket.move = packetOut->command;
  fullPacket.game = packetOut->game;
  fullPacket.sequence = packetOut->sequence;
  fullPacket.board[0][0] = board[0][0];
  fullPacket.board[0][1] = board[0][1];
  fullPacket.board[0][2] = board[0][2];
  fullPacket.board[1][0] = board[1][0];
  fullPacket.board[1][1] = board[1][1];
  fullPacket.board[1][2] = board[1][2];
  fullPacket.board[2][0] = board[2][0];
  fullPacket.board[2][1] = board[2][1];
  fullPacket.board[2][2] = board[2][2];

  printf("[TCP CLIENT]\nVersion: %x\nCommand: %x\nMove: %x\nGame: %x\nSequence: %x\nBoard: %c %c %c %c %c %c %c %c %c\n\n", fullPacket.version, fullPacket.command, fullPacket.move, fullPacket.game, fullPacket.sequence,
         fullPacket.board[0][0], fullPacket.board[0][1], fullPacket.board[0][2],
         fullPacket.board[1][0], fullPacket.board[1][1], fullPacket.board[1][2],
         fullPacket.board[2][0], fullPacket.board[2][1], fullPacket.board[2][2]);

  /***********************/
  /*    Send new packet  */
  /***********************/

  rc = write(tcpSocket, &fullPacket, sizeof(fullPacket));
  if (rc < 0)
  {
    perror("Write");
    exit(1);
  }

  return 0;
}

int readIPAddrFromFile(struct sockaddr_in *toAddress, int *sock)
{
  /*********************************************************/
  /*          Reads IP address and Port Number             */
  /*          from file and attempts to connect            */
  /*********************************************************/

  FILE *fileptr;
  char filename[100];
  char ipaddress[INET_ADDRSTRLEN];
  int port, rc;
  socklen_t addrLen = sizeof(struct sockaddr_in);

  memset(ipaddress, 0, INET_ADDRSTRLEN);

  strcpy(filename, "backup_connections.txt");
  fileptr = fopen(filename, "r");

  if (fileptr == NULL)
  {
    perror("FILE OPEN");
    exit(1);
  }

  /*****************************/
  /*        Loop to            */
  /*   read IP's from file     */
  /*****************************/

  while (fscanf(fileptr, "%s"
                         "%d",
                ipaddress, &port) == 2)
  {
    printf("[FILE]:\nIP Addr: %s\nPort: %d\n\n", ipaddress, port);
    toAddress->sin_port = htons(port);
    inet_pton(AF_INET, ipaddress, &(toAddress->sin_addr));
    rc = connect(*sock, (struct sockaddr *)toAddress, addrLen);
    if (rc == 0) //Connection was good, now break out of while loop
    {
      break;
    }
    else //If rc is < 0 no connection established, then continue loop
    {
      continue;
    }
  }

  fclose(fileptr);

  return rc;
}