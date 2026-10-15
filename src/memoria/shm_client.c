#define _POSIX_C_SOURCE 200809L

#include "shm_comun.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define COLOR_CIAN "\033[1;36m"
#define COLOR_VERDE "\033[1;32m"
#define COLOR_AMARILLO "\033[1;33m"
#define COLOR_ROJO "\033[1;31m"
#define COLOR_TENUE "\033[2m"
#define COLOR_RESET "\033[0m"

enum {
    ENTRADA_EOF = 0,
    ENTRADA_VALIDA = 1,
    ENTRADA_TERMINAR = 2,
    FLUJO_FIN_ENTRADA = 3
};

static const char *const nombres_columnas[] = {
    "track_id", "genre", "energy", "valence", "danceability", "acousticness",
    "instrumentalness", "tempo_bpm", "length_sec", "label", "marketing_budget",
    "playlist_adds", "editorial_playlist", "tiktok_virality", "artist_listeners",
    "featured_artist", "first_month_streams", "halflife_days", "streams_3yr",
    "slow_burner", "is_hit"
};

static int waiting(sem_t *semaforo) {
    while (sem_wait(semaforo) == -1) {
        if (errno != EINTR) {
            perror("sem_wait");
            return -1;
        }
    }
    return 0;
}

static int enviar_peticion(Compartido *compartido, sem_t *sem_req, sem_t *sem_resp,
                           TipoSolicitud tipo) {
    compartido->tipo_solicitud = tipo;
    if (sem_post(sem_req) == -1) {
        perror("sem_post sem_req");
        return -1;
    }
    return waiting(sem_resp);
}

static int es_comando_terminar(const char *texto) {
    static const char comando[] = "terminar";
    size_t i;

    if (strcmp(texto, "q") == 0) {
        return 1;
    }
    for (i = 0; comando[i] != '\0' && texto[i] != '\0'; i++) {
        if (tolower((unsigned char)texto[i]) != comando[i]) {
            return 0;
        }
    }
    return comando[i] == '\0' && texto[i] == '\0';
}

static int leer_linea(const char *mensaje, char *buffer, size_t capacidad) {
    fputs(mensaje, stdout);
    fflush(stdout);
    if (fgets(buffer, (int)capacidad, stdin) == NULL) {
        return ENTRADA_EOF;
    }

    size_t longitud = strlen(buffer);
    if (longitud > 0 && buffer[longitud - 1] != '\n' && !feof(stdin)) {
        int caracter;
        while ((caracter = getchar()) != '\n' && caracter != EOF) {
        }
        printf(COLOR_AMARILLO "Entrada demasiado larga; máximo %zu caracteres.\n" COLOR_RESET,
               capacidad - 1);
        return -1;
    }
    if (longitud > 0 && buffer[longitud - 1] == '\n') {
        buffer[--longitud] = '\0';
    }
    if (longitud > 0 && buffer[longitud - 1] == '\r') {
        buffer[--longitud] = '\0';
    }

    char *inicio = buffer;
    while (*inicio != '\0' && isspace((unsigned char)*inicio)) {
        inicio++;
    }
    char *fin = inicio + strlen(inicio);
    while (fin > inicio && isspace((unsigned char)fin[-1])) {
        fin--;
    }
    longitud = (size_t)(fin - inicio);
    memmove(buffer, inicio, longitud);
    buffer[longitud] = '\0';
    return ENTRADA_VALIDA;
}

static int pedir_valor(const char *mensaje, char *valor, size_t capacidad) {
    for (;;) {
        int estado = leer_linea(mensaje, valor, capacidad);
        if (estado == ENTRADA_EOF) {
            return ENTRADA_EOF;
        }
        if (estado < 0) {
            continue;
        }
        if (es_comando_terminar(valor)) {
            return ENTRADA_TERMINAR;
        }
        if (valor[0] == '\0') {
            puts("El valor no puede estar vacío.");
            continue;
        }
        return ENTRADA_VALIDA;
    }
}

static int separar_campos(char *fila, char **campos, size_t cantidad) {
    char *inicio = fila;
    for (size_t i = 0; i + 1 < cantidad; i++) {
        char *coma = strchr(inicio, ',');
        if (coma == NULL) {
            return -1;
        }
        *coma = '\0';
        campos[i] = inicio;
        inicio = coma + 1;
    }
    if (strchr(inicio, ',') != NULL) {
        return -1;
    }
    campos[cantidad - 1] = inicio;
    return 0;
}

