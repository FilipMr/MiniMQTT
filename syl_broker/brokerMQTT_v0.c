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









////// KONIEC PLIKU NAGLOWKOWEGO ///////// 

int main(int argc, char **argv)
{
    int listenfd, connfd, sockfd;
    int epollfd, nready, n, currfd;
    struct sockaddr_in6 servaddr;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    char buff[MAXLINE];
    struct epoll_event events[MAXEVENTS];
    struct epoll_event ev;

    if((listenfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "socket() error!: %s\n", strerror(errno));
        return -1;
    }

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

                currfd = connfd;
                if((n = read(currfd, buff, sizeof buff)) == -1) 
                {
                    // Closing the descriptor will make epoll remove it from the set of descriptors which are monitored.
                    fprintf(stderr, "read() error!: %s\n", strerror(errno));
                    close(currfd);
                    continue;
                }
                printf("Received payload: %s", buff);
                if(n == 0) {
                    // The socket sent EOF. 
                    close (currfd);
                    continue;
                }
                if( write(currfd, buff, n) == -1) {
                    // Something went wrong.
                    close(currfd);
                    continue;
                }
            }
        }
    }

    close(listenfd);
    close(epollfd);

    return 0;
}


int setup_socket(void)
{

}