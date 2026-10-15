#ifndef SHM_COMUN_H
#define SHM_COMUN_H

#define NOMBRE_SHM "/server_compmem_shm"

typedef struct {
    int  tipo_solicitud;
    char par1[64];
    char par2[64];
    int  state;
    int  num_reg;
    char reg[100];
    int  cod_error;
} Compartido;

#endif