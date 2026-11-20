# MiniHTTPd

Proyecto de la materia Computación Distribuida con el objetivo de afianzar los conceptos y temas vistos durante clases con el din de desarrollar un servidor web HTTP/1.1 estático escrito en C para Linux, implementado mediante la API de sockets POSIX y multiplexación de E/S con `epoll` en un solo hilo, sin bibliotecas HTTP externas.

## Estructura del proyecto

```text
minihttpd/
├── Makefile
├── README.md
├── include/
│   ├── files.h
│   ├── http.h
│   ├── mime.h
│   └── server.h
├── src/
│   ├── main.c      # Bucle de eventos con epoll y gestión de descriptores
│   ├── server.c    # Creación de socket pasivo TCP (passivesock, passiveTCP, errexit)
│   ├── http.c      # Parsing de solicitudes GET, cabeceras y envío de respuestas
│   ├── files.c     # Resolución segura de rutas dentro de ./www
│   └── mime.c      # Mapeo de extensiones a Content-Type
└── www/
    ├── index.html
    ├── style.css
    └── image.png
```
## Requisitos y compilación

Requiere un entorno Linux (o WSL) con `gcc` y `make`.

```bash
make
```

Para limpiar los archivos objeto y el binario generado:

```bash
make clean
```

El `Makefile` compila con `-Wall -Wextra -Iinclude -g`.

## Ejecución

El servidor debe ejecutarse desde la raíz del repositorio, ya que el directorio de archivos estáticos está definido de forma relativa en `WWW_ROOT` (`./www`).

```bash
# Puerto por defecto (9090)
./minihttpd

# Puerto personalizado
./minihttpd 8080
```

## Características implementadas

- **Concurrencia:** un solo hilo basado en eventos con `epoll_create1()`, `epoll_ctl()` y `epoll_wait()`, manteniendo conexiones persistentes (`keep-alive`) y usando `signal(SIGPIPE, SIG_IGN)` como prevención ante desconexiones abruptas.
- **Socket pasivo:** `SO_REUSEADDR` habilitado para reinicios inmediatos tras cerrar el servidor y `portbase = 0` para enlazar el puerto exacto indicado por consola.
- **Protocolo HTTP/1.1 y HTTP/1.0:**
  - Método soportado: `GET` (otros métodos devuelven `405 Method Not Allowed`).
  - Encabezados analizados sin distinguir mayúsculas (`strcasecmp()`): `Host` (obligatorio en HTTP/1.1), `Connection` y `User-Agent`.
  - Delimitación de respuestas mediante `Content-Length` obtenido con `stat()`.
- **Códigos de estado:** `200 OK`, `400 Bad Request`, `403 Forbidden`, `404 Not Found`, `405 Method Not Allowed` y `500 Internal Server Error`.
- **Tipos MIME:** `.html` (`text/html`), `.css` (`text/css`), `.js` (`application/javascript`), `.png` (`image/png`), `.jpg` (`image/jpeg`) y `application/octet-stream` por defecto.
- **Seguridad:**
  - Prevención de *Directory Traversal* mediante rechazo de `..`, resolución  con `realpath()`, verificación de prefijo con `strncmp()` y comprobación de archivo regular y permiso de lectura (`S_ISREG` y `access(..., R_OK)`).
  - Prevención de desbordamientos de búfer evitando `strcpy` y `sprintf`; se emplean `snprintf()`, `sscanf()` con límites de ancho y topes máximos (`MAX_REQ_SIZE=8192`, `MAX_LINE_LEN=1024`, `MAX_URI_LEN=256`, `MAX_HEADER_LEN=512`).



