/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>

#define PORT "8989" // the port client will be connecting to

#define MAXDATASIZE 140 // max number of bytes we can get at once

int sockfd;
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void sig_usr(int signo)
{
	if(signo==SIGINT)
	{
		printf("\nDisconnected from the server. \n");
		close(sockfd);
		exit(0);
	}
	else
		printf("Unknown signal number\n");
}

int main(int argc, char *argv[])
{
	struct tm *info;
	time_t rawtime;
	fd_set readfds, readrecv;
	struct timeval tv;
    int numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
  	char mesg[140];

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }
  	//Ctrl + C handler
  	if(signal(SIGINT, sig_usr) == SIG_ERR)
			printf("Error creating SIG_INT\n");

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

		printf("1 - Login\n2 - Create Account\n");

    freeaddrinfo(servinfo); // all done with this structure

		while(1){
		  time(&rawtime);
			info = localtime(&rawtime);
			FD_ZERO(&readfds);
			FD_ZERO(&readrecv);
			FD_SET(STDIN_FILENO, &readfds);
			FD_SET(sockfd, &readrecv);
			tv.tv_sec = 0;
			tv.tv_usec =1000;

			int select_retval2 = select(sockfd + 1, &readrecv, NULL, NULL, &tv);
			int select_retval = select(STDIN_FILENO+1, &readfds, NULL, NULL, &tv);

				if (FD_ISSET(STDIN_FILENO, &readfds)) {
    			if (fgets(mesg,sizeof(mesg), stdin)) {
        		//printf("%s",mesg);
        		if(send(sockfd, mesg, strlen(mesg), 0) == -1) {
          		perror("send");
          	}
        	}
    		}
    		if (FD_ISSET(sockfd, &readrecv)) {
    			if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        		perror("recv");
        		exit(1);
    			}
    			else	{
    				int hour = info->tm_hour;
    				int min = info->tm_min;
						buf[numbytes-1] = '\0'; //numbytes - 1 to omit '\n' when sending message by hitting [Enter]
		    		printf("<%i:%d> %s",hour, min, buf);
		    		fflush(stdout); //force screen to show the text
					}
    		}

    		//printf("fgets: %i - recv: %d , %i %d\n", select_retval, select_retval2, FD_ISSET(STDIN_FILENO, &readrecv), FD_ISSET(sockfd, &readrecv));

		}
    close(sockfd);

    return 0;
}
