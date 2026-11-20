#include <string.h>
#include "mime.h"

struct mime_entry
{
    const char *ext;
    const char *type;
};

/*Tipos de mime soportados solo lectura*/
static const struct mime_entry mime_table[] = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {NULL, NULL}};

/*Devuelve el Content_Type de la extension en la tabla*/
const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
    {
        return "application/octet-stream";
    }
    for (int i = 0; mime_table[i].ext != NULL; i++)
    {
        if (strcmp(ext, mime_table[i].ext) == 0)
        {
            return mime_table[i].type;
                }
    }
    return "application/octet-stream";
}