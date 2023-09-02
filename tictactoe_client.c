#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#define MAX 100
#define PORT 8080
#define SA struct sockaddr

char buff[MAX];
char sym;
struct timeval tv = { 15 , 0};
fd_set rd;

//Function to get a valid move from the user, return -1 on timeout
int get_valid_move(char* board) {
    int r,c;
    input :
    printf("Enter row , col : ");
    fflush(stdout);
    FD_ZERO(&rd);
    FD_SET(STDIN_FILENO, &rd);
    tv.tv_sec = 15;
    tv.tv_usec = 0;

    if(select(1+STDIN_FILENO, &rd, 0, 0, &tv) > 0)
        scanf("%d %d",&r,&c);
    else
        return -1;

    if(r<1 || r>3 || c<1 || c>3) {
        printf("\nInvalid move! 1 <= row,col <= 3, try again\n");
        goto input;
    }
    r--;
    c--;
    int m = r*3 + c;
    if(board[m]=='X' || board[m]=='O') {
        printf("\nInvalid move! Move must be on an empty cell. Try again\n");
        goto input;
    }
    board[m] = sym;
    return m;
}

//Function to render the current board state
void render(char* board) {
    for(int i=0;i<9;i++) {
        if(board[i]=='*')
            board[i]=' ';
    }
    printf(" %c | %c | %c \n",board[0],board[1],board[2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n",board[3],board[4],board[5]);
    printf("-----------\n");
    printf(" %c | %c | %c \n",board[6],board[7],board[8]);

}

void start_game(int sockfd) {

    int player_id;
    read(sockfd,&player_id,4);
    printf("Your player ID is %d\n",player_id);

    while(1) {
        int check = 0;
        //Read messages from server
        read(sockfd,buff,sizeof(buff));
        if(strncmp(buff,"W",1) == 0) {  //Wait for partner request
            printf("Waiting for a partner to join . . .\n\n");
        }
        else if(strncmp(buff,"X",1)==0) { //Symbol X allocation
            int opponent_id;
            char ch;
            sscanf(buff,"%c %d",&ch,&opponent_id);
            printf("Your partner’s ID is %d\n",opponent_id);
            printf("Your symbol is X\n");
            sym = 'X';
        }
        else if(strncmp(buff,"O",1)==0) { //Symbol O allocation
            int opponent_id;
            char ch;
            sscanf(buff,"%c %d",&ch,&opponent_id);
            printf("Your partner’s ID is %d\n",opponent_id);
            printf("Your symbol is O\n");
            sym = 'O';
        }
        else if(strncmp(buff,"S",1)==0) { //Start game
            printf("Starting game...\n\n");
        }
        else if(strncmp(buff,"M",1)==0) { //Request to make a move
            char board[10];
            char ch;
            sscanf(buff,"%c %s",&ch,board);
            printf("\nYour opponent played : \n");
            render(board);
            int move = get_valid_move(board);
            printf("\nYou played : \n");
            render(board);
            write(sockfd,&move,4);
        }
        else if(strncmp(buff,"F",1)==0) { //Request to make the first move
            char board[10];
            for(int i=0;i<9;i++)    board[i]='*';
            char ch;
            printf("\nYour turn to play : \n");
            render(board);
            int move = get_valid_move(board);
            printf("\nYou played : \n");
            render(board);
            write(sockfd,&move,4);
        }
        else if(strncmp(buff,"V",1)==0) { //Victory
            printf("\nGame Over. You Won!\n\n");
            check=1;
        }
        else if(strncmp(buff,"D",1)==0) { //Defeat
            char ch;
            char board[10];
            sscanf(buff,"%c %s",&ch,board);
            printf("\nYour opponent played : \n");
            render(board);
            printf("\nGame Over. Defeat :\(\n\n");
            check=1;
        }
        else if(strncmp(buff,"T",1)==0) { //Tie
            char ch;
            char board[10];
            sscanf(buff,"%c %s",&ch,board);
            printf("\nFinal state : \n");
            render(board);
            printf("\nGame Over. Its a tie.\n\n");
            check=1;
        }
        else if(strncmp(buff,"E",1)==0) { //Exit
            printf("Session ended. Disconnected from server\n");
            close(sockfd);
            return;
        }
        else if(strncmp(buff,"I",1)==0) { //Timeout by user
            printf("You took too long to respond.\n");
            check = 1;
        }
        else if(strncmp(buff,"J",1)==0) { //Timeout by partner
            printf("Opponent took too long to respond.\n");
            check = 1;
        }
        if(check) { //Checking for continuing game
            char cont;
            printf("\nDo you wish to play again ? [1 for yes, no otherwise]\n");
            FD_ZERO(&rd);
            FD_SET(STDIN_FILENO, &rd);
            tv.tv_sec = 15;
            tv.tv_usec = 0;
            int ok = 0;

            if(select(1+STDIN_FILENO, &rd, 0, 0, &tv) > 0) {
                scanf("%d",&ok);
            }

            write(sockfd,&ok,4);
        }
    }
}

int main(int argc, char** argv)
{
	int sockfd, connfd;
	struct sockaddr_in servaddr, cli;
	char* ip = argv[1];

	if(argc != 2) {
        printf("Too few or too many arguments\nUsage : ./<program name> <ip>\n");
        return 0;
    }


    //Socket creation
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("Socket creation failed\n");
		exit(0);
	}

	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(ip);
	servaddr.sin_port = htons(PORT);

	//Connecting to game server
	if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
		printf("Connection with the server failed\n");
		exit(0);
	}
	else
		printf("Connected to the game server\n");

    start_game(sockfd);
    return 0;
}