static void mostrar_fila(const char *fila, int indice) {
    char copia[SHM_MAX_LONGITUD_FILA];
    char *campos[sizeof(nombres_columnas) / sizeof(nombres_columnas[0])];
    snprintf(copia, sizeof(copia), "%s", fila);
    if (separar_campos(copia, campos, sizeof(campos) / sizeof(campos[0])) == -1) {
        printf("  %2d. %s\n", indice, fila);
        return;
    }

    printf(COLOR_VERDE "  %2d. %s" COLOR_RESET "  [%s]\n", indice,
           campos[0], campos[1]);
    for (size_t i = 2; i < sizeof(campos) / sizeof(campos[0]); i += 2) {
        size_t fin = i + 2;
        if (fin > sizeof(campos) / sizeof(campos[0])) {
            fin = sizeof(campos) / sizeof(campos[0]);
        }
        printf("      ");
        for (size_t j = i; j < fin; j++) {
            printf("%-19s %-12s", nombres_columnas[j], campos[j]);
            if (j + 1 < fin) {
                printf(" | ");
            }
        }
        putchar('\n');
    }
}

static int mostrar_respuesta(const Compartido *compartido, const char *consulta) {
    switch (compartido->state) {
        case RESPUESTA_OK:
            if (compartido->num_reg < 0 || compartido->num_reg > SHM_MAX_REGISTROS) {
                fputs(COLOR_ROJO "El servidor devolvió una cantidad de filas inválida.\n" COLOR_RESET,
                      stderr);
                return 0;
            }
            printf(COLOR_CIAN "%s: %d resultado(s)" COLOR_RESET "\n", consulta,
                   compartido->num_reg);
            for (int i = 0; i < compartido->num_reg; i++) {
                mostrar_fila(compartido->reg[i], i + 1);
            }
            if (compartido->hay_mas) {
                puts(COLOR_TENUE "Hay más resultados disponibles." COLOR_RESET);
            }
            return 1;
        case RESPUESTA_SIN_RESULTADOS:
            printf(COLOR_AMARILLO "Sin resultados para %s.\n" COLOR_RESET, consulta);
            return 1;
        case RESPUESTA_ERROR:
            fprintf(stderr, COLOR_ROJO "Error del servidor: %s (código %d).\n" COLOR_RESET,
                    strerror(compartido->cod_error), compartido->cod_error);
            return 0;
        case RESPUESTA_TERMINADO:
            puts("El servidor ya terminó.");
            return 0;
        default:
            fputs(COLOR_ROJO "Respuesta desconocida del servidor.\n" COLOR_RESET, stderr);
            return 0;
    }
}

static int terminar_servidor(Compartido *compartido, sem_t *sem_req, sem_t *sem_resp) {
    if (enviar_peticion(compartido, sem_req, sem_resp, SOLICITUD_TERMINAR) == -1) {
        return -1;
    }
    if (compartido->state != RESPUESTA_TERMINADO) {
        fputs(COLOR_ROJO "El servidor no confirmó la terminación.\n" COLOR_RESET, stderr);
        return -1;
    }
    puts(COLOR_CIAN "Sesión cerrada correctamente." COLOR_RESET);
    return 0;
}

static int pedir_opcion(const char *mensaje, int minimo, int maximo, int *opcion) {
    char entrada[32];
    for (;;) {
        int estado = leer_linea(mensaje, entrada, sizeof(entrada));
        if (estado == ENTRADA_EOF) {
            return ENTRADA_EOF;
        }
        if (estado < 0) {
            continue;
        }
        char *fin;
        errno = 0;
        long valor = strtol(entrada, &fin, 10);
        if (entrada[0] != '\0' && errno == 0 && *fin == '\0' &&
            valor >= minimo && valor <= maximo) {
            *opcion = (int)valor;
            return ENTRADA_VALIDA;
        }
        printf(COLOR_AMARILLO "Elige un número entre %d y %d.\n" COLOR_RESET, minimo, maximo);
    }
}

