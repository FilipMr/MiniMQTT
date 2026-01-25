#include    <sys/types.h>
#include    <sys/socket.h>
#include    <sys/time.h>
#include    <time.h>
#include    <netinet/in.h>
#include    <arpa/inet.h>
#include    <errno.h>
#include    <netdb.h>
#include    <signal.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <strings.h>   // bzero
#include    <limits.h>
#include    <sys/epoll.h>
#include    <unistd.h>
#include    <fcntl.h>

#include "MQTTstruct.h"

#define MAXLINE 1024
#define SA struct sockaddr
#define LISTENQ 2
#define INFTIM -1
#define MAXEVENTS 2000

#define BACKLOG 100
#define PORT 8888

const char *data = "parowki\n";

typedef struct {
    size_t got;                 // ile bajtow structa juz mamy
    cliAnswer msg;              // bufor na cala strukture
} ClientState;

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    int epollfd, nready, currfd;
    struct sockaddr_in6 servaddr;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    char buff[MAXLINE], askBuff[MAXLINE];
    struct epoll_event events[MAXEVENTS];
    struct epoll_event ev;

    MQTTpacket data;
    MQTTpacket packet = {"id_DEFAULT", DATA_PACKET, "payload_testowy"};

    /* tablica stanow dla fd */
    long maxfds = sysconf(_SC_OPEN_MAX);
    if (maxfds < 0) maxfds = 1024;
    ClientState *st = calloc((size_t)maxfds, sizeof(*st));
    if (!st) {
        fprintf(stderr, "calloc() error!: %s\n", strerror(errno));
        return -1;
    }

    if((listenfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "socket() error!: %s\n", strerror(errno));
        free(st);
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
        fprintf(stderr, "bind() error!: %s\n", strerror(errno));
        close(listenfd);
        free(st);
        return -1;
    }

    if((listen(listenfd, BACKLOG)) < 0)
    {
        fprintf(stderr, "listen() error!: %s\n", strerror(errno));
        close(listenfd);
        free(st);
        return -1;
    }

    if((epollfd = epoll_create(MAXEVENTS))== -1)
    {
        fprintf(stderr, "epoll_create() error!: %s\n", strerror(errno));
        close(listenfd);
        free(st);
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
    {
        fprintf(stderr, "epoll_ctl() error!: %s\n", strerror(errno));
        close(epollfd);
        close(listenfd);
        free(st);
        return -1;
    }

    printf("Waiting for client... \n");
    
    while(1)
    {
        nready = epoll_wait(epollfd, events, MAXEVENTS, -1);
        if(nready == -1)
        {
            fprintf(stderr, "epoll_wait() error!: %s\n", strerror(errno));
            break;
        }

        for (int i = 0; i < nready; i++)
        {
            currfd = events[i].data.fd;

            if(currfd == listenfd)
            {
                peer_addr_len = sizeof(peer_addr);

                connfd = accept(listenfd, (SA*)&peer_addr, &peer_addr_len);
                if(connfd < 0)
                {
                    fprintf(stderr, "accept() error!: %s\n", strerror(errno));
                    continue;
                }

                if (connfd >= maxfds) {
                    close(connfd);
                    continue;
                }

                if (set_nonblocking(connfd) < 0) {
                    close(connfd);
                    continue;
                }

                /* reset stanu odbioru dla nowego klienta */
                st[connfd].got = 0;
                memset(&st[connfd].msg, 0, sizeof(st[connfd].msg));

                printf("Connected with client\n");

                // wiadomosc powitalna
                const char* welcomeMessage = "**************** MQTT BROKER **************** \n";
                const char* welcomeMessage2 = "Publish payload on topic [press 'p'] \n";
                const char* welcomeMessage3 = "Subscribe on topic [press 's'] \n";
                const char* welcomeMessage4 = "\n";

                char fullMessage[MAXLINE];
                snprintf(fullMessage, MAXLINE, "%s%s%s%s",
                         welcomeMessage, welcomeMessage2, welcomeMessage3, welcomeMessage4);

                if (send(connfd, fullMessage, strlen(fullMessage), 0) < 0) {
                    fprintf(stderr, "send() error: %s\n", strerror(errno));
                    close(connfd);
                    continue;
                }

                // zarejestrowanie klienta w epoll dla zwielokrotnienia
                ev.events = EPOLLIN | EPOLLRDHUP;
                ev.data.fd = connfd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
                    close(connfd);
                    continue;
                }

                printf("Waiting for client response about action... \r\n");
            }
            else
            {
                // rozlaczenie (RDHUP/HUP/ERR)
                if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    close(currfd);
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, currfd, NULL);
                    st[currfd].got = 0;
                    continue;
                }

                // odbior tylko wtedy, gdy epoll da znac ze klient jest gotowy do odczytu
                while (1) {
                    size_t need = sizeof(cliAnswer) - st[currfd].got;
                    ssize_t n = recv(currfd,
                                     (char*)&st[currfd].msg + st[currfd].got,
                                     need,
                                     0);

                    if (n == 0) {
                        // klient rozlaczony
                        close(currfd);
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, currfd, NULL);
                        st[currfd].got = 0;
                        break;
                    }

                    if (n < 0) {
                        if (errno == EINTR) continue;
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // nie ma wiecej danych teraz
                            break;
                        }
                        // blad
                        close(currfd);
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, currfd, NULL);
                        st[currfd].got = 0;
                        break;
                    }

                    st[currfd].got += (size_t)n;

                    if (st[currfd].got == sizeof(cliAnswer)) {
                        // mamy kompletna strukture -> mozemy dzialac
                        cliAnswer cliAnswer = st[currfd].msg;

                        // (bezpieczenstwo) jesli answer to char[], dopilnuj '\0'
                        #ifdef __GNUC__
                        /* zakladamy, ze MQTTstruct.h ma pole answer jako tablica char */
                        cliAnswer.answer[sizeof(cliAnswer.answer) - 1] = '\0';
                        #endif

                        printf("Client option: %s\n", cliAnswer.answer);

                        if(strcmp(cliAnswer.answer, "p") == 0)
                        {
                            printf("Client choose publish\n");
                        }
                        else if (strcmp(cliAnswer.answer, "s") == 0)
                        {
                            printf("Client choose subscribe\n");
                        }
                        else
                        {
                            printf("Client choose wrong\n");
                        }

                        // przygotuj sie na kolejna strukture od tego samego klienta
                        st[currfd].got = 0;
                        memset(&st[currfd].msg, 0, sizeof(st[currfd].msg));
                    }
                }
            }
        }
    }
    
    close(listenfd);
    close(epollfd);
    free(st);

    return 0;
}
