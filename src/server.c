#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include "server.h"

/*En 0 para que el usuario elija su puerto en consola*/
unsigned short portbase = 0;

/*Imprime un error en la salida de errores y termina el servidor*/
int errexit(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}

/*Se crea, enlaza y pone en modo pasivo un socket */
int passivesock(const char *service, const char *transport, int qlen)
{
    struct servent *pse;
    struct protoent *ppe;
    struct sockaddr_in sin;
    int s, type;
    int opt = 1;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;

    if ((pse = getservbyname(service, transport)))
        sin.sin_port = htons(ntohs((unsigned short)pse->s_port) + portbase);
    else if ((sin.sin_port = htons((unsigned short)atoi(service))) == 0)
        errexit("Error: No se pudo obtener el servicio o puerto \"%s\"\n", service);

    if ((ppe = getprotobyname(transport)) == 0)
        errexit("Error: No se pudo obtener el protocolo \"%s\"\n", transport);

    if (strcmp(transport, "udp") == 0)
        type = SOCK_DGRAM;
    else
        type = SOCK_STREAM;

    /*Se crea el socket y devuelve su descriptor*/
    s = socket(PF_INET, type, ppe->p_proto);
    if (s < 0)
        errexit("Error: No se pudo crear el socket. Motivo: %s\n", strerror(errno));

    /*Permite reutilizar el puerto tras reiniciar*/
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /*bind enlaza al socket con la IP y el puerto*/
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errexit("Error: No se pudo enlazar al puerto %s. Motivo: %s\n", service, strerror(errno));

    /*TCP marca el socket como pasivo "listen" y qlen indica las conexiones a esperar en cola*/
    if (type == SOCK_STREAM && listen(s, qlen) < 0)
        errexit("Error: No se pudo escuchar en el puerto %s. Motivo: %s\n", service, strerror(errno));

    return s;
}

/*llama a passivesock() con tcp */
int passiveTCP(const char *service, int qlen)
{
    return passivesock(service, "tcp", qlen);
}