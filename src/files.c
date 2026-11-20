#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "files.h"

file_status_t resolve_safe_path(const char *www_root, const char *uri,
                                char *resolved_out, size_t out_size,
                                size_t *file_size)
{
    char root_real[PATH_MAX];   /*ruta real */
    char candidate[PATH_MAX];   /*root real y uri*/
    char target_real[PATH_MAX]; /*ruta despues de resolver enlaces y ".."*/
    struct stat st;

    /*./www la tenemos como ruta y si falla o no existe manda un error 500*/
    if (realpath(www_root, root_real) == NULL)
    {
        return FILE_INTERNAL_ERROR;
    }

    /*rechazamos con 403 directo al ver en la URI ".." */
    if (strstr(uri, "..") != NULL)
    {
        return FILE_FORBIDDEN;
    }

    /*usamos snprintf con la ruta y evitamos buffer Overflows*/
    int written;
    if (strcmp(uri, "/") == 0)
    {
        written = snprintf(candidate, sizeof(candidate), "%s/index.html", root_real);
    }
    else
    {
        written = snprintf(candidate, sizeof(candidate), "%s%s", root_real, uri);
    }

    if (written < 0 || (size_t)written >= sizeof(candidate))
    {
        return FILE_FORBIDDEN;
    }

    /*realpath() consulta al sistema de archivos, elimina saltos y sigue enlaces simbolicos*/
    if (realpath(candidate, target_real) == NULL)
    {
        if (errno == ENOENT || errno == ENOTDIR)
        {
            return FILE_NOT_FOUND;
        }
        if (errno == EACCES)
        {
            return FILE_FORBIDDEN;
        }
        return FILE_INTERNAL_ERROR;
    }

    /*Comprobamos que target_real empiece con root real y el caracter siguiente sea / */
    size_t root_len = strlen(root_real);
    if (strncmp(target_real, root_real, root_len) != 0 ||
        (target_real[root_len] != '/' && target_real[root_len] != '\0'))
    {
        return FILE_FORBIDDEN;
    }

    if (stat(target_real, &st) < 0)
    {
        return FILE_INTERNAL_ERROR;
    }

    /*Rechaza los directorios y archivos sin permiso de lectura con 403*/
    if (!S_ISREG(st.st_mode) || access(target_real, R_OK) < 0)
    {
        return FILE_FORBIDDEN;
    }

    int out_len = snprintf(resolved_out, out_size, "%s", target_real);
    if (out_len < 0 || (size_t)out_len >= out_size)
    {
        return FILE_INTERNAL_ERROR;
    }
    /*usaremos en content-length*/

    *file_size = (size_t)st.st_size;
    return FILE_OK;
}