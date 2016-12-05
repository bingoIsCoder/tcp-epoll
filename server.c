#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>

#define OPEN_MAX 100
#define PORT 8888
#define BACKLOG 1024
#define MAX_EVENTS 500
#define BUF_SIZE 1024
#define IP_ADDR "127.0.0.1"

void modify_event(int epollFd, int fd, int state);
void delete_event(int epollFd, int fd, int state);

int main(int argc, char *argv[])
{
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == socketFd)
    {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr, clientAddr;
    bzero(&serverAddr, sizeof(struct sockaddr_in));
    bzero(&clientAddr, sizeof(struct sockaddr_in));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(IP_ADDR);

    if (-1 == bind(socketFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)))
    {
        perror("bind()");
        close(socketFd);
        exit(EXIT_FAILURE);
    }
    if (-1 == listen(socketFd, BACKLOG))
    {
        perror("listen()");
        close(socketFd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event eventListenFd;
    struct epoll_event events[MAX_EVENTS];
    bzero(events, sizeof(events));
    bzero(&eventListenFd, sizeof(eventListenFd));
    eventListenFd.data.fd = socketFd;
    eventListenFd.events = EPOLLIN;

    char buf[BUF_SIZE];
    bzero(buf, sizeof(buf));

    int epollFd = epoll_create(OPEN_MAX); //argument is ignored, greater than 0 is ok.
    if (-1 == epollFd)
    {
        perror("epoll_create()");
        close(socketFd);
        exit(EXIT_FAILURE);
    }


    int ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, socketFd, &eventListenFd);
    if (-1 == ret)
    {
        perror("epoll_ctl()");
        close(socketFd);
        exit(EXIT_FAILURE);
    }

    int i;
    int fd;
    int connFd;
    socklen_t addrLen;
    struct epoll_event ev;
    bzero(&ev, sizeof(ev));
    while (1)
    {
        ret = epoll_wait(epollFd, events, MAX_EVENTS, -1);

        for (i = 0; i < MAX_EVENTS; i++)
        {
            fd = events[i].data.fd;

            if ((fd == socketFd) && (events[i].events & EPOLLIN))
            {
               connFd = accept(socketFd, (struct sockaddr *)&clientAddr, &addrLen);
               if (connFd == -1)
               {
                   perror("accept()");
                   continue;
               }
               else
               {
                   printf("Accept a new client: %s:%d\n", inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port);
                   ev.data.fd = connFd;
                   ev.events = EPOLLIN;
                   epoll_ctl(epollFd, EPOLL_CTL_ADD, connFd, &ev);
               }
            }
            else if (events[i].events & EPOLLIN)
            {
                ret = read(fd, buf, BUF_SIZE);
                if(ret == -1)
                {
                    perror("read()");
                    close(fd);
                    delete_event(epollFd, fd, EPOLLIN);
                }
                else if(ret == 0)
                {
                    fprintf(stderr, "Client closed.\n");
                    close(fd);
                    delete_event(epollFd, fd, EPOLLIN);
                }
                else
                {
                    printf("Read message: %s\n", buf);
                    modify_event(epollFd, fd, EPOLLOUT);
                }
                bzero(buf, BUF_SIZE);

            }
            else if (events[i].events & EPOLLOUT)
            {
                ret = write(fd, buf, strlen(buf));
                if (ret == -1)
                {
                    perror("write()");
                    close(fd);
                    delete_event(epollFd, fd, EPOLLOUT);
                }
                else
                    modify_event(epollFd, fd, EPOLLIN);
                bzero(buf, BUF_SIZE);
            }
        }
    }

    return 0;
}

void delete_event(int epollFd, int fd, int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, &ev);
}

void modify_event(int epollFd, int fd, int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev);
}
