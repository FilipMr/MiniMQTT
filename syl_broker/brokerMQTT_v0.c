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


//////////////////// RZECZY DO PLIKU NAGLOWKOWEGO, HEADER STRUKTUY ITD /////////////////// 

enum qos_level
{
    AT_MOST_ONCE,
    AT_LEAST_ONCE, // for future
    EXACTLY_ONCE   // for future
};



union mqtt_header // pierwszy bajt naglowka 
{
    unsigned char byte;
    struct
    {
        unsigned retain : 1; // czy ostatnia wiadomość ma być zapamiętana dla nowych subskrybentów
        unsigned qos : 2;    // poziom QoS (0, 1, 2)
        unsigned dup : 1;    // flaga ponownej transmisji
        unsigned type : 4;   // typ pakietu (CONNECT, PUBLISH, itd.)
    } bits;
};

struct mqtt_connect
{
    union mqtt_header header;
    union 
    {
        unsigned char byte;
        struct
        {
            unsigned reserved : 1;
            unsigned will     : 1;
            unsigned rfFuture : 6; // Reserved for future 
        };
    }bits;
    
    struct // dane do konfiguracji sesji
    {
        unsigned short keepalive;   // maksymalny czas bez komunikacji
        unsigned char *client_id;   // unikalny identyfikator klienta
        unsigned char *username;    // dane uwierzytelniania 
        unsigned char *password;
        unsigned char *will_topic;  // Last Will Message - to wiadomość, którą broker wysyła automatycznie, gdy klient rozłączy się nieprawidłowo
        unsigned char *will_message;
    } payload; 
};



const char *data = "parowki\n";




////// KONIEC PLIKU NAGLOWKOWEGO ///////// 

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

    // MQTTpacket *packet = malloc(sizeof(MQTTpacket));
    // if (!packet) {
    //     fprintf(stderr, "Memory allocation error\n");
    //     return -1;
    // }
    // memset(packet, 0, sizeof(MQTTpacket));  

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

    ///////////////////////////////////// koniec funkcji setup_socket()//////////////////// 
    printf("Waiting for client... \n");
    
    while(1)
    {
        nready = epoll_wait(epollfd, events, MAXEVENTS, -1);
        if(nready == -1)
        {
            fprintf(stderr, "epoll_wait() error!: %s\n", strerror(errno));
            return -1;
        }

        for (int i = 0; i < nready; i++)
        {
            if(events[i].data.fd == listenfd)
            {
                peer_addr_len = sizeof(peer_addr);
                connfd = accept(listenfd, (SA*)&peer_addr, &peer_addr_len);
                if(connfd < 0)
                {
                    fprintf(stderr, "accept() error!: %s\n", strerror(errno));
                    continue;
                }
                printf("Connected with client\n");

                const char* welcomeMessage = "**************** MQTT BROKER **************** \n";
                const char* welcomeMessage2 = "Publish payload on topic [press 'p'] \n";
                const char* welcomeMessage3 = "Subscribe on topic [press 's'] \n";
                const char* welcomeMessage4 = "\n";

                // Połącz wszystkie wiadomości w jeden ciąg
                char fullMessage[MAXLINE];
                snprintf(fullMessage, MAXLINE, "%s%s%s%s", welcomeMessage, welcomeMessage2, welcomeMessage3, welcomeMessage4);

                // Wyślij pełną wiadomość jedną komendą
                if (send(connfd, fullMessage, strlen(fullMessage), 0) < 0) {
                    fprintf(stderr, "send() error: %s\n", strerror(errno));
                    continue;
                }

                printf("Waiting for client response about action... \r\n");
    
                if((n = recv(connfd, &cliAnswer, sizeof(cliAnswer), 0)) < 0) 
                {
                    fprintf(stderr, "recv answer() error!: %s\n", strerror(errno));
                    continue;
                }
                printf("Client choosen option: %s \r\n", cliAnswer.answer);
                if(strcmp(cliAnswer.answer, "p") == 0)
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
                // // Sprawdzanie, co klient chce wykonać
                // if(strcmp(buff, "p") == 0)
                // {
                //     printf("Klient wybrał publish.\n");
                //       const char* askTopic = "Tell me in which topic, are you interested in: \n";
                //     if(send(currfd, askTopic, sizeof(askTopic), 0) < 0)
                //     {
                //         fprintf(stderr, "send() error: %s", strerror(errno));
                //     }

                //     if((n = recv(currfd, buff, MAXLINE, 0)) < 0) 
                //     {
                //         fprintf(stderr, "recv() error!: %s\n", strerror(errno));
                //         continue;
                //     }
                //     buff[n] = '\0'; 
                //     printf("TOPIC Clienta: %s", buff);


                //     // // Wyślij dane
                //     // if(send(currfd, &packet, sizeof(packet), 0) < 0)
                //     // {
                //     //     fprintf(stderr, "send() error: %s\n", strerror(errno));
                //     //     close(currfd);
                //     //     continue;
                //     // }S
                // }
                // // else if(strcmp(buff, "2") == 0)
                // // {}
                


                // currfd = connfd;
                // if((n = recv(currfd, &data, sizeof(data), 0)) < 0) 
                // {
                //     // Closing the descriptor will make epoll remove it from the set of descriptors which are monitored.
                //     fprintf(stderr, "recv() error!: %s\n", strerror(errno));
                //     close(currfd);
                //     continue;
                // }
                // printf("Received Client_id: %s", data.client_id);
                // printf("Received payload: %s", data.payload);
                // printf("Received data type: %d", data.type);
                // if(n == 0) {
                //     // The socket sent EOF. 
                //     close (currfd);
                //     continue;
                // }


                // if( send(currfd, &packet, sizeof(packet), 0) < 0) {
                //     // Something went wrong.c
                //     fprintf(stderr, "send error: %s", strerror(errno));
                //     close(currfd);
                //     continue;
                // }
            }
        }
    }

    // free(packet);
    close(listenfd);
    close(epollfd);

    return 0;
}   