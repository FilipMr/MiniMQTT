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

#define PORT 8888

int setup_socket(void);



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

    if((listenfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
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
                n = recvfrom(listenfd, buff, MAXLINE, 0, (SA *)&peer_addr, &peer_addr_len);
                if(n < 0)
                {
                    fprintf(stderr, "recvfrom() error!: %s\n", strerror(errno));
                    continue;
                }

                char host[NI_MAXHOST], service[NI_MAXSERV]; // stae do funkcji getnameinfo(), czest standardowej bibliteka POSIX
                int s = getnameinfo((SA* )&peer_addr, peer_addr_len, host,
                                 NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICHOST);
                
                if (s == 0)
			      printf("UDP: Received %ld bytes from %s:%s\n",
				(long) n, host, service);
                else
                    fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

                if (sendto(listenfd, buff, n, 0, (SA *) &peer_addr,
                                        peer_addr_len) != n)
                    fprintf(stderr, "UDP Error sending response\n");

                continue; 
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