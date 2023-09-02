#include <stdio.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/select.h>
#define MAX 80
#define PORT 8080
#define SA struct sockaddr
#define MAX_GAMES 100

FILE* fd;
unsigned int player_id = 1;
unsigned int game_id = 1;
int sockfd;
pthread_mutex_t mutex;

void stop_server() {
    // Function used as interrupt handler to handle and
    // close global variables of the server when it is stopped

    printf("Stopping server\n");
    fclose(fd);
    close(sockfd);
    pthread_mutex_destroy(&mutex);
    exit(0);
}

//Check if the current board state corresponds to a final state in the game
char game_over(char* board) {
    if( (board[0]!='*' && board[0]==board[1] && board[1]==board[2]) ) return board[0];
    if( (board[3]!='*' && board[3]==board[4] && board[4]==board[5]) ) return board[3];
    if( (board[6]!='*' && board[6]==board[7] && board[7]==board[8]) ) return board[6];
    if( (board[0]!='*' && board[0]==board[3] && board[3]==board[6]) ) return board[0];
    if( (board[1]!='*' && board[1]==board[4] && board[4]==board[7]) ) return board[1];
    if( (board[2]!='*' && board[2]==board[5] && board[5]==board[8]) ) return board[2];
    if( (board[0]!='*' && board[0]==board[4] && board[4]==board[8]) ) return board[0];
    if( (board[2]!='*' && board[2]==board[4] && board[4]==board[6]) ) return board[2];

    int cnt = 0;
    for(int i=0;i<9;i++) {
        if(board[i]=='*')
            cnt++;
    }
    return cnt>0?0:1;
}

void *start_game(void *conndata) {
    int* client_sockfd = (int*) conndata;
    int pid1 = client_sockfd[2];
    int pid2 = client_sockfd[3];
    int gid;
    int winner;

    char board[10];
    int redo;
    struct timeval tv;
    struct timespec t_start,t_end;
    fd_set rd;
    int moveseq[10];
    int moveptr = 1;

    //Game starts
    do {
        moveptr = 1;
        pthread_mutex_lock(&mutex);
        gid = game_id++;
        pthread_mutex_unlock(&mutex);

        write(client_sockfd[0],"S",1);
        write(client_sockfd[1],"S",1);
        sleep(1);
        clock_gettime(CLOCK_MONOTONIC,&t_start);

        //Initialize board
        for(int i=0;i<9;i++) {
            board[i]='*';
        }
        board[9]=0;

        int cur_player = 1;

        //First move
        write(client_sockfd[0],"F",1);
        int move;

        char status = 0;

        read(client_sockfd[0],&move,4);
        if(move != -1)
            board[move] = 'X';
        else {
            status = 2;
            cur_player = 0;
        }
        moveseq[0] = move;

        //While game is not over, ask for moves and update game state
        while( status == 0 ) {
            char msg[30];
            sprintf(msg,"M %s",board);
            write(client_sockfd[cur_player],msg,strlen(msg));

            read(client_sockfd[cur_player],&move,4);
            moveseq[moveptr++] = move;
            if(move == -1) {
                status = 2;
                break;
            }

            board[move] = cur_player == 0 ? 'X' : 'O';
            cur_player = 1 - cur_player;
            status = game_over(board);
        }
        if(status == 'X') { //X player wins
            char msg[30];
            sprintf(msg,"D %s",board);
            write(client_sockfd[0],"V",1);
            write(client_sockfd[1],msg,strlen(msg));
            winner = pid1;
        }
        else if(status == 'O') { //O player wins
            char msg[30];
            sprintf(msg,"D %s",board);
            write(client_sockfd[0],msg,strlen(msg));
            write(client_sockfd[1],"V",1);
            winner = pid2;
        }
        else if(status == 1) { // Tie
            char msg[30];
            sprintf(msg,"T %s",board);
            write(client_sockfd[0],msg,strlen(msg));
            write(client_sockfd[1],msg,strlen(msg));
            winner = -1;
        }
        else if(status == 2) { // Timeout
            write(client_sockfd[cur_player],"I",1);
            write(client_sockfd[1-cur_player],"J",1);
        }
        int r1 = 0, r0 = 0;

        clock_gettime(CLOCK_MONOTONIC,&t_end);

        double duration = (t_end.tv_sec - t_start.tv_sec) + (t_end.tv_nsec - t_start.tv_nsec)*1e-9;

        //Asking for continuing game
        read(client_sockfd[0],&r0,4);
        read(client_sockfd[1],&r1,4);
        redo = r0&r1;

        //Logging game stats to file
        pthread_mutex_lock(&mutex);
        fprintf(fd,"%d, %d, %d, %.6f, %d, [",gid,pid1,pid2,duration,winner);
        for(int i=0;i+1<moveptr;i++) {
            fprintf(fd,"%d, ",moveseq[i]);
        }
        fprintf(fd,"%d]\n",moveseq[moveptr-1]);
        pthread_mutex_unlock(&mutex);

    }while(redo==1);


    //Exit disconnecting the clients
    write(client_sockfd[0],"E",1);
    write(client_sockfd[1],"E",1);
    close(client_sockfd[0]);
    close(client_sockfd[1]);

    pthread_exit(0);
}

//Main method
int main() {

	int len;
	struct sockaddr_in servaddr,cli;

	//Setup log file
	fd = (FILE*)fopen("server-log.txt","w");
	fprintf(fd,"GameID, Player1ID, Player2ID, Duration, Winner, MoveSequence\n");

	//Socket creation
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("Socket creation failed\n");
		exit(0);
	}

    pthread_mutex_init(&mutex, 0);

    signal(SIGINT, stop_server);
	bzero(&servaddr, sizeof(servaddr));

    //Setting up game server
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);

	if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
		printf("Socket bind failed\n");
		exit(0);
	}

    printf("Game server started. Waiting for players.\n");

    //Infinite loop to accept incoming connections
    while(1) {

        int *client_data = (int *)malloc(4*sizeof(int));

        //Connecting with first client
        if ((listen(sockfd, 5)) != 0) {
            printf("Listen failed\n");
            exit(0);
        }

        client_data[0] = accept(sockfd, (SA*)&cli, &len);
        if (client_data[0] < 0) {
            printf("Server accept failed\n");
            exit(0);
        }

        //Making client 1 wait till next client connects
        write(client_data[0],&player_id,4);
        client_data[2] = player_id;
        player_id ++;

        write(client_data[0],"W",1);

        //Connecting with second client
        if ((listen(sockfd, 5)) != 0) {
            printf("Listen failed\n");
            exit(0);
        }

        client_data[1] = accept(sockfd, (SA*)&cli, &len);
        if (client_data[1] < 0) {
            printf("Server accept failed\n");
            exit(0);
        }
        client_data[3] = player_id;
        write(client_data[1],&player_id,4);
        player_id ++;

        char opp[20];

        //Assigning symbols and opponents
        sprintf(opp,"X %d",player_id-1);
        write(client_data[0],opp,strlen(opp));
        sprintf(opp,"O %d",player_id-2);
        write(client_data[1],opp,strlen(opp));

        pthread_t thread;

        //Setting up a new thread for the game
        if (pthread_create(&thread, NULL, start_game, (void *)client_data)) {
            printf("Game creation failed\n");
        }
    }
    pthread_exit(0);
}
