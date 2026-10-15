#define _POSIX_C_SOURCE 200809L 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "shm_comun.h"
#include <semaphore.h>
#include <errno.h>
#include <ctype.h>


void fullfit_data(Compartido *p) {
    p->tipo_solicitud = 1;
    snprintf(p->par1, sizeof(p->par1), "Parametro 1");
    snprintf(p->par2, sizeof(p->par2), "Parametro 2");
    p->state = 0;
    p->num_reg = 5;
    snprintf(p->reg, sizeof(p->reg), "Registro de prueba #1");
    p->cod_error = 0;
}

void delete_names(void) {
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

int main(void) {
    //delete_names(); Por si hago algo imbecil 

    int fd = shm_open(NOMBRE_SHM, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, sizeof(Compartido)) == -1) {
        perror("ftruncate");
        close(fd);
        shm_unlink(NOMBRE_SHM);
        return EXIT_FAILURE;
    }

    Compartido *p = mmap(NULL, sizeof(Compartido), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_unlink(NOMBRE_SHM);
        return EXIT_FAILURE;
    }

    close(fd);
    fullfit_data(p);

    printf(" Servidor iniciado con PID: %d, esperando solicitudes...\n", getpid());
    // getchar(); Esto se usaba antes del semaforo

    sem_t *sem_req = sem_open(NOMBRE_REQ, O_CREAT, 0600, 0);
    sem_t *sem_res = sem_open(NOMBRE_RESP, O_CREAT, 0600, 0);
    if (sem_req == SEM_FAILED || sem_res == SEM_FAILED) {
        perror("sem_open");
        munmap(p, sizeof(Compartido));
        shm_unlink(NOMBRE_SHM);
        return EXIT_FAILURE;
    }

    for (;;) {
        if (waiting(sem_req) == -1) break;
        if (p->state == 2) break;

        printf("[servidor] recibí:    %s\n", p->par1);
        snprintf(p->reg, sizeof(p->reg), "%s%s", "Este mensaje es una mierda: ", p->par1);
        sem_post(sem_res);
    }

    munmap(p, sizeof(Compartido));
    delete_names();

    return 0;
}
