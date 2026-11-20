# MiniHTTPd

Proyecto de la materia Computación Distribuida con el objetivo de afianzar los conceptos y temas vistos durante clases con el fin de desarrollar un servidor web HTTP/1.1 estático escrito en C para Linux, implementado mediante la API de sockets POSIX y multiplexación de E/S con `epoll` en un solo hilo, sin bibliotecas HTTP externas.

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

- **Concurrencia:** un solo hilo basado en eventos con `epoll_create1()`, `epoll_ctl()` y `epoll_wait()`, manteniendo conexiones persistentes (`keep-alive`) y usando `signal(SIGPIPE, SIG_IGN)` como prevención ante desconexiones inesperadas.
- **Socket pasivo:** `SO_REUSEADDR` habilitado para reinicios inmediatos tras cerrar el servidor y `portbase = 0` para enlazar el puerto exacto indicado por consola.
- **Protocolo HTTP/1.1 y HTTP/1.0:**
  - Método soportado: `GET` (otros métodos devuelven `405 Method Not Allowed`).
  - Encabezados analizados sin distinguir mayúsculas (`strcasecmp()`): `Host` (obligatorio en HTTP/1.1), `Connection` y `User-Agent`.
  - Delimitación de respuestas mediante `Content-Length` obtenido con `stat()`.
- **Seguridad:**
  - Prevención de *Directory Traversal* mediante rechazo de `..`, resolución  con `realpath()`, verificación de prefijo con `strncmp()` y comprobación de archivo regular y permiso de lectura (`S_ISREG` y `access(..., R_OK)`).
  - Prevención de desbordamientos de búfer evitando `strcpy` y `sprintf`; se emplean `snprintf()`, `sscanf()` con límites de ancho y topes máximos (`MAX_REQ_SIZE=8192`, `MAX_LINE_LEN=1024`, `MAX_URI_LEN=256`, `MAX_HEADER_LEN=512`).   

- **Códigos de estado y tipos MIME**

    | Código | Respuesta |
    | :--- | :--- |
    | `200` | OK |
    | `400` | Bad Request |
    | `403` | Forbidden |
    | `404` | Not Found |
    | `405` | Method Not Allowed |
    | `500` | Internal Server Error |

    | Extensión | Content-Type |
    | :--- | :--- |
    | `.html` | `text/html` |
    | `.css` | `text/css` |
    | `.js` | `application/javascript` |
    | `.png` | `image/png` |
    | `.jpg` | `image/jpeg` |
    | Otros | `application/octet-stream` |


## Pruebas de verificación

Con el servidor ejecutándose en el puerto `9090`, se pueden reproducir las pruebas principales desde otra terminal:

### Servicio estático, tipos MIME e integridad

```bash
curl -s -o /dev/null -w "%{http_code} %{content_type}\n" localhost:9090/
curl -s -o /dev/null -w "%{http_code} %{content_type}\n" localhost:9090/style.css
curl -s -o /dev/null -w "%{http_code} %{content_type}\n" localhost:9090/image.png
curl -s localhost:9090/image.png | cmp - www/image.png   
```

### Conexión persistente `keep-alive` y cabeceras

```bash
curl -v localhost:9090/ localhost:9090/style.css 2>&1 | grep -iE "connection|re-using|HTTP/1"
printf 'GET / HTTP/1.0\r\n\r\n' | nc -w2 localhost 9090 | head -4
```

### Códigos de error y validaciones

```bash
curl -s -o /dev/null -w "%{http_code}\n" localhost:9090/noexiste          # 404
curl -s -o /dev/null -w "%{http_code}\n" -X POST localhost:9090/          # 405
printf 'GET / HTTP/1.1\r\n\r\n' | nc -w2 localhost 9090                   # 400 (sin Host)
printf 'GET / HTTP/1.1\r\nHost x\r\n\r\n' | nc -w2 localhost 9090         # 400 (cabecera sin ':')
```

### Seguridad (*Directory Traversal* y límites de tamaño)

```bash
curl --path-as-is -s -o /dev/null -w "%{http_code}\n" localhost:9090/../../etc/passwd  # 403
ln -s /etc/passwd www/link.html
curl -s -o /dev/null -w "%{http_code}\n" localhost:9090/link.html                      # 403
rm www/link.html
curl -s -o /dev/null -w "%{http_code}\n" "localhost:9090/$(head -c 300 /dev/zero | tr '\0' 'a')"  # 400
```

### Concurrencia

```bash
for i in $(seq 100); do curl -s -o /dev/null -w "%{http_code}\n" localhost:9090/ & done | sort | uniq -c; wait
```

## Limitaciones conocidas

- **Sockets bloqueantes:** `write_all()` utiliza escritura bloqueante, por eso con la transferencia lenta de un archivo grande logra retener al único hilo del servidor hasta terminar o cerrarse la conexión.
- **Lectura única por solicitud:** `handle_http_request()` realiza un solo `read()`, por lo que una solicitud dividida en fragmentos devuelve `400` y ante dos solicitudes enviadas juntas *pipelining* solo se procesa la primera.
- **Sin límites de inactividad:** no se aplican *timeouts* a conexiones `keep-alive` inactivas ni un tope interno de conexiones antes de agotar el límite de descriptores `ulimit -n`.
- **Rutas:** no se recortan *query strings* `/?x=1` devuelve `404`, no se realiza decodificación porcentual `%20`, `%2e` y el filtro de `..` rechaza archivos legítimos que incluyan doble punto en su nombre.

