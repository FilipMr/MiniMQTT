#include 	<sys/types.h>
#include 	<sys/socket.h>
#include 	<netdb.h>
#include 	<stdio.h>
#include 	<stdlib.h>
#include 	<unistd.h>
#include 	<string.h>
#include        <sys/time.h>    /* timeval{} for select() */
#include        <time.h>                /* timespec{} for pselect() */
#include        <netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include        <arpa/inet.h>   /* inet(3) functions */
#include        <errno.h>
#include        <fcntl.h>               /* for nonblocking */
#include        <netdb.h>
#include        <signal.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>

#define MAXLINE 500

int
main(int argc, char *argv[])
{

	struct timeval delay;
	int	sfd, n, s, i;
	socklen_t			len;
	char				recvline[MAXLINE], str[INET6_ADDRSTRLEN+1];
	time_t				ticks;
	struct sockaddr_in	servaddr, localaddr, peer_addr;
	char host[NI_MAXHOST], service[NI_MAXSERV];

     if (argc != 2) {
        fprintf(stderr, "Usage: %s <server-ip>\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1\n", argv[0]);
        return 1;
    }

    // Creating socket
	if ( (sfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		fprintf(stderr,"socket error : %s\n", strerror(errno));
		return 1;
	}

	bzero(&localaddr, sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr.s_addr   = htonl(INADDR_ANY);
	localaddr.sin_port   = htons(0);	// My port - os choosing

	if ( bind( sfd, (struct sockaddr *) &localaddr, sizeof(localaddr)) < 0){
		fprintf(stderr,"bind error : %s\n", strerror(errno));
		return 1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr   = 0;
	servaddr.sin_port   = htons(8888);	// My server

    // if ( bind( sfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
	// 	fprintf(stderr,"bind error : %s\n", strerror(errno));
	// 	return 1;
	// }

    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) != 1) {
        fprintf(stderr, "inet_pton error: invalid server IP: %s\n", argv[1]);
        close(sfd);
        return 1;
    }

	delay.tv_sec =6;  //opoznienie na gniezdzie
	delay.tv_usec = 0; 
	len = sizeof(delay);
	if( setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &delay, len) == -1 ){
		fprintf(stderr,"SO_RCVTIMEO setsockopt error : %s\n", strerror(errno));
		return 1;
	}		
		
	for( i=0; i < 3; i++){
        const char *msg = "hello";

        if( (n = sendto(sfd, msg, strlen(msg), 0, (struct sockaddr *) &servaddr, sizeof(servaddr)) ) < 0 ){
            fprintf(stderr, "sendto error : %s\n", strerror(errno));
            close(sfd);
            return 1;
		}

	        /* Read data from server */
		fprintf(stderr, "Waiting for server ...\n");
	
		len = sizeof(peer_addr);	
		if( (n = recvfrom(sfd, recvline, MAXLINE, 0, (struct sockaddr *) &peer_addr, 
				&len) ) < 0 ){ 
	
			perror("recfrom error");
			// if( errno == (EAGAIN | EWOULDBLOCK)) {
			if( errno == EAGAIN || errno == EWOULDBLOCK) {
				printf("Timeout - no serwers\n");
				exit(1);
			}
			else{
				printf("RECVFROM error\n");
				exit(1);
			}
		}
	
        // Print who replied
		s = getnameinfo((struct sockaddr *) &peer_addr,
						len, host, NI_MAXHOST,
						service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
		if (s == 0)
			printf("Received %ld bytes from %s:%s\n",
								(long) n, host, service);
		else
			fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
			
		recvline[n] = 0;	/* null terminate */
		if (fputs(recvline, stdout) == EOF){
			fprintf(stderr,"fputs error : %s\n", strerror(errno));
			exit(1);
		}        
	}
    close(sfd);
	exit(EXIT_SUCCESS);
}