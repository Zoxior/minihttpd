#ifndef HTTP_H
#define HTTP_H

#define MAX_REQ_SIZE 8192
#define MAX_LINE_LEN 1024
#define MAX_URI_LEN 256
#define MAX_HEADER_LEN 512
#define WWW_ROOT "./www"

int handle_http_request(int fd);

#endif