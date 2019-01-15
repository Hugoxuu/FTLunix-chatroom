/* 
 * 18749 Team Project Server Backup version 2 by Team 7
 * Team member: Lue Li, Yan Pan, Yunmeng Xie, Zeyuan Xu
 * Final Demo: Fault-tolerant Chat Room
 * Date: 12/15/2018
 */

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "csapp.h"

#define DefaultPort "3491"
#define primary_filename "server"
#define log_filename "log.txt"
#define log_buffer_size 4096

/* flag to indicate if the primary server is down */
int is_server_down = 0;

/* pid for child process */
pid_t pid;

/* SIGALRM signal handler */
void sigalrm_handler();

/* receive the checkpoint information and store it locally */
void recv_checkpoint(int fd);

int main(int argc, char *argv[])
{
	char checkpoint_itv[100] = "60";
	char primary_host[100]; // becomes the host of its backup after this back up becomes primary
//	printf("Welcome to back up server!\n");
	char opt;
		while((opt = getopt(argc,argv,"hP:f:"))!= -1){
			switch(opt){
				case 'h':
					printf("\t-h\t\tprint this message\n");
					printf("\t-P <host>\tassign primary server host.Default none.\n");
					printf("\t-f <frequency>\tconfig checkpointing interval(After becomes primary)(60 secs by defalut)\n");
					return 0;
				case 'P':
					strncpy(primary_host,optarg,100);
					break;
				case 'f':
					// max hb 5 mins
					strncpy(checkpoint_itv,optarg,100);
					break;
				default:
					printf("check %s -h for help\n",argv[0]);
					return 0;

			}
		}

	/* listen descriptor, connection descriptor */
	int listenfd, connfd;

	/* primary server address info */
    struct sockaddr_storage primary_server_addr;
	socklen_t primary_server_len;

	/* try to listen at the given port */
	listenfd = Open_listenfd(DefaultPort);


	/* Accept connection from the primary server */
	connfd = Accept(listenfd, 
		(SA *)&primary_server_addr, &primary_server_len);

	/* install signal alrm handler for child process only*/
	signal(SIGALRM, sigalrm_handler);
	alarm(10);

	printf("Connection with primary server is successful.!\n");

	/* one while loop for one checkpointing process */	
	while (!is_server_down)
	{
		recv_checkpoint(connfd);
			/* one checkpoint process done */
	}

	/* primary server is down, backup -> new primary */
	printf("The old primary server is down, I am the new primary server now.\n");

	/* Get rid of the old primary server */
	Close(connfd);
	Close(listenfd);

	/* The command arguments to pass */
	char* arguments[] = {primary_filename,"-B",primary_host,"-f",checkpoint_itv, NULL}; 
	/* Run the primary server program */
	Signal(SIGALRM, SIG_DFL); //restore the sigalrm handler
	Execve(primary_filename, arguments, environ);

}

void recv_checkpoint(int fd)
{
		// buffer to store the checkpoint message
	char log_buffer[log_buffer_size];

	// open the local log file
	int log_fd;
	int recvbytes;

	// read and write until EOF
	

	if ((recvbytes = recv(fd, log_buffer, log_buffer_size,MSG_DONTWAIT)) <= 0){
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			return; // nothing received
		}
		else{ // EOF or error
			is_server_down = 1;
			return;	
		}
	}
	printf("Backup server is receving checkpoint from primary server.\n");


	if ((log_fd = open(log_filename, O_WRONLY|O_CREAT, S_IRWXU))<0){
			perror("open");
			exit(1);
		}
	
	//printf("receiving %d bytes, writing to local file \n", recvbytes);

	write(log_fd, log_buffer, recvbytes);	
	
	printf("checkpoint done!\n");
	/* reset alarm for timeout */
	is_server_down = 0;
	alarm(10);

	//if reaching EOF then stop writing and return
	Close(log_fd);

	return;
}

/* SIGALRM signal handler, 
 * set is_server_down when alarmed 
 */
void sigalrm_handler(){
    is_server_down = 1;
    return;
}



