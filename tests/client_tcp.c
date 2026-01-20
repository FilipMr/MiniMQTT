#include        <sys/types.h>   /* basic system data types */
#include        <sys/socket.h>  /* basic socket definitions */
#include        <netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include        <arpa/inet.h>   /* inet(3) functions */
#include        <errno.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include        <strings.h>
#include        <unistd.h>

#include "MQTTstruct.h"

#define MAXLINE 1024
#define SA      struct sockaddr

char buff[MAXLINE];

int main(int argc, char *argv[])
{
    int         sockfd, n;
    struct      sockaddr_in servaddr;
    char        recvline[MAXLINE+1];   
    int err;
    const char* msg = "Hello Sylvo\n";
	const char* client_id = "Filo *-* ";
	const char* topic = "test/msg\n";
	MQTTpacket fromServerdata;
	cliAnswer cliAnswer;

	// MQTTpacket* packet = (MQTTpacket*)malloc(sizeof(MQTTpacket));
	// memset(packet, 0, sizeof(MQTTpacket));
	// strncpy(packet->client_id, client_id, MAXCLIENTS);
	// packet->type = DATA_PACKET;
	// strncpy(packet->payload, msg, MAX_PAYLOAD_SIZE-1);

	MQTTpacket packet = {"Filo_ID\n", DATA_PACKET, "SIEMA ENIU\n"};


    if (argc != 2) {
        fprintf(stderr, "ERROR: usage: a.out <IPaddress>  \n");
		return 1;
    }

    // Creating socket
	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		fprintf(stderr,"socket error : %s\n", strerror(errno));
		return 1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port   = htons(8888);	// My port - os choosing

    if ( (err=inet_pton(AF_INET, argv[1], &servaddr.sin_addr)) <= 0){
        if(err == 0 )
			fprintf(stderr,"inet_pton error for %s \n", argv[1] );
		else
			fprintf(stderr,"inet_pton error for %s : %s \n", argv[1], strerror(errno));
		return 1;
    }

    if ( connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr,"connect error : %s \n", strerror(errno));
		return 1;
    }

    // Odbiór wiadomości od serwera
    n = recv(sockfd, buff, MAXLINE, 0);
    if (n < 0) {
        perror("recv failed");
        close(sockfd);
        exit(1);
    }
    buff[n] = '\0';
    printf("\n%s", buff);

	// publishPacket(sockfd, msg, packet);

	printf("Choose option: ");
	scanf("%s", cliAnswer.answer);
	if(strcmp(cliAnswer.answer, "p") == 0)
	{
		printf("Type topic: ");
		scanf("%s", cliAnswer.topic);

		printf("Type your payload: ");
		scanf("%s", cliAnswer.payload);
	}


	if(send(sockfd, &cliAnswer, sizeof(cliAnswer), 0) < 0)
	{
		perror("send failed");
	}



	// n = recv(sockfd, buff, sizeof(buff), 0);
	// if(n < 0)
	// {
	// 	buff[n] = "\n";
	// 	printf("%s", buff);
	// }


	// n = recv(sockfd, &fromServerdata, sizeof(fromServerdata), 0);
    // if (n < 0)
	// 	fprintf(stderr,"recv error : %s\n", strerror(errno));

	// printf("from server, client_id: %s\n", fromServerdata.client_id);
	// printf("from server, msg_type : %d\n", fromServerdata.type);
	// printf("from server, payload : %s\n", fromServerdata.payload);
	// fprintf(stderr,"\nOK\n");
	fflush(stdout);


	// free(packet);s
    close(sockfd);
	exit(0);
}