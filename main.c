#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "bmp.h"

int executeCommand(const char *command, char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        // Error en fork
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Proceso hijo
        printf("Executing command: %s\n", command); // Mensaje de depuración
        execvp(command, argv);
        // Si execvp falla
        perror("execvp");
        exit(1);
    } else {
        // Proceso padre
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return 1;
        }
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                fprintf(stderr, "Error executing %s\n", command);
                return 1;
            }
        } else {
            fprintf(stderr, "%s did not terminate normally\n", command);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // Verificar que el archivo de entrada existe
    if (access(argv[1], F_OK) == -1) {
        fprintf(stderr, "Input file %s does not exist\n", argv[1]);
        return 1;
    }

    // Ejecutar el publisher
    char *publisherArgs[] = {"./publisher", argv[1], NULL};
    // printf("Executing command: ./publisher %s\n", argv[1]); // Mensaje de depuración
    if (executeCommand("./publisher", publisherArgs) != 0) {
        return 1;
    }
    printf("Publisher finished successfully.\n"); // Mensaje de depuración

    // Ejecutar blurrer y edge_detector simultáneamente
    pid_t pid_blurrer = fork();
    if (pid_blurrer == 0) {
        // Proceso hijo para blurrer
        printf("Executing command: ./blurrer shm 4 shm\n"); // Mensaje de depuración
        execl("./blurrer", "blurrer", "shm", "4", "shm", (char *)NULL);
        perror("Error executing blurrer");
        exit(1);
    } else if (pid_blurrer < 0) {
        perror("fork for blurrer");
        return 1;
    }

    pid_t pid_edge_detector = fork();
    if (pid_edge_detector == 0) {
        // Proceso hijo para edge_detector
        printf("Executing command: ./edge_detector shm 4 shm\n"); // Mensaje de depuración
        execl("./edge_detector", "edge_detector", "shm", "4", "shm", (char *)NULL);
        perror("Error executing edge_detector");
        exit(1);
    } else if (pid_edge_detector < 0) {
        perror("fork for edge_detector");
        return 1;
    }

    // Esperar a que blurrer y edge_detector terminen
    int status;
    printf("Waiting for blurrer to finish...\n"); // Mensaje de depuración
    waitpid(pid_blurrer, &status, 0);
    if (WIFEXITED(status)) {
        printf("Blurrer finished with status %d.\n", WEXITSTATUS(status)); // Mensaje de depuración
    } else {
        fprintf(stderr, "Blurrer did not terminate normally.\n");
    }

    printf("Waiting for edge_detector to finish...\n"); // Mensaje de depuración
    waitpid(pid_edge_detector, &status, 0);
    if (WIFEXITED(status)) {
        printf("Edge_detector finished with status %d.\n", WEXITSTATUS(status)); // Mensaje de depuración
    } else {
        fprintf(stderr, "Edge_detector did not terminate normally.\n");
    }

    // Ejecutar el combiner
    char *combinerArgs[] = {"./combiner", argv[1], NULL};
    printf("Executing command: ./combiner %s\n", argv[1]); // Mensaje de depuración
    if (executeCommand("./combiner", combinerArgs) != 0) {
        return 1;
    }
    printf("Combiner finished successfully.\n"); // Mensaje de depuración

    printf("All processes completed successfully.\n");
    return 0;
}