#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include "bmp.h"
# include "utils.h"
 
#define SHM_KEY_IMAGE 1234
#define SHM_KEY_BLUR 5678
#define SHM_KEY_EDGE 91011


BMP_Image* createAndAttachSharedMemory(key_t shmKey, int width, int height) {
    int shm_id = shmget(shmKey, sizeof(BMP_Image) + width * height * sizeof(Pixel), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Error creating shared memory");
        return NULL;
    }

    BMP_Image *shm_image = (BMP_Image *)shmat(shm_id, NULL, 0);
    if (shm_image == (BMP_Image *)-1) {
        perror("Error attaching shared memory");
        return NULL;
    }

    return shm_image;
}

void copyImageToSharedMemory(BMP_Image *shm_image, BMP_Image *image) {
    int width = image->header.width_px;
    int height = image->norm_height;

    memcpy(shm_image, image, sizeof(BMP_Image));
    shm_image->pixels = (Pixel **)(shm_image + 1);
    for (int i = 0; i < height; i++) {
        shm_image->pixels[i] = (Pixel *)(shm_image->pixels + height) + i * width;
        memcpy(shm_image->pixels[i], image->pixels[i], width * sizeof(Pixel));
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    BMP_Image *image = readImageFromFile(argv[1]);
    if (image == NULL) {
        return 1;
    }

    if (verifyImage(image)) {
        return 1;
    }

    int width = image->header.width_px;
    int height = image->norm_height;

    BMP_Image *shm_image = createAndAttachSharedMemory(SHM_KEY_IMAGE, width, height);
    if (shm_image == NULL) {
        freeImage(image);
        return 1;
    }
    copyImageToSharedMemory(shm_image, image);

    BMP_Image *shm_blur = createAndAttachSharedMemory(SHM_KEY_BLUR, width, height);
    if (shm_blur == NULL) {
        freeImage(image);
        return 1;
    }
    copyImageToSharedMemory(shm_blur, image);

    BMP_Image *shm_edge = createAndAttachSharedMemory(SHM_KEY_EDGE, width, height);
    if (shm_edge == NULL) {
        freeImage(image);
        return 1;
    }
    copyImageToSharedMemory(shm_edge, image);

    freeImage(image);

    printf("Image loaded into shared memory successfully.\n");

    return 0;
}