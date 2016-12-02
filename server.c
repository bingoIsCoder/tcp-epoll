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
    int addrLen = sizeof(clientAddr);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

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

    struct epoll_event event;
    struct epoll_event wait_event;
    bzero(&event, sizeof(struct epoll_event));
    bzero(&wait_event, sizeof(struct epoll_event));

    int fd[OPEN_MAX];
    memset(fd, -1, sizeof(fd));
    fd[0] = socketFd;

    int epollFd = epoll_create(MAX_EVENTS); //argument is ignored, greater than 0 is ok.
    if (-1 == epollFd)
    {
        perror("epoll_create()");
        close(socketFd);
        exit(EXIT_FAILURE);
    }

    event.data.fd = socketFd;
    event.events = EPOLLIN;

    int ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, socketFd, &event);
    if (-1 == ret)
    {
        perror("epoll_ctl()");
        close(socketFd);
        exit(EXIT_FAILURE);
    }

    int maxEvents = 0;
    int fdNum = 0;
    while (true)
    {
        ret = epoll_wait(epollFd, &wait_event, maxEvents + 1, -1);

        if ((wait_event.data.fd == socketFd) &&
                (EPOLLIN == wait_event.events))
        {
            int connFd = accept(socketFd, (struct sockaddr *)&clientAddr, &addrLen);
            for (fdNum = 0; fdNum < OPEN_MAX; fdNum++)
            {
                if (fd[fdNum] < 0)
                {
                    fd[fdNum] = connFd;
                    event.data.fd = connFd;
                    event.events = EPOLLIN;

                    ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, connFd, &event);
                    if (-1 == ret)
                    {
                        perror("epoll_ctl()2");
                        close(epollFd);
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
            }
        }

        if (fdNum > maxEvents)
            maxEvents = fdNum;
    }

    return 0;
}