static int continuar_pagina(void) {
    char entrada[SHM_MAX_PARAMETRO];
    for (;;) {
        int estado = leer_linea("Enter = siguiente página, m = menú: ",
                                entrada, sizeof(entrada));
        if (estado == ENTRADA_EOF) {
            return FLUJO_FIN_ENTRADA;
        }
        if (estado < 0) {
            continue;
        }
        if (entrada[0] == '\0') {
            return 1;
        }
        if (strcmp(entrada, "m") == 0 || strcmp(entrada, "M") == 0) {
            return 0;
        }
        puts("Pulsa Enter para avanzar o escribe m para volver al menú.");
    }
}

static void mostrar_catalogo_tracks(const Compartido *compartido, int desplazamiento) {
    printf(COLOR_CIAN "Canciones %d-%d" COLOR_RESET "\n", desplazamiento + 1,
           desplazamiento + compartido->num_reg);
    for (int i = 0; i < compartido->num_reg; i++) {
        char fila[SHM_MAX_LONGITUD_FILA];
        snprintf(fila, sizeof(fila), "%s", compartido->reg[i]);
        char *separador = strchr(fila, ',');
        if (separador == NULL) {
            printf("  %2d. %s\n", desplazamiento + i + 1, fila);
            continue;
        }
        *separador = '\0';
        printf("  %2d. %-16s [%s]\n", desplazamiento + i + 1, fila, separador + 1);
    }
}

static void mostrar_catalogo_genres(const Compartido *compartido, int desplazamiento) {
    puts(COLOR_CIAN "Géneros disponibles" COLOR_RESET);
    for (int i = 0; i < compartido->num_reg; i++) {
        printf("  %2d. %s\n", desplazamiento + i + 1, compartido->reg[i]);
    }
}

static int ejecutar_track_id(Compartido *compartido, sem_t *sem_req, sem_t *sem_resp,
                             const char *track_id) {
    snprintf(compartido->par1, sizeof(compartido->par1), "%s", track_id);
    if (enviar_peticion(compartido, sem_req, sem_resp, SOLICITUD_TRACK_ID) == -1) {
        return -1;
    }
    return mostrar_respuesta(compartido, "Consulta por track_id") ? 0 : -1;
}

static int ejecutar_busqueda_genre(Compartido *compartido, sem_t *sem_req, sem_t *sem_resp,
                                   const char *genre) {
    snprintf(compartido->par1, sizeof(compartido->par1), "%s", genre);
    compartido->tam_bloque = SHM_MAX_REGISTROS;
    if (enviar_peticion(compartido, sem_req, sem_resp, SOLICITUD_GENRE) == -1) {
        return -1;
    }

    int pagina = 1;
    for (;;) {
        char titulo[SHM_MAX_PARAMETRO + 32];
        snprintf(titulo, sizeof(titulo), "Bloque %d para genre=%s", pagina, genre);
        if (!mostrar_respuesta(compartido, titulo)) {
            return -1;
        }
        if (compartido->state != RESPUESTA_OK || !compartido->hay_mas) {
            return 0;
        }

        int continuar = continuar_pagina();
        if (continuar != 1) {
            return continuar == FLUJO_FIN_ENTRADA ? FLUJO_FIN_ENTRADA : 0;
        }
        if (enviar_peticion(compartido, sem_req, sem_resp, SOLICITUD_SIGUIENTE) == -1) {
            return -1;
        }
        pagina++;
    }
}

