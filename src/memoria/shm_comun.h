#ifndef SHM_COMUN_H
#define SHM_COMUN_H

#define NOMBRE_SHM "/server_compmem_shm"
#define NOMBRE_REQ  "/demo_sem_req"
#define NOMBRE_RESP "/demo_sem_resp"

#define SHM_MAX_PARAMETRO 128
#define SHM_MAX_REGISTROS 10
#define SHM_MAX_LONGITUD_FILA 1024

typedef enum {
    SOLICITUD_TRACK_ID = 1,
    SOLICITUD_GENRE,
    SOLICITUD_LISTAR_TRACKS,
    SOLICITUD_LISTAR_GENRES,
    SOLICITUD_SIGUIENTE,
    SOLICITUD_TERMINAR
} TipoSolicitud;

typedef enum {
    RESPUESTA_OK = 0,
    RESPUESTA_SIN_RESULTADOS,
    RESPUESTA_ERROR,
    RESPUESTA_TERMINADO
} EstadoRespuesta;

typedef struct {
    TipoSolicitud tipo_solicitud;
    char par1[SHM_MAX_PARAMETRO];
    char par2[SHM_MAX_PARAMETRO];
    int tam_bloque;
    EstadoRespuesta state;
    int num_reg;
    int hay_mas;
    char reg[SHM_MAX_REGISTROS][SHM_MAX_LONGITUD_FILA];
    int cod_error;
} Compartido;

#endif