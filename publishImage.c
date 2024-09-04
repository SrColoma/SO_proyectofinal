#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include "bmp.h"

int publishImage(BMP_Image *image, key_t shmKey) {
    // Obtener el ID de la memoria compartida
    int shmId = shmget(shmKey, sizeof(BMP_Image), IPC_CREAT | 0666);
    if (shmId < 0) {
        perror("shmget");
        return -1;
    }

    // Adjuntar la memoria compartida al espacio de direcciones del proceso
    BMP_Image *sharedImage = (BMP_Image *)shmat(shmId, NULL, 0);
    if (sharedImage == (BMP_Image *)-1) {
        perror("shmat");
        return -1;
    }

    // Copiar la imagen a la memoria compartida
    *sharedImage = *image;

    // Desadjuntar la memoria compartida
    if (shmdt(sharedImage) == -1) {
        perror("shmdt");
        return -1;
    }

    return 0;
}