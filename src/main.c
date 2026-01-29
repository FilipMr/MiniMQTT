#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   // bzero
#include <limits.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h> // do sprawdzania plikow

#include "cJSON.h"

#include "MQTTstruct.h"

#define MAXLINE 1024
#define SA struct sockaddr
#define LISTENQ 2
#define INFTIM -1
#define MAXEVENTS 2000
#define MAXCLIENTS_K_V 200

#define BACKLOG 100
#define PORT 8888
#define DISCOVERY_PORT 5000


static volatile int connectedClients = 0;

typedef struct
{
    int key;
    char value[BACKLOG];
    char payload[MAXLINE];
} dictionary_t;
dictionary_t clientBase[MAXCLIENTS_K_V];

typedef struct {
    size_t got;                 // ile bajtow structa juz mamy
    cliAnswer msg;              // bufor na cala strukture
} ClientState;

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

#define MAX_TOPIC_LEN  100
#define MAX_TOPIC_SUBS 100
typedef struct
{
    char topicFromServer[MAX_TOPIC_LEN];
    char payloadFromServer[MAXLINE];
}topicPayloadFromServer;
topicPayloadFromServer subsUpdateFromServer[MAX_TOPIC_SUBS];


// Demonizacja procesu
int demonize() {
    int i;
    pid_t pid;

    if ((pid = fork()) < 0) {
        return (-1);
    }
    else if (pid) {
        _exit(0); // Zakoncz proces rodzica
    }

    if (setsid() < 0) {
        return (-1); // Przejmij sesje
    }

    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    if ((pid = fork()) < 0) {
        return (-1);
    }
    else if (pid) {
        _exit(0); // Ponowne rozdzielenie aby odlaczyc od terminala
    }

    // chdir("/"); // zmien folder

    // Zamkniecie deskryptorow pliku
	long maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd < 0) maxfd = 1024;
    for (long i = 0; i < maxfd; i++) close((int)i);


	// Przekieruj do /dev/null
	open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);

	// openlog(pname, LOG_PID, facility);
	
	// setuid(uid); // zmiana uzytkownika

    return (0);
}


