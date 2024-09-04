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

    // Adjuntar la memoria compartida
    Pixel *shm = (Pixel *)shmat(shmId, NULL, 0);
    if (shm == (Pixel *)-1) {
        perror("Error attaching shared memory");
        return 1;
    }

    // Leer la imagen desde la memoria compartida
    BMP_Image *image = readImageFromSharedMemory(SHM_KEY_IMAGE);
    if (image == NULL) {
        return 1;
    }

    // Guardar la imagen en un archivo
    if (writeImage(outputFile, image) != 0) {
        fprintf(stderr, "Error writing image to file\n");
        freeImageMemory(image);
        return 1;
    }

    // Liberar la memoria de la imagen
    freeImageMemory(image);

    // Desadjuntar la memoria compartida
    shmdt(shm);

    printf("Image saved to %s successfully.\n", outputFile);
    return 0;
}