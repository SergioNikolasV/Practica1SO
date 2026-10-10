#include "common.h"

static void print_menu(void) {
    printf("\n=== Consulta de canciones ===\n");
    printf("1. Buscar por track_id\n");
    printf("2. Buscar por genero\n");
    printf("3. Buscar por label_tier\n");
    printf("0. Salir\n");
    printf("Opcion: ");
    fflush(stdout);
}

int main(void) {
    // Intentamos obtener la memoria compartida existente (sin IPC_CREAT)
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shmid == -1) {
        fprintf(stderr, "Error: El servidor no esta activo o no ha creado la memoria compartida.\n");
        return 1;
    }

    SharedData *sh = shmat(shmid, NULL, 0);
    if (sh == (void *)-1) {
        perror("Error en shmat");
        return 1;
    }

    sem_t *sem_req  = sem_open(SEM_REQ, 0);
    sem_t *sem_resp = sem_open(SEM_RESP, 0);
    if (sem_req == SEM_FAILED || sem_resp == SEM_FAILED) {
        perror("Error abriendo semaforos");
        shmdt(sh);
        return 1;
    }

    char buf[256];
    int opcion;

    while (1) {
        print_menu();
        if (!fgets(buf, sizeof(buf), stdin)) break; // Maneja EOF o Ctrl+D

        if (sscanf(buf, "%d", &opcion) != 1) {
            printf("Opcion invalida.\n");
            continue;
        }

        if (opcion == CRIT_EXIT) {
            printf("Saliendo del cliente...\n");
            break;
        }

        if (opcion < 1 || opcion > 3) {
            printf("Opcion invalida.\n");
            continue;
        }

        printf("Valor a buscar: ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\r\n")] = '\0';

        if (buf[0] == '\0') {
            printf("El valor no puede estar vacio.\n");
            continue;
        }

        // Escribimos en memoria compartida
        sh->req.criteria = opcion;
        snprintf(sh->req.query, MAX_CAPACITY, "%s", buf);

        // Hay petición
        sem_post(sem_req);

        sem_wait(sem_resp);

        if (sh->resp.status != 0) {
            printf("\nError del servidor: %s\n", sh->resp.result);
        } else if (sh->resp.result[0] == '\0') {
            printf("\nNo se encontraron resultados.\n");
        } else {
            printf("\n%s", sh->resp.result);
        }
    }

    // Desvinculamos recursos en el cliente (el cliente no elimina IPC_RMID ni sem_unlink)
    shmdt(sh);
    sem_close(sem_req);
    sem_close(sem_resp);
    return 0;
}