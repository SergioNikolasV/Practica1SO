#include "comunication.h"

enum {
    MAX_SHOWN = 20,
    NUM_CRITERIA = 3,
};

static const int COL_BY_CRITERIA[] = {0, 1, 2, 10};

static int create_fifo(const char *path) {
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        perror(path);
        return -1;
    }
    return 0;
}

static int get_field(const char *line, int n, char *out, size_t size) {
    const char *p = line;
    for (int i = 1; i < n; i++) {
        p = strchr(p, ',');
        if (!p) return -1;
        p++;
    }

    const char *end = strchr(p, ',');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r')) len--;
    if (len >= size) len = size - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

static void search_dataset(const char *path, const Request *req, Response *resp) {
    resp->status = 0;
    resp->result[0] = '\0';

    if (req->criteria < 1 || req->criteria > NUM_CRITERIA) {
        resp->status = 1;
        snprintf(resp->result, MAX_CAPACITY, "Criterio de busqueda invalido");
        return;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        resp->status = 1;
        snprintf(resp->result, MAX_CAPACITY, "No se pudo abrir el dataset: %s", strerror(errno));
        return;
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    size_t used = 0;
    int first = 1;
    int total = 0;
    int shown = 0;
    char *header = NULL;
    char field[256];
    int col = COL_BY_CRITERIA[req->criteria];

    while ((n = getline(&line, &cap, file)) != -1) {
        if (first) {
            first = 0;
            header = strdup(line);
            continue;
        }

        if (get_field(line, col, field, sizeof(field)) != 0) continue;
        if (strcmp(field, req->query) != 0) continue;

        total++;
        if (shown == 0 && header) {
            size_t header_len = strlen(header);
            if (header_len < MAX_CAPACITY - 128) {
                memcpy(resp->result, header, header_len);
                used = header_len;
                resp->result[used] = '\0';
            }
        }

        if (shown < MAX_SHOWN && used + (size_t)n + 1 < MAX_CAPACITY - 64) {
            memcpy(resp->result + used, line, (size_t)n);
            used += (size_t)n;
            resp->result[used] = '\0';
            shown++;
        }
    }

    if (total > shown) {
        snprintf(resp->result + used, MAX_CAPACITY - used,
                 "... mostrando %d de %d coincidencias\n", shown, total);
    }

    free(header);
    free(line);
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo.csv>\n", argv[0]);
        return 1;
    }

    if (create_fifo(FIFO_REQUEST) == -1 || create_fifo(FIFO_RESPONSE) == -1) {
        unlink(FIFO_REQUEST);
        unlink(FIFO_RESPONSE);
        return 1;
    }

    printf("Servidor FIFO activo (PID %d). Dataset: %s\n", getpid(), argv[1]);
    printf("Esperando cliente...\n");

    int req_fd = open(FIFO_REQUEST, O_RDONLY);
    if (req_fd == -1) {
        perror("Error abriendo FIFO de solicitudes");
        unlink(FIFO_REQUEST);
        unlink(FIFO_RESPONSE);
        return 1;
    }

    int resp_fd = open(FIFO_RESPONSE, O_WRONLY);
    if (resp_fd == -1) {
        perror("Error abriendo FIFO de respuestas");
        close(req_fd);
        unlink(FIFO_REQUEST);
        unlink(FIFO_RESPONSE);
        return 1;
    }

    printf("Cliente conectado.\n");

    while (1) {
        Request req;
        Response resp;
        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));

        ssize_t n = read_full(req_fd, &req, sizeof(req));
        if (n == 0) {
            printf("El cliente cerro la FIFO de solicitudes.\n");
            break;
        }
        if (n != (ssize_t)sizeof(req)) {
            if (n == -1) perror("Error leyendo solicitud desde FIFO");
            else fprintf(stderr, "Solicitud incompleta recibida.\n");
            break;
        }

        if (req.criteria == CRIT_EXIT) {
            resp.status = 2;
            snprintf(resp.result, MAX_CAPACITY, "Servidor finalizado por solicitud del cliente.");
            if (write_full(resp_fd, &resp, sizeof(resp)) != (ssize_t)sizeof(resp)) {
                perror("Error enviando confirmacion de salida");
            }
            break;
        }

        search_dataset(argv[1], &req, &resp);

        if (write_full(resp_fd, &resp, sizeof(resp)) != (ssize_t)sizeof(resp)) {
            perror("Error escribiendo respuesta en FIFO");
            break;
        }
    }

    close(resp_fd);
    close(req_fd);
    unlink(FIFO_REQUEST);
    unlink(FIFO_RESPONSE);
    printf("Servidor FIFO finalizado. Recursos eliminados.\n");
    return 0;
}
