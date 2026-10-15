#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "shm_comun.h"
#include <semaphore.h>
#include <errno.h>
#include <string.h>

void printdata(Compartido *p) {
    printf("Tipo de solicitud: %d\n", p->tipo_solicitud);
    printf("Parametro 1: %s\n", p->par1);
    printf("Parametro 2: %s\n", p->par2);
    printf("Estado: %d\n", p->state);
    printf("Numero de registros: %d\n", p->num_reg);
    printf("Registro: %s\n", p->reg);   
    printf("Codigo de error: %d\n", p->cod_error);
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

int main(void) {

    int fd = shm_open(NOMBRE_SHM, O_RDWR, 0600);
    if (fd == -1){
        perror("shm_open");
        return EXIT_FAILURE;
    }

    Compartido *p = mmap(NULL, sizeof(Compartido), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);

    sem_t *sem_req = sem_open(NOMBRE_REQ, O_CREAT, 0600, 0);
    sem_t *sem_res = sem_open(NOMBRE_RESP, O_CREAT, 0600, 0);
    if (sem_req == SEM_FAILED || sem_res == SEM_FAILED) {
        perror("sem_open");
        munmap(p, sizeof(Compartido));
        shm_unlink(NOMBRE_SHM);
        return EXIT_FAILURE;
    }

    for (;;) {
        char mensaje[100];
        if (!fgets(mensaje, sizeof(mensaje), stdin)){ /* 0. leo */
            perror("fgets");
            munmap(p, sizeof(Compartido));
            shm_unlink(NOMBRE_SHM);
            return EXIT_FAILURE;
        } else if (strcmp(mensaje, "Escaparrr\n") == 0) {
            break;
        }

        snprintf(p->par1, sizeof(p->par1), "%s", mensaje);  /* 1. escribo */
        sem_post(sem_req);                                          /* 2. aviso al servidor */
        if (waiting(sem_res) == -1) break;                         /* 3. duermo hasta la respuesta */
        printf("[cliente]  respuesta: %s\n", p->reg);           /* 4. leo */
    }

    p-> state = 2;  /* 5. aviso al servidor que termino */

    sem_post(sem_req);

    sem_close(sem_req);
    sem_close(sem_res);

    munmap(p, sizeof(Compartido));

    return 0;
}