static int explorar_tracks(Compartido *compartido, sem_t *sem_req, sem_t *sem_resp) {
    if (enviar_peticion(compartido, sem_req, sem_resp, SOLICITUD_LISTAR_TRACKS) == -1) {
        return -1;
    }

    int desplazamiento = 0;
    for (;;) {
        if (compartido->state == RESPUESTA_SIN_RESULTADOS) {
            puts("El CSV no contiene canciones.");
            return 0;
        }
        if (compartido->state != RESPUESTA_OK || compartido->num_reg < 0 ||
            compartido->num_reg > SHM_MAX_REGISTROS) {
            mostrar_respuesta(compartido, "Catálogo de canciones");
            return -1;
        }
        mostrar_catalogo_tracks(compartido, desplazamiento);

        char mensaje[128];
        snprintf(mensaje, sizeof(mensaje),
                 "Número = consultar ID, Enter = siguiente, m = menú: ");
        char entrada[SHM_MAX_PARAMETRO];
        int estado = leer_linea(mensaje, entrada, sizeof(entrada));
        if (estado == ENTRADA_EOF) {
            return FLUJO_FIN_ENTRADA;
        }
        if (estado < 0) {
            continue;
        }
        if (strcmp(entrada, "m") == 0 || strcmp(entrada, "M") == 0) {
            return 0;
        }
        if (entrada[0] == '\0') {
            if (!compartido->hay_mas) {
                puts("Ya estás en la última página.");
                continue;
            }
            desplazamiento += compartido->num_reg;
            if (enviar_peticion(compartido, sem_req, sem_resp, SOLICITUD_SIGUIENTE) == -1) {
                return -1;
            }
            continue;
        }

        char *fin;
        errno = 0;
        long seleccion = strtol(entrada, &fin, 10);
        long indice = seleccion - desplazamiento - 1;
        if (errno != 0 || *fin != '\0' || indice < 0 || indice >= compartido->num_reg) {
            puts("Selecciona un número de la página actual, Enter para avanzar o m para menú.");
            continue;
        }
        char fila[SHM_MAX_LONGITUD_FILA];
        snprintf(fila, sizeof(fila), "%s", compartido->reg[indice]);
        char *separador = strchr(fila, ',');
        if (separador == NULL) {
            fputs(COLOR_ROJO "El catálogo devolvió una fila inválida.\n" COLOR_RESET, stderr);
            return -1;
        }
        *separador = '\0';
        if (ejecutar_track_id(compartido, sem_req, sem_resp, fila) == -1) {
            return -1;
        }
        return 0;
    }
}

static int explorar_genres(Compartido *compartido, sem_t *sem_req, sem_t *sem_resp) {
    if (enviar_peticion(compartido, sem_req, sem_resp, SOLICITUD_LISTAR_GENRES) == -1) {
        return -1;
    }

    int desplazamiento = 0;
    for (;;) {
        if (compartido->state == RESPUESTA_SIN_RESULTADOS) {
            puts("No se encontraron géneros en el CSV.");
            return 0;
        }
        if (compartido->state != RESPUESTA_OK || compartido->num_reg < 0 ||
            compartido->num_reg > SHM_MAX_REGISTROS) {
            mostrar_respuesta(compartido, "Géneros disponibles");
            return -1;
        }
        mostrar_catalogo_genres(compartido, desplazamiento);

        char entrada[SHM_MAX_PARAMETRO];
        int estado = leer_linea("Número = buscar género, Enter = siguiente, m = menú: ",
                                entrada, sizeof(entrada));
        if (estado == ENTRADA_EOF) {
            return FLUJO_FIN_ENTRADA;
        }
        if (estado < 0) {
            continue;
        }
        if (strcmp(entrada, "m") == 0 || strcmp(entrada, "M") == 0) {
            return 0;
        }
        if (entrada[0] == '\0') {
            if (!compartido->hay_mas) {
                puts("Ya estás en la última página.");
                continue;
            }
            desplazamiento += compartido->num_reg;
            if (enviar_peticion(compartido, sem_req, sem_resp, SOLICITUD_SIGUIENTE) == -1) {
                return -1;
            }
            continue;
        }

        char *fin;
        errno = 0;
        long seleccion = strtol(entrada, &fin, 10);
        long indice = seleccion - desplazamiento - 1;
        if (errno != 0 || *fin != '\0' || indice < 0 || indice >= compartido->num_reg) {
            puts("Selecciona un número de la página actual, Enter para avanzar o m para menú.");
            continue;
        }
        char genre[SHM_MAX_PARAMETRO];
        if (strlen(compartido->reg[indice]) >= sizeof(genre)) {
            fputs(COLOR_ROJO "El género recibido excede el tamaño admitido.\n" COLOR_RESET,
                  stderr);
            return -1;
        }
        size_t longitud_genre = strlen(compartido->reg[indice]);
        memcpy(genre, compartido->reg[indice], longitud_genre + 1);
        return ejecutar_busqueda_genre(compartido, sem_req, sem_resp, genre);
    }
}

