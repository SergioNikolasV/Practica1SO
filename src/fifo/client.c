#include "comunication.h"

static void print_menu(void) {
    printf("\n=== Consulta de canciones por FIFO ===\n");
    printf("1. Buscar por track_id\n");
    printf("2. Buscar por genero\n");
    printf("3. Buscar por label_tier\n");
    printf("0. Salir y finalizar servidor\n");
    printf("Opcion: ");
    fflush(stdout);
}

static int send_request(int req_fd, int resp_fd, const Request *req, Response *resp) {
    if (write_full(req_fd, req, sizeof(*req)) != (ssize_t)sizeof(*req)) {
        perror("Error escribiendo solicitud en FIFO");
        return -1;
    }

    if (read_full(resp_fd, resp, sizeof(*resp)) != (ssize_t)sizeof(*resp)) {
        perror("Error leyendo respuesta desde FIFO");
        return -1;
    }

    return 0;
}

int main(void) {
    int req_fd = open(FIFO_REQUEST, O_WRONLY | O_NONBLOCK);
    if (req_fd == -1) {
        if (errno == ENOENT) {
            fprintf(stderr, "Error: las FIFO no existen. Inicie primero el servidor.\n");
        } else if (errno == ENXIO) {
            fprintf(stderr, "Error: el servidor no esta leyendo la FIFO de solicitudes.\n");
        } else {
            perror("Error abriendo FIFO de solicitudes");
        }
        return 1;
    }

    int flags = fcntl(req_fd, F_GETFL);
    if (flags == -1 || fcntl(req_fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        perror("Error configurando FIFO de solicitudes");
        close(req_fd);
        return 1;
    }

    int resp_fd = open(FIFO_RESPONSE, O_RDONLY);
    if (resp_fd == -1) {
        perror("Error abriendo FIFO de respuestas");
        close(req_fd);
        return 1;
    }

    printf("Cliente FIFO activo (PID %d).\n", getpid());

    char buf[256];
    int option;

    while (1) {
        print_menu();
        if (!fgets(buf, sizeof(buf), stdin)) break;

        if (sscanf(buf, "%d", &option) != 1) {
            printf("Opcion invalida.\n");
            continue;
        }

        if (option < 0 || option > 3) {
            printf("Opcion invalida.\n");
            continue;
        }

        Request req;
        Response resp;
        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));
        req.criteria = option;

        if (option == CRIT_EXIT) {
            if (send_request(req_fd, resp_fd, &req, &resp) == 0) {
                printf("%s\n", resp.result);
            }
            break;
        }

        printf("Valor a buscar: ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\r\n")] = '\0';

        if (buf[0] == '\0') {
            printf("El valor no puede estar vacio.\n");
            continue;
        }

        snprintf(req.query, sizeof(req.query), "%s", buf);

        if (send_request(req_fd, resp_fd, &req, &resp) != 0) break;

        if (resp.status != 0) {
            printf("\nError del servidor: %s\n", resp.result);
        } else if (resp.result[0] == '\0') {
            printf("\nNo se encontraron resultados.\n");
        } else {
            printf("\n%s", resp.result);
        }
    }

    close(resp_fd);
    close(req_fd);
    return 0;
}
