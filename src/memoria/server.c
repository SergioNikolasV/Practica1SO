#include "common.h"

enum {
    MAX_SHOWN = 20,
    NUM_CRITERIA = 3,
};

static const int COL_BY_CRITERIA[] = {0, 1, 2, 10};

// Banderín atómico para registrar señal de apagado
static volatile sig_atomic_t stop = 0;

static void on_signal(int s) {
    (void)s;
    stop = 1;
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

void search_dataset(const char *path, const Request *rq, Response *rs) {
    rs->status = 0;
    rs->result[0] = '\0';

    if (rq->criteria < 1 || rq->criteria > NUM_CRITERIA) {
        rs->status = 1;
        snprintf(rs->result, MAX_CAPACITY, "Criterio de busqueda invalido");
        return;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        rs->status = 1;
        snprintf(rs->result, MAX_CAPACITY, "No se pudo abrir el dataset: %s", strerror(errno));
        return;
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    size_t used = 0;
    int first = 1, total = 0, shown = 0;
    char *header = NULL;
    char field[256];
    int col = COL_BY_CRITERIA[rq->criteria];

    while ((n = getline(&line, &cap, file)) != -1) {
        if (first) {
            first = 0;
            header = strdup(line);
            continue;
        }
        if (get_field(line, col, field, sizeof(field)) != 0) continue;
        if (strcmp(field, rq->query) != 0) continue;

        total++;
        if (shown == 0 && header) {
            size_t hl = strlen(header);
            if (hl < MAX_CAPACITY - 128) {
                memcpy(rs->result, header, hl);
                used = hl;
                rs->result[used] = '\0';
            }
        }
        if (shown < MAX_SHOWN && used + (size_t)n + 1 < MAX_CAPACITY - 64) {
            memcpy(rs->result + used, line, n);
            used += n;
            rs->result[used] = '\0';
            shown++;
        }
    }

    if (total > shown) {
        snprintf(rs->result + used, MAX_CAPACITY - used,
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

    // Registrar manejador de señal para cierre limpio (Ctrl+C)
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);


    int shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Error en shmget");
        return 1;
    }

    SharedData *sh = shmat(shmid, NULL, 0);
    if (sh == (void *)-1) {
        perror("Error en shmat");
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    // Limpiamos nombres anteriores por precaución antes de crear
    sem_unlink(SEM_REQ);
    sem_unlink(SEM_RESP);

    sem_t *sem_req  = sem_open(SEM_REQ, O_CREAT, 0666, 0);
    sem_t *sem_resp = sem_open(SEM_RESP, O_CREAT, 0666, 0);
    if (sem_req == SEM_FAILED || sem_resp == SEM_FAILED) {
        perror("Error en sem_open");
        shmdt(sh);
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    printf("Servidor activo (PID %d). Dataset: %s\n", getpid(), argv[1]);
    printf("Presione Ctrl+C para finalizar el servidor.\n");

    // Bucle principal de atención a peticiones
    while (!stop) {
        if (sem_wait(sem_req) == -1) {
            if (errno == EINTR) continue;
            perror("Error en sem_wait");
            break;
        }

        search_dataset(argv[1], &sh->req, &sh->resp);

        if (sem_post(sem_resp) == -1) {
            perror("Error en sem_post");
            break;
        }
    }

    printf("\nCerrando servidor y liberando recursos IPC...\n");

    // Liberación estricta de recursos
    shmdt(sh);
    shmctl(shmid, IPC_RMID, NULL);
    sem_close(sem_req);
    sem_close(sem_resp);
    sem_unlink(SEM_REQ);
    sem_unlink(SEM_RESP);

    printf("Servidor finalizado con exito.\n");
    return 0;
}