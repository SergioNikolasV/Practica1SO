#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>

#define N_TERMINOS 100000000

int main() {
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd) == -1) {
        perror("Error al crear el pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();

    if (pid < 0) {
        perror("Error en fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { 
        close(pipefd[0]);

        double suma_hijo = 0.0;


        for (long k = 0; k < N_TERMINOS; k+=2) {
            suma_hijo += (1.0) / (2 * k + 1);
        }

        write(pipefd[1], &suma_hijo, sizeof(suma_hijo));

        close(pipefd[1]);
        exit(0);

    } else { 
        close(pipefd[1]);

        double suma_padre = 0.0;
        double suma_hijo = 0.0;

        for (long k = 1; k < N_TERMINOS; k+=2) {
            suma_padre += (-1.0) / (2 * k + 1);
        }

        read(pipefd[0], &suma_hijo, sizeof(suma_hijo));
        close(pipefd[0]);

        wait(NULL);

        double pi_aproximado = suma_padre + suma_hijo;
        printf("Aproximación de PI: %.15f\n", 4*pi_aproximado);
    }

    return 0;
}