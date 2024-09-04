#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_file>\n", argv[0]);
        return 1;
    }
    char *outputFile = argv[1];

    // Acceder a la memoria compartida
    int shmId = shmget(SHM_KEY_IMAGE, 0, 0666);
    if (shmId < 0) {
        perror("Error accessing shared memory");
        return 1;
    }
    printf("Shared memory accessed successfully.\n");

    // Adjuntar la memoria compartida
    Pixel *shm = (Pixel *)shmat(shmId, NULL, 0);
    if (shm == (Pixel *)-1) {
        perror("Error attaching shared memory");
        return 1;
    }
    printf("Shared memory attached successfully.\n");

    // Leer la imagen desde la memoria compartida
    BMP_Image *image = readImageFromSharedMemory(SHM_KEY_IMAGE);
    if (image == NULL) {
        fprintf(stderr, "Error reading image from shared memory\n");
        return 1;
    }
    printf("Image read from shared memory successfully.\n");

    // Guardar la imagen en un archivo
    printf("Attempting to write image to file: %s\n", outputFile);
    int writeResult = writeImage(outputFile, image);
    if (writeResult != 0) {
        fprintf(stderr, "Error writing image to file, writeImage returned %d\n", writeResult);
        freeImageMemory(image);
        return 1;
    }
    printf("Image written to file successfully.\n");

    // Liberar la memoria de la imagen
    printf("Attempting to free image memory.\n");
    freeImageMemory(image);
    printf("Image memory freed successfully.\n");

    // Desadjuntar la memoria compartida
    printf("Attempting to detach shared memory.\n");
    if (shmdt(shm) == -1) {
        perror("Error detaching shared memory");
        return 1;
    }
    printf("Shared memory detached successfully.\n");

    printf("Image saved to %s successfully.\n", outputFile);
    return 0;
}