#define _POSIX_C_SOURCE 200809L 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "shm_comun.h"

void fullfitdata(Compartido *p) {
    p->tipo_solicitud = 1;
    snprintf(p->par1, sizeof(p->par1), "Parametro 1");
    snprintf(p->par2, sizeof(p->par2), "Parametro 2");
    p->state = 0;
    p->num_reg = 5;
    snprintf(p->reg, sizeof(p->reg), "Registro de prueba #1");
    p->cod_error = 0;
}

int main(void) {

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
    fullfitdata(p);

    printf(" Servidor iniciado con PID: %d, orpima cualquier tecla para concluir...\n", getpid());
    getchar();

    munmap(p, sizeof(Compartido));
    shm_unlink(NOMBRE_SHM);

    return 0;
}
