#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "bmp.h"
#include "utils.h"


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <num_threads>\n", argv[0]);
        return 1;
    }

    int numThreads = atoi(argv[2]);
    if (numThreads <= 0) {
        fprintf(stderr, "Number of threads must be a positive integer.\n");
        return 1;
    }

    // Verificar que el archivo de entrada existe
    if (access(argv[1], F_OK) == -1) {
        fprintf(stderr, "Input file %s does not exist\n", argv[1]);
        return 1;
    }

    // Ejecutar el publisher
    char numThreadsStr[10];
    snprintf(numThreadsStr, sizeof(numThreadsStr), "%d", numThreads);
    char *publisherArgs[] = {"./publisher", argv[1], numThreadsStr, NULL};
    // printf("Executing command: ./publisher %s %s\n", argv[1], numThreadsStr); // Mensaje de depuración
    if (executeCommand("./publisher", publisherArgs) != 0) {
        return 1;
    }
    printf("Publisher finished successfully.\n"); // Mensaje de depuración

    // Ejecutar blurrer y edge_detector simultáneamente
    pid_t pid_blurrer = fork();
    if (pid_blurrer == 0) {
        // Proceso hijo para blurrer
        printf("Executing command: ./blurrer shm %s shm\n", numThreadsStr); // Mensaje de depuración
        execl("./blurrer", "blurrer", "shm", numThreadsStr, "shm", (char *)NULL);
        perror("Error executing blurrer");
        exit(1);
    } else if (pid_blurrer < 0) {
        perror("fork for blurrer");
        return 1;
    }

    pid_t pid_edge_detector = fork();
    if (pid_edge_detector == 0) {
        // Proceso hijo para edge_detector
        printf("Executing command: ./edge_detector shm %s shm\n", numThreadsStr); // Mensaje de depuración
        execl("./edge_detector", "edge_detector", "shm", numThreadsStr, "shm", (char *)NULL);
        perror("Error executing edge_detector");
        exit(1);
    } else if (pid_edge_detector < 0) {
        perror("fork for edge_detector");
        return 1;
    }

    // Esperar a que blurrer y edge_detector terminen
    int status;
    waitpid(pid_blurrer, &status, 0);
    if (WIFEXITED(status)) {
        printf("Blurrer finished with status %d.\n", WEXITSTATUS(status));
    } else {
        fprintf(stderr, "Blurrer did not terminate normally.\n");
    }

    waitpid(pid_edge_detector, &status, 0);
    if (WIFEXITED(status)) {
        printf("Edge_detector finished with status %d.\n", WEXITSTATUS(status));
    } else {
        fprintf(stderr, "Edge_detector did not terminate normally.\n");
    }

    // Ejecutar el combiner
    char *combinerArgs[] = {"./combiner", "temp.bmp",NULL};
    // printf("Executing command: ./combiner %s\n", argv[1]); // Mensaje de depuración
    if (executeCommand("./combiner", combinerArgs) != 0) {
        return 1;
    }
    printf("Combiner finished successfully.\n"); // Mensaje de depuración

    printf("All processes completed successfully.\n");
    return 0;
}