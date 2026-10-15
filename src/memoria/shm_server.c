#define _POSIX_C_SOURCE 200809L 

#include "shm_comun.h"
#include "../comun/search.h"

#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static volatile sig_atomic_t shutdown_requested;
static sem_t *sem_req_signal;

static void solicitar_cierre(int signal_number) {
    (void)signal_number;
    shutdown_requested = 1;
    if (sem_req_signal != NULL) {
        sem_post(sem_req_signal);
    }
}

static void delete_names(void) {
    sem_unlink(NOMBRE_REQ);
    sem_unlink(NOMBRE_RESP);
    shm_unlink(NOMBRE_SHM);
}

static int waiting(sem_t *s) {
    while (sem_wait(s) == -1) {
        if (errno != EINTR) {
            perror("sem_wait");
            return -1;
        }
    }
    return 0;
}

static void limpiar_respuesta(Compartido *p) {
    p->num_reg = 0;
    p->hay_mas = 0;
    p->cod_error = 0;
    memset(p->reg, 0, sizeof(p->reg));
}

static void copiar_resultado(Compartido *p, SearchStatus estado,
                             const SearchResult *resultado) {
    limpiar_respuesta(p);
    if (resultado->num_filas < 0 || resultado->num_filas > SHM_MAX_REGISTROS) {
        p->state = RESPUESTA_ERROR;
        p->cod_error = EOVERFLOW;
        return;
    }

    p->num_reg = resultado->num_filas;
    p->hay_mas = resultado->hay_mas;
    p->cod_error = resultado->codigo_error;
    for (int i = 0; i < p->num_reg; i++) {
        memcpy(p->reg[i], resultado->filas[i], sizeof(p->reg[i]));
    }

    switch (estado) {
        case SEARCH_OK:
            p->state = RESPUESTA_OK;
            break;
        case SEARCH_SIN_RESULTADOS:
            p->state = RESPUESTA_SIN_RESULTADOS;
            break;
        case SEARCH_ERROR:
        default:
            p->state = RESPUESTA_ERROR;
            break;
    }
}

static void responder_error(Compartido *p, int codigo_error) {
    limpiar_respuesta(p);
    p->state = RESPUESTA_ERROR;
    p->cod_error = codigo_error;
}

int main(int argc, char **argv) {


    if (argc > 2) {
        fprintf(stderr, "Uso: %s [ruta_csv]\n", argv[0]);
        return EXIT_FAILURE;
    }

    SearchContext *busqueda = search_open(argc == 2 ? argv[1] : NULL);
    if (busqueda == NULL) {
        perror(argc == 2 ? argv[1] : SEARCH_CSV_DEFAULT);
        return EXIT_FAILURE;
    }

    int fd = shm_open(NOMBRE_SHM, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) {
        perror("shm_open");
        search_close(busqueda);
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, sizeof(Compartido)) == -1) {
        perror("ftruncate");
        close(fd);
        shm_unlink(NOMBRE_SHM);
        search_close(busqueda);
        return EXIT_FAILURE;
    }

    Compartido *p = mmap(NULL, sizeof(Compartido), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_unlink(NOMBRE_SHM);
        search_close(busqueda);
        return EXIT_FAILURE;
    }

    close(fd);
    memset(p, 0, sizeof(*p));

    printf(" Servidor iniciado con PID: %d, esperando solicitudes...\n", getpid());
    fflush(stdout);

    sem_t *sem_req = sem_open(NOMBRE_REQ, O_CREAT, 0600, 0);
    sem_t *sem_res = sem_open(NOMBRE_RESP, O_CREAT, 0600, 0);
    if (sem_req == SEM_FAILED || sem_res == SEM_FAILED) {
        perror("sem_open");
        if (sem_req != SEM_FAILED) sem_close(sem_req);
        if (sem_res != SEM_FAILED) sem_close(sem_res);
        munmap(p, sizeof(Compartido));
        search_close(busqueda);
        delete_names();
        return EXIT_FAILURE;
    }

    struct sigaction accion = {0};
    accion.sa_handler = solicitar_cierre;
    sigemptyset(&accion.sa_mask);
    sem_req_signal = sem_req;
    if (sigaction(SIGINT, &accion, NULL) == -1 || sigaction(SIGTERM, &accion, NULL) == -1) {
        perror("sigaction");
        sem_req_signal = NULL;
        sem_close(sem_req);
        sem_close(sem_res);
        munmap(p, sizeof(Compartido));
        search_close(busqueda);
        delete_names();
        return EXIT_FAILURE;
    }

    int terminar = 0;
    while (!terminar) {
        if (waiting(sem_req) == -1 || shutdown_requested) {
            break;
        }

        SearchResult resultado;
        SearchStatus estado;
        switch (p->tipo_solicitud) {
            case SOLICITUD_TRACK_ID:
                estado = search_track_id(busqueda, p->par1, &resultado);
                copiar_resultado(p, estado, &resultado);
                break;
            case SOLICITUD_GENRE:
                estado = search_genre_start(busqueda, p->par1, p->tam_bloque, &resultado);
                copiar_resultado(p, estado, &resultado);
                break;
            case SOLICITUD_LISTAR_TRACKS:
                estado = search_track_catalog_start(busqueda, &resultado);
                copiar_resultado(p, estado, &resultado);
                break;
            case SOLICITUD_LISTAR_GENRES:
                estado = search_genres_start(busqueda, &resultado);
                copiar_resultado(p, estado, &resultado);
                break;
            case SOLICITUD_SIGUIENTE:
                estado = search_next(busqueda, &resultado);
                copiar_resultado(p, estado, &resultado);
                break;
            case SOLICITUD_TERMINAR:
                limpiar_respuesta(p);
                p->state = RESPUESTA_TERMINADO;
                terminar = 1;
                break;
            default:
                responder_error(p, EINVAL);
                break;
        }

        if (sem_post(sem_res) == -1) {
            perror("sem_post");
            break;
        }
    }

    search_close(busqueda);
    sem_req_signal = NULL;
    sem_close(sem_req);
    sem_close(sem_res);
    munmap(p, sizeof(Compartido));
    delete_names();

    return 0;
}