int main(int argc, char **argv)
{
    int listenfd, connfd, multicastfd;
    int epollfd, nready, currfd;
    int one = 1;
    struct sockaddr_in6 servaddr;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    char buff[MAXLINE], askBuff[MAXLINE];
    struct epoll_event events[MAXEVENTS];
    struct epoll_event ev;

    if (demonize() < 0) {
        perror("daemon");
    }

    // Mulicast
    if((multicastfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
    }

    setsockopt(multicastfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    
    struct sockaddr_in udp_bind;
    memset(&udp_bind, 0, sizeof(udp_bind));
    udp_bind.sin_family = AF_INET;
    udp_bind.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_bind.sin_port = htons(DISCOVERY_PORT);

    if (bind(multicastfd, (struct sockaddr*)&udp_bind, sizeof(udp_bind)) < 0) {
        perror("bind udp");
    }

    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr("239.1.2.3");      // your multicast group
    
    // #define SYLWEK_USER /// ZAKOMENTUJ TA LINIJKE JESLI NIE JESTES SYLWKIEM :) 
    #ifndef SYLWEK_USER
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    #endif
    #ifdef SYLWEK_USER
    mreq.imr_interface.s_addr = inet_addr("192.168.56.101");
    #endif

    if (setsockopt(multicastfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("IP_ADD_MEMBERSHIP");
    }

    MQTTpacket data;
    MQTTpacket packet = {"id_DEFAULT", DATA_PACKET, "topic_default", "payload_testowy"};

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

    // register tcp to epoll
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

    // register udp to epoll
    ev.events = EPOLLIN;
    ev.data.fd = multicastfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, multicastfd, &ev) == -1) {
    perror("epoll_ctl add multicastfd");
    exit(1);
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

            if (currfd == multicastfd) {
                struct sockaddr_in cli;
                socklen_t clen = sizeof(cli);
                char msg[256];

                ssize_t n = recvfrom(multicastfd, msg, sizeof(msg) - 1, 0,
                                    (struct sockaddr*)&cli, &clen);
                if (n < 0) {
                    if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
                        perror("recvfrom");
                    continue;
                }

                msg[n] = '\0';
                while (n > 0 && (msg[n-1] == '\n' || msg[n-1] == '\r')) msg[--n] = '\0';

                if (strcmp(msg, "mqttclient") == 0) {
                    // reply ONLY with TCP port; client uses source IP of this UDP reply
                    char reply[32];
                    snprintf(reply, sizeof(reply), "%d", PORT);

                    if (sendto(multicastfd, reply, strlen(reply), 0,
                            (struct sockaddr*)&cli, clen) < 0) {
                        perror("sendto");
                    }
                }
                continue;
            }


            if(currfd == listenfd)
            {
                peer_addr_len = sizeof(peer_addr);

                connfd = accept(listenfd, (SA*)&peer_addr, &peer_addr_len);
                connectedClients ++;
                
                
                if(connfd < 0)
                {
                    fprintf(stderr, "accept() error!: %s\n", strerror(errno));
                    connectedClients--;
                    continue;
                }

                if (connfd >= maxfds) {
                    close(connfd);
                    connectedClients--;
                    continue;
                }

                if (set_nonblocking(connfd) < 0) {
                    close(connfd);
                    connectedClients--;
                    continue;
                }

                /* reset stanu odbioru dla nowego klienta */
                st[connfd].got = 0;
                memset(&st[connfd].msg, 0, sizeof(st[connfd].msg));

                printf("Connected with client\n");

                // wiadomosc powitalna
                const char* welcomeMessage = "------------ MiniMQTT BROKER ------------ \n";
                const char* welcomeMessage2 =
                                                "      █████╗  ██████╗  ██╗  ██╗ \n"
                                                "     ██╔══██╗██╔════╝  ██║  ██║ \n"
                                                "     ███████║██║  ███╗ ███████║ \n"
                                                "     ██╔══██║██║   ██║ ██╔══██║ \n"
                                                "     ██║  ██║╚██████╔╝ ██║  ██║ \n"
                                                "     ╚═╝  ╚═╝ ╚═════╝  ╚═╝  ╚═╝ \n";

                const char* welcomeMessage3 =
                                                "\n"
                                                " AGH UNIVERSITY OF SCIENCE AND TECHNOLOGY\n"
                                                " Faculty of: Electronics and Telecomunications\n";
                const char* welcomeMessage4 =
                                                "\n"
                                                "Project authors:\n"
                                                "  - SYLWESTER SLUSARCZYK\n"
                                                "  - FILIP MROZIK\n";

                char fullMessage[MAXLINE];
                snprintf(fullMessage, MAXLINE, "%s%s%s%s",
                         welcomeMessage, welcomeMessage2, welcomeMessage3, welcomeMessage4);

                if (send(connfd, fullMessage, strlen(fullMessage), 0) < 0) {
                    fprintf(stderr, "send() error: %s\n", strerror(errno));
                    close(connfd);
                    connectedClients--;
                    continue;
                }

                // zarejestrowanie klienta w epoll dla zwielokrotnienia
                ev.events = EPOLLIN | EPOLLRDHUP;
                ev.data.fd = connfd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
                    close(connfd);
                    connectedClients--;
                    continue;
                }

                printf("Waiting for client response about action... \r\n");
                continue;
            }
            else
            {
                // rozlaczenie (RDHUP/HUP/ERR)
                if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    close(currfd);
                    connectedClients--;
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, currfd, NULL);
                    st[currfd].got = 0;
                    continue;
                }
            }

            if (!(events[i].events & EPOLLIN)) {
                continue;
                }

            // odbior tylko wtedy, gdy epoll da znac ze klient jest gotowy do odczytu
            while (1) {
                //size_t need = sizeof(cliAnswer) - st[currfd].got;
                ssize_t n = recv(currfd,
                                &st[currfd].msg,
                                sizeof(st[currfd].msg),
                                0);

                                printf("fd=%d recv=%zd got=%zu/%zu\n",
                                   currfd, n, st[currfd].got, sizeof(cliAnswer));


                if (n == 0) {
                    // klient rozlaczony
                    close(currfd);
                    connectedClients--;
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
                    connectedClients--;
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, currfd, NULL);
                    st[currfd].got = 0;
                    break;
                }

                st[currfd].got += (size_t)n;

                if (st[currfd].got == sizeof(cliAnswer)) {
                    cliAnswer cliAnswer = st[currfd].msg;

                    printf("Client option: %s\n", cliAnswer.answer);

                    //zapisanie do pliku json
                    cJSON *root = cJSON_CreateObject();
                    cJSON_AddStringToObject(root, "client_id", cliAnswer.client_id);
                    cJSON_AddNumberToObject(root, "type", (int)cliAnswer.type);
                    cJSON_AddStringToObject(root, "answer", cliAnswer.answer);
                    cJSON_AddStringToObject(root, "topic", cliAnswer.topic);
                    cJSON_AddStringToObject(root, "payload", cliAnswer.payload);
                    cJSON_AddNumberToObject(root, "fd", currfd);

                    char *json = cJSON_Print(root);
                    char jsonFilename[256];           /* Dodalem zeby kazda struktura komunikacji byla w osobnym pliku
                                                            nazwanym "pub/sub_{client_id}_{topic}.json */
                    

                    if(strcmp(cliAnswer.answer, "p") == 0)
                    {
                        printf("Client choose publish\n");

                        snprintf( jsonFilename, sizeof(jsonFilename),
                        "%s.json",cliAnswer.topic);
                        for(int i = 0; i < MAXCLIENTS_K_V; i++)
                        {
                            if(strcmp(cliAnswer.topic, clientBase[i].value) == 0)
                            {
                                snprintf(clientBase[i].payload, sizeof(clientBase[i].payload),
                                        "%s", cliAnswer.payload);
                                
                                snprintf(subsUpdateFromServer[i].topicFromServer,
                                        sizeof(subsUpdateFromServer[i].topicFromServer),
                                        "%s", clientBase[i].value);

                                snprintf(subsUpdateFromServer[i].payloadFromServer,
                                        sizeof(subsUpdateFromServer[i].payloadFromServer),
                                        "%s", clientBase[i].payload);
                                
                                printf("Succesfully added payload to topic: %s\npayload: %s\n",
                                        clientBase[i].value, clientBase[i].payload);
                                
                            
                                int sockFdToSend = clientBase[i].key;
                                if(send(sockFdToSend, &subsUpdateFromServer[i], sizeof(subsUpdateFromServer[i]), 0) < 0)
                                {
                                    fprintf(stderr, "\nFail to Send payload to subscriber\n");
                                }
                                sleep(1);
                            }
                        }

                    }
                    else if (strcmp(cliAnswer.answer, "s") == 0)
                    {
                        printf("Client choose subscribe\n");
                        clientBase[connectedClients-1].key = currfd;   /* zapis deskryptora klienta i topicu do naszej bazy danych subskrybcji klientow */
                        snprintf(
                            clientBase[connectedClients-1].value,
                                    sizeof(clientBase[connectedClients-1].value),
                                    "%s", cliAnswer.topic
                                );


                        printf("View from Client Database: key= %d, value=%s \n",
                                clientBase[connectedClients-1].key,
                                clientBase[connectedClients-1].value
                            );

                        for (int i = connectedClients-1 ; i >= 0; i--)
                        {
                            printf("\nView from Client Database: key= %d, value=%s \n",
                                clientBase[i].key,
                                clientBase[i].value
                            );

                        }
                                    
                        snprintf( jsonFilename, sizeof(jsonFilename),
                        "%s.json", cliAnswer.client_id);
                    }
                    else
                    {
                        printf("Client choose wrong\n");
                        snprintf( jsonFilename, sizeof(jsonFilename),
                        "FAIL_%s_%s.json", cliAnswer.client_id, cliAnswer.topic );
                    }
                

                    FILE *fp = fopen(jsonFilename, "w");
                    if (fp == NULL) {
                        perror("Failed to open file");
                    } else {
                        fprintf(fp, "%s\n", json);
                        fclose(fp);
                    }
    
                    free(json);
                    cJSON_Delete(root);

                    // przygotuj sie na kolejna strukture od tego samego klienta
                    st[currfd].got = 0;
                    memset(&st[currfd].msg, 0, sizeof(st[currfd].msg));
                }
            }
        }
    }
    
    close(listenfd);
    close(epollfd);
    free(st);

    return 0;
}
