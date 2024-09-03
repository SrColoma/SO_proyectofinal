#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"

#define SHM_KEY_IMAGE 1234
#define SHM_KEY_BLUR 5678
#define SHM_KEY_EDGE 91011

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *srcFile = fopen(argv[1], "rb");
    if (srcFile == NULL) {
        perror("Error opening file");
        return 1;
    }

    BMP_Image *image = readImage(srcFile);
    if (image == NULL) {
        fclose(srcFile);
        fprintf(stderr, "Error reading image\n");
        return 1;
    }
    fclose(srcFile);

    int width = image->header.width_px;
    int height = image->norm_height;
    int halfHeight = height / 2;

    // Verificar que la imagen se haya leído correctamente
    if (image->pixels == NULL) {
        fprintf(stderr, "Error: image pixels are NULL\n");
        freeImage(image);
        return 1;
    }

    // Crear memoria compartida para la imagen
    int shm_id = shmget(SHM_KEY_IMAGE, sizeof(BMP_Image) + width * height * sizeof(Pixel), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Error creating shared memory");
        freeImage(image);
        return 1;
    }

    BMP_Image *shm_image = (BMP_Image *)shmat(shm_id, NULL, 0);
    if (shm_image == (BMP_Image *)-1) {
        perror("Error attaching shared memory");
        freeImage(image);
        return 1;
    }

    // Copiar la imagen a la memoria compartida
    memcpy(shm_image, image, sizeof(BMP_Image));
    shm_image->pixels = (Pixel **)(shm_image + 1);
    for (int i = 0; i < height; i++) {
        shm_image->pixels[i] = (Pixel *)(shm_image->pixels + height) + i * width;
        memcpy(shm_image->pixels[i], image->pixels[i], width * sizeof(Pixel));
    }

    // Liberar la imagen original
    freeImage(image);

    printf("Image loaded into shared memory successfully.\n");

    return 0;
}