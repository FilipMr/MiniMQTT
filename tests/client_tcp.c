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
#define DISCOVERY_PORT 5000
#define DEFAULT_TCP_PORT 8888
#define MCAST_GRP "239.1.2.3"
#define DISCOVERY_MSG "mqttclient"


char fromServer[MAXLINE];

int main(int argc, char *argv[])
{
    int         sockfd, n;
    struct      sockaddr_in servaddr;
    char        recvline[MAXLINE+1];   
    int err;
    const char* msg = "Hello Sylvo\n";
	const char* client_id = "Filo *-* ";
	const char* topic = "test/msg\n";
	MQTTpacket fromServerdata; // struct do odbioru danych z servera
	cliAnswer cliAnswer; 	   // struct do wysylania danych na server


    char server_ip[INET_ADDRSTRLEN] = {0};
    int server_port = DEFAULT_TCP_PORT;

    if (argc >= 2) {
        snprintf(server_ip, sizeof(server_ip), "%s", argv[1]);
        if (argc >= 3) {
            server_port = atoi(argv[2]);
            if (server_port <= 0 || server_port > 65535) {
                fprintf(stderr, "ERROR: invalid port: %s\n", argv[2]);
                return 1;
            }
        }
    } else {
        int udpfd;
        struct sockaddr_in mcast_addr;
        struct sockaddr_in reply_addr;
        socklen_t reply_len = sizeof(reply_addr);
        char reply_buf[64];

        if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            fprintf(stderr, "udp socket error: %s\n", strerror(errno));
            return 1;
        }

        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        if (setsockopt(udpfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            fprintf(stderr, "setsockopt SO_RCVTIMEO error: %s\n", strerror(errno));
            close(udpfd);
            return 1;
        }

        bzero(&mcast_addr, sizeof(mcast_addr));
        mcast_addr.sin_family = AF_INET;
        mcast_addr.sin_port = htons(DISCOVERY_PORT);
        if (inet_pton(AF_INET, MCAST_GRP, &mcast_addr.sin_addr) <= 0) {
            fprintf(stderr, "inet_pton error for %s: %s\n", MCAST_GRP, strerror(errno));
            close(udpfd);
            return 1;
        }

        if (sendto(udpfd, DISCOVERY_MSG, strlen(DISCOVERY_MSG), 0,
                   (SA *)&mcast_addr, sizeof(mcast_addr)) < 0) {
            fprintf(stderr, "sendto discovery error: %s\n", strerror(errno));
            close(udpfd);
            return 1;
        }

        n = recvfrom(udpfd, reply_buf, sizeof(reply_buf) - 1, 0,
                     (SA *)&reply_addr, &reply_len);
        if (n < 0) {
            fprintf(stderr, "recvfrom discovery error: %s\n", strerror(errno));
            close(udpfd);
            return 1;
        }

        reply_buf[n] = '\0';
        server_port = atoi(reply_buf);
        if (server_port <= 0 || server_port > 65535) {
            fprintf(stderr, "invalid port in discovery reply: %s\n", reply_buf);
            close(udpfd);
            return 1;
        }

        if (inet_ntop(AF_INET, &reply_addr.sin_addr, server_ip, sizeof(server_ip)) == NULL) {
            fprintf(stderr, "inet_ntop error: %s\n", strerror(errno));
            close(udpfd);
            return 1;
        }

        close(udpfd);
        printf("Discovered server %s:%d\n", server_ip, server_port);
    }

	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		fprintf(stderr,"socket error : %s\n", strerror(errno));
		return 1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port   = htons(server_port);	// server TCP port

    if ( (err=inet_pton(AF_INET, server_ip, &servaddr.sin_addr)) <= 0){
        if(err == 0 )
			fprintf(stderr,"inet_pton error for %s \n", server_ip );
		else
			fprintf(stderr,"inet_pton error for %s : %s \n", server_ip, strerror(errno));
		return 1;
    }

    if ( connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) 
	{
        fprintf(stderr,"connect error : %s \n", strerror(errno));
		return 1;
    }

    // Odbiór wiadomości od serwera do buffora "fromServer"
    n = recv(sockfd, fromServer, MAXLINE, 0);
    if (n < 0) {
        perror("recv failed");
        exit(1);
    }
    fromServer[n] = '\0';
    printf("\n%s", fromServer);

	// uzupełnianie struktury aby wysłać rządanie na server 
	snprintf(cliAnswer.client_id, sizeof(cliAnswer.client_id), "%s", "Filo");
	cliAnswer.type = INFO_PACKET;
	printf("Choose option: ");
	scanf("%s", cliAnswer.answer);
	if(strcmp(cliAnswer.answer, "p") == 0)
	{
		printf("Type topic: ");
		scanf("%s", cliAnswer.topic);

		printf("Type your payload: ");
		scanf("%s", cliAnswer.payload);
	}
	else if (strcmp(cliAnswer.answer, "s") == 0)
	{
		printf("Type topic: ");
		scanf("%s", cliAnswer.topic);
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
