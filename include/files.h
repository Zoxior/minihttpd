#ifndef FILES_H
#define FILES_H

#include <stddef.h>

typedef enum
{
    FILE_OK = 200,
    FILE_FORBIDDEN = 403,
    FILE_NOT_FOUND = 404,
    FILE_INTERNAL_ERROR = 500
} file_status_t;

file_status_t resolve_safe_path(const char *www_root, const char *uri,
                                char *resolved_out, size_t out_size,
                                size_t *file_size);

#endif