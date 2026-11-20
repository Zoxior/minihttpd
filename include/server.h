#ifndef SERVER_H
#define SERVER_H

int errexit(const char *format, ...);
int passivesock(const char *service, const char *transport, int qlen);
int passiveTCP(const char *service, int qlen);

#endif