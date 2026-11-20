#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <signal.h>
#include "server.h"
#include "http.h"

#define DEFAULT_PORT "9090"
#define MAX_EVENTS 1024

int main(int argc, char *argv[])
{
    /*Ignoramos  SIGPIPE para que una descoexion abrupta no cierre el servidor*/
    signal(SIGPIPE, SIG_IGN);
    /*Podemos ejecutar ./minihttpd 8080 o por defecto usar 9090 */
    const char *service = (argc >= 2) ? argv[1] : DEFAULT_PORT;

    /*Usamos a server.c para crear el socket maestro con bind y listen , SOMAXCONN usa el tamaño permitido de cola por el OS*/
    int listenfd = passiveTCP(service, SOMAXCONN);
    if (listenfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /*Creamos la instancia de epoll en el kernel de linux*/
    int epfd = epoll_create1(0);
    if (epfd < 0)
    {
        errexit("epoll_create1: %s\n", strerror(errno));
    }

    /*EPOLLIN nos avisara cuando lleguen nuevas conexiones*/
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;

    /*Registramos listenfd en el monitor de epoll con EPOLL_CTL_ADD*/
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) < 0)
    {
        errexit("epoll_ctl(listenfd): %s\n", strerror(errno));
    }

    printf("MiniHTTPd escuchando en el puerto %s (epfd: %d)\n", service, epfd);

    /*Aqui el kernel pondra solo los sockets con actividad */
    struct epoll_event events[MAX_EVENTS];

    while (1)
    {
        /*Bloqueamos el proceso sin gastar CPU hasta tener actividad*/
        int nready = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nready < 0)
        {
            if (errno == EINTR)
                continue;
            errexit("epoll_wait: %s\n", strerror(errno));
        }

        /*Recorre descriptores que  despertaron*/
        for (int i = 0; i < nready; i++)
        {
            int fd = events[i].data.fd;

            if (fd == listenfd)
            {
                /*Acepta la conexion entrante y obtiene clientfd*/
                int clientfd = accept(listenfd, NULL, NULL);
                if (clientfd < 0)
                {
                    perror("accept");
                    continue;
                }

                /*Registramos el cliente en epoll para detectar cuando envie GET*/
                ev.events = EPOLLIN;
                ev.data.fd = clientfd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev) < 0)
                {
                    perror("epoll_ctl add clientfd");
                    close(clientfd);
                }
            }
            else
            {
                int keep_alive = handle_http_request(fd);

                /*Sin conexion persistente eliminamos el socket de epoll con EPOL_CTL_DEL y liberamos con close*/
                if (!keep_alive)
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }
            }
        }
    }

    close(listenfd);
    return 0;
}