/* 
 * 18749 Team Project Server version 4 by Team 7
 * Team member: Lue Li, Yan Pan, Yunmeng Xie, Zeyuan Xu
 * Final Demo: Fault-tolerant Chat Room
 * Date: 12/15/2018
 */

#include <stdio.h> 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <errno.h>
#include <sys/shm.h>
#include <time.h>
#include <fcntl.h>
#include <netdb.h>
#include <getopt.h>
#define PERM S_IRUSR|S_IWUSR 
#define MYPORT 3490
#define BACKLOG 10
#define MAXHOST 32//maximum host name characters
#define BUFSIZE 1024
#define WELCOME "|----------Welcome to the chat room! ----------|"

const char *pathName = "record.txt";
// checkpoint sent flag
volatile sig_atomic_t ck_ready = 0;
/* SIGALRM signal handler */
/* SIGALRM signal handler, set hb_ready when alarmed */
void sigalrm_handler(){
//    alarm(heartbeat_itv);
    ck_ready = 1;
    return;
}

void itoa(int i,char*string)
{
	int power,j;
	j=i;
	for(power=1;j>=10;j/=10)
		power*=10;
	for(;power>0;power/=10)
	{
		*string++='0'+i/power;
		i%=power;
	}
	*string='\0';
}

// Get current system time
void get_cur_time(char * time_str)
{
	time_t timep;
	struct tm *p_curtime;
	char *time_tmp;
	time_tmp=(char *)malloc(2);
	memset(time_tmp,0,2);

	memset(time_str,0,20);
	time(&timep);
	p_curtime = localtime(&timep);
	strcat(time_str," (");
	itoa(p_curtime->tm_hour,time_tmp);
	strcat(time_str,time_tmp);
	strcat(time_str,":");
	itoa(p_curtime->tm_min,time_tmp);
	strcat(time_str,time_tmp);
	strcat(time_str,":");
	itoa(p_curtime->tm_sec,time_tmp);
	strcat(time_str,time_tmp);
	strcat(time_str,")");
	free(time_tmp);
}

key_t shm_create()
{
	key_t shmid;

	if((shmid = shmget(IPC_PRIVATE,1024,PERM)) == -1)
	{
		fprintf(stderr,"Create Share Memory Error:%s\n\a",strerror(errno));
		exit(1);
	}
	return shmid;
}

int bindPort(unsigned short int port)
{ 
	int sockfd;
	struct sockaddr_in my_addr;
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(my_addr.sin_zero),0);
	int on = 1;
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(1);
	}
	if(bind(sockfd,(struct sockaddr*)&my_addr,sizeof(struct sockaddr)) == -1)
	{
		perror("bind");
		exit(1);
	}
	printf("bing success!\n");
	return sockfd;
}


