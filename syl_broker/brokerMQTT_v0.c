#include    <sys/types.h>   /* basic system data types */
#include    <sys/socket.h>  /* basic socket definitions */
#include    <sys/time.h>    /* timeval{} for select() */
#include    <time.h>                /* timespec{} for pselect() */
#include    <netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include    <arpa/inet.h>   /* inet(3) functions */
#include    <errno.h>
#include    <netdb.h>
#include    <signal.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <limits.h>		/* for OPEN_MAX */
#include    <sys/epoll.h>
#include    <unistd.h>

#include "MQTTstruct.h"


#define MAXLINE 1024
#define SA struct sockaddr
#define LISTENQ 2
#define INFTIM -1
#define MAXEVENTS 2000


#define SA struct sockaddr 
#define BACKLOG 100

#define PORT 8888

int setup_socket(void);


const char *data = "parowki\n";


int main(int argc, char **argv)
{
    int listenfd, connfd, sockfd;
    int epollfd, nready, n, currfd;
    struct sockaddr_in6 servaddr;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    char buff[MAXLINE], askBuff[MAXLINE];
    struct epoll_event events[MAXEVENTS];
    struct epoll_event ev;
    cliAnswer cliAnswer;

    MQTTpacket data;

    /// obiekt do testow, testy wysylania packietu do clienta 
    MQTTpacket packet = {"id_DEFAULT", DATA_PACKET, "payload_testowy"};


    if((listenfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "socket() error!: %s\n", strerror(errno));
        return -1;
    }
    int one = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    bzero(&servaddr, sizeof(servaddr));

    ( (struct sockaddr_in6 *)&servaddr ) -> sin6_family = AF_INET6;
    ( (struct sockaddr_in6 *)&servaddr ) -> sin6_addr = in6addr_any;
    ( (struct sockaddr_in6 *)&servaddr ) -> sin6_port = htons(PORT);

    if(bind(listenfd, (SA *)&servaddr, sizeof(servaddr)) < 0)
    {
        fprintf(stderr, "socket() error!: %s\n", strerror(errno));
        return -1;
    }

    if((listen(listenfd, BACKLOG)) < 0)
    {
        fprintf(stderr, "listen() error!: %s\n", strerror(errno));
        return -1;
    }

    if((epollfd = epoll_create(MAXEVENTS))== -1)
    {
        fprintf(stderr, "epoll_create() error!: %s\n", strerror(errno));
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
    {
        fprintf(stderr, "epoll_ctl() error!: %s\n", strerror(errno));
        return -1;
    }

    printf("Waiting for client... \n");
    
    while(1)
    {
        nready = epoll_wait(epollfd, events, MAXEVENTS, -1);
        if(nready == -1)
        {
            fprintf(stderr, "epoll_wait() error!: %s\n", strerror(errno));
            return -1;
        }

        for (int i = 0; i < nready; i++) /// for iterujący po tej liscie "aktywnych" deskryptorów w jadrze zapisanych przez epoll
        {
            if(events[i].data.fd == listenfd) // sprawdzamy czy deskryptor na tej liscie jest taki sam jak naszego gniazda nasluchujacego 
            {
                peer_addr_len = sizeof(peer_addr);
                connfd = accept(listenfd, (SA*)&peer_addr, &peer_addr_len);  /// dostajemy adres clienta
                if(connfd < 0)
                {
                    fprintf(stderr, "accept() error!: %s\n", strerror(errno));
                    continue;
                }
                printf("Connected with client\n");

                /// wiadomosc powitalna
                const char* welcomeMessage = "**************** MQTT BROKER **************** \n";
                const char* welcomeMessage2 = "Publish payload on topic [press 'p'] \n";
                const char* welcomeMessage3 = "Subscribe on topic [press 's'] \n";
                const char* welcomeMessage4 = "\n";

                char fullMessage[MAXLINE];
                snprintf(fullMessage, MAXLINE, "%s%s%s%s", welcomeMessage, welcomeMessage2, welcomeMessage3, welcomeMessage4);

                if (send(connfd, fullMessage, strlen(fullMessage), 0) < 0) {
                    fprintf(stderr, "send() error: %s\n", strerror(errno));
                    continue;
                }

                printf("Waiting for client response about action... \r\n");
    
                if((n = recv(connfd, &cliAnswer, sizeof(cliAnswer), 0)) < 0) /// odczyt wyboru ktorego dokonał klient
                {
                    fprintf(stderr, "recv answer() error!: %s\n", strerror(errno));
                    continue;
                }
                printf("Client choosen option: %s \r\n", cliAnswer.answer); /// dla debugu, zeby widziec co nam przyszlo od klienta
                if(strcmp(cliAnswer.answer, "p") == 0) /// logika wyboru co klient wybrał, trzeba napisać funkcje: PUBLISH() oraz SUBSCRIBE(), ktore beda obslugiwac wybor klienta
                {
                    printf("Client choose publish\n");
                    // wywolujemy nasza funkcje publish 
                }
                else if (strcmp(cliAnswer.answer, "s") == 0)
                {
                    printf("Client choose subscribe\n");
                    // wywolujemy nasza funkcje odpowiedzialna za subscribe
                }
                else
                {
                    printf("Client choose wrong\n");
                }
            }
        }
    }
    
    close(listenfd);
    close(epollfd);

    return 0;
}   