int main(void) {
    int fd = shm_open(NOMBRE_SHM, O_RDWR, 0600);
    if (fd == -1) {
        fprintf(stderr, "No encuentro el servidor. Inícialo antes de ejecutar el cliente.\n");
        perror("shm_open");
        return EXIT_FAILURE;
    }

    Compartido *compartido = mmap(NULL, sizeof(*compartido), PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fd, 0);
    if (compartido == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return EXIT_FAILURE;
    }
    close(fd);

    sem_t *sem_req = sem_open(NOMBRE_REQ, 0);
    sem_t *sem_resp = sem_open(NOMBRE_RESP, 0);
    if (sem_req == SEM_FAILED || sem_resp == SEM_FAILED) {
        perror("sem_open");
        if (sem_req != SEM_FAILED) sem_close(sem_req);
        if (sem_resp != SEM_FAILED) sem_close(sem_resp);
        munmap(compartido, sizeof(*compartido));
        return EXIT_FAILURE;
    }

    puts(COLOR_CIAN "==============================================" COLOR_RESET);
    puts(COLOR_CIAN "       CONSULTA DE CANCIONES - HITO 3" COLOR_RESET);
    puts(COLOR_CIAN "==============================================" COLOR_RESET);
    puts("Explora el catálogo o consulta directamente por ID o género.");
    puts(COLOR_TENUE "El cursor de página cambia al iniciar otra operación.\n" COLOR_RESET);

    int menu_activo = 1;
    int solicitar_terminacion = 0;
    int codigo_salida = EXIT_SUCCESS;
    while (menu_activo) {
        puts("\n" COLOR_CIAN "Menú" COLOR_RESET);
        puts("  1. Explorar canciones (track_id + genre)");
        puts("  2. Ver géneros disponibles");
        puts("  3. Buscar por track_id");
        puts("  4. Buscar por genre");
        puts("  0. Salir y cerrar el servidor");

        int opcion;
        int estado = pedir_opcion("Opción: ", 0, 4, &opcion);
        if (estado == ENTRADA_EOF) {
            puts("\nFin de entrada; se cierra el cliente sin detener el servidor.");
            break;
        }
        if (estado != ENTRADA_VALIDA) {
            continue;
        }
        if (opcion == 0) {
            menu_activo = 0;
            solicitar_terminacion = 1;
            continue;
        }

        int resultado = 0;
        char valor[SHM_MAX_PARAMETRO];
        switch (opcion) {
            case 1:
                resultado = explorar_tracks(compartido, sem_req, sem_resp);
                break;
            case 2:
                resultado = explorar_genres(compartido, sem_req, sem_resp);
                break;
            case 3:
                estado = pedir_valor("track_id: ", valor, sizeof(valor));
                if (estado == ENTRADA_EOF) {
                    puts("\nFin de entrada; se cierra el cliente sin detener el servidor.");
                    menu_activo = 0;
                } else if (estado == ENTRADA_TERMINAR) {
                    menu_activo = 0;
                    solicitar_terminacion = 1;
                } else if (estado == ENTRADA_VALIDA) {
                    resultado = ejecutar_track_id(compartido, sem_req, sem_resp, valor);
                }
                break;
            case 4:
                estado = pedir_valor("genre: ", valor, sizeof(valor));
                if (estado == ENTRADA_EOF) {
                    puts("\nFin de entrada; se cierra el cliente sin detener el servidor.");
                    menu_activo = 0;
                } else if (estado == ENTRADA_TERMINAR) {
                    menu_activo = 0;
                    solicitar_terminacion = 1;
                } else if (estado == ENTRADA_VALIDA) {
                    resultado = ejecutar_busqueda_genre(compartido, sem_req, sem_resp, valor);
                }
                break;
        }
        if (resultado < 0) {
            codigo_salida = EXIT_FAILURE;
            menu_activo = 0;
        } else if (resultado == FLUJO_FIN_ENTRADA) {
            puts("\nFin de entrada; se cierra el cliente sin detener el servidor.");
            menu_activo = 0;
        }
    }

    if (solicitar_terminacion && codigo_salida == EXIT_SUCCESS &&
        terminar_servidor(compartido, sem_req, sem_resp) == -1) {
        codigo_salida = EXIT_FAILURE;
    }

    sem_close(sem_req);
    sem_close(sem_resp);
    munmap(compartido, sizeof(*compartido));
    return codigo_salida;
}