#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "shm_comun.h"

void printdata(Compartido *p) {
    printf("Tipo de solicitud: %d\n", p->tipo_solicitud);
    printf("Parametro 1: %s\n", p->par1);
    printf("Parametro 2: %s\n", p->par2);
    printf("Estado: %d\n", p->state);
    printf("Numero de registros: %d\n", p->num_reg);
    printf("Registro: %s\n", p->reg);   
    printf("Codigo de error: %d\n", p->cod_error);
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
    printdata(p);
    munmap(p, sizeof(Compartido));


    return 0;
}