int main(int argc, char *argv[])
{
	int sockfd,clientfd,sin_size,recvbytes; 
	pid_t pid,ppid,pidd;
	char *buf, *r_addr, *w_addr, *temp, *time_str;
	struct sockaddr_in their_addr;
	key_t shmid;
	int checkpoint_freq = 60;
	int backup = 0;
	char backup_host[MAXHOST]="192.168.0.0";
	char opt;
	while((opt = getopt(argc,argv,"hB:f:"))!= -1){
        switch(opt){
            case 'h':
            	printf("\t-h\t\tprint this message\n");
                printf("\t-B <host>\tassign backup server host.Default none.\n");
                printf("\t-f <frequency>\tconfig checkpointing interval(60 secs by defalut)\n");
                return 0;
            case 'B':
                backup = 1;
                strncpy(backup_host,optarg,MAXHOST);
                break;
            case 'f':
                // max hb 5 mins
                checkpoint_freq = atoi(optarg) > 300 ? 300 :atoi(optarg);
                break;
            default:
                printf("check %s -h for help\n",argv[0]);
                return 0;

        }
    }
	shmid = shm_create();

	temp = (char *)malloc(255);
	time_str=(char *)malloc(20);
	sockfd = bindPort(MYPORT);
	int out = open(pathName, O_RDWR | O_CREAT, S_IRWXU); //open record file
	char *start = "Records:\n";
	write(out, start, strlen(start));
	pidd = fork();
	if (pidd == 0){
		int fd;
		char *ck_buf;
		int port = 3491;
		int sendbytes;
		struct hostent *host;
		struct sockaddr_in clientaddr;
		if(signal(SIGALRM,sigalrm_handler) == SIG_ERR){
            perror("checkpoint initialization");
            exit(1);
        }
        host = gethostbyname(backup_host);
		if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1){
				perror("socket\n");
				exit(1);
		}

		clientaddr.sin_family = AF_INET;
		clientaddr.sin_port = htons((uint16_t)port);
		clientaddr.sin_addr = *((struct in_addr *)host->h_addr);
		bzero(&(clientaddr.sin_zero),0);
		printf("Connectting...\n");
		if(connect(fd,(struct sockaddr *)&clientaddr,sizeof(struct sockaddr)) == -1){
			perror("connect");
			exit(1);
		}
		if((ck_buf = (char *)malloc(BUFSIZE * sizeof(char))) == NULL){
			perror("malloc");
			exit(1);
		}

		ck_ready = 0;
		alarm(checkpoint_freq);
		while(1) {
		if (ck_ready == 1) {
			ck_ready = 0;
			alarm(checkpoint_freq);
			printf("Sending checkpoint...\n");
			char linebuf[1024];
			FILE *fp = fopen(pathName, "r");
			memset(ck_buf,0,BUFSIZE);
			while(fgets(linebuf, 1024, (FILE *)fp)) {
				strcat(ck_buf, linebuf);
			}
			if((sendbytes = send(fd,ck_buf,strlen(ck_buf),0)) == -1){
            perror("send\n");
            //break;
           	}
			printf("%s\n", ck_buf);
			fclose(fp);
			
			}
			
		}
		free(ck_buf);

		}
		else if (pidd > 0) {
		while(1)
		{ 		
		if(listen(sockfd,BACKLOG) == -1)
		{
			perror("listen");
			exit(1);
		}
		printf("listening......\n");
		if((clientfd = accept(sockfd,(struct sockaddr*)&their_addr,&sin_size)) == -1)
		{
			perror("accept");
			exit(1);
		}
		printf("accept from:%s\n",inet_ntoa(their_addr.sin_addr));
		send(clientfd,WELCOME,strlen(WELCOME),0);
		buf = (char *)malloc(255);

		ppid = fork();
		if(ppid == 0)
		{
			pid = fork();
			while(1)
			{
				if(pid > 0)
				{
					memset(buf,0,255);

					if((recvbytes = recv(clientfd,buf,255,0)) <= 0)
					{
						perror("recv1");
						close(clientfd);
						raise(SIGKILL);
						exit(1);
					}
					//write buf's data to share memory
					w_addr = shmat(shmid, 0, 0);
					memset(w_addr, '\0', 1024);
					strncpy(w_addr, buf, 1024);
					get_cur_time(time_str);
					strcat(buf,time_str);
					printf(" %s\n",buf);
					if (strcmp(w_addr, "Are you alive?") != 0) {
						strcat(buf, "\n");
						write(out, buf, strlen(buf)); //write records into file
					}

				}
				else if(pid == 0)
				{
					sleep(1);
					r_addr = shmat(shmid, 0, 0);

					if(strcmp(temp,r_addr) != 0)
					{
						
						if (strcmp(r_addr, "Are you alive?") == 0) 
						{
							strcpy(r_addr, "I am alive!");
						}
						else {
								get_cur_time(time_str); 
								strcat(r_addr,time_str);	
							}
						printf("%s\n", r_addr);

						if(send(clientfd,r_addr,strlen(r_addr),0) == -1)
						{
							perror("send");
						}
						memset(r_addr, '\0', 1024);
						strcpy(r_addr,temp);
					}
					
					//used to send checkpoint
				}
				else
				perror("fork");
			}
		}
	}
	}
	printf("------------------------------\n");
	free(buf);
	free(temp);
	free(time_str);
	close(sockfd);
	close(clientfd);
	close(out);
	return 0;
}

