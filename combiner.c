#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"

#define SHM_KEY_BLUR 5678
#define SHM_KEY_EDGE 91011

void freeImageMemory(BMP_Image *image) {
    for (int i = 0; i < image->norm_height; i++) {
        free(image->pixels[i]);
    }
    free(image->pixels);
    free(image);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_file>\n", argv[0]);
        return 1;
    }

    // Leer la mitad de la imagen procesada por blurrer desde la memoria compartida
    int shmIdBlur = shmget(SHM_KEY_BLUR, 0, 0666);
    if (shmIdBlur < 0) {
        perror("Error accessing shared memory for blurrer");
        return 1;
    }
    Pixel *shmBlur = (Pixel *)shmat(shmIdBlur, NULL, 0);
    if (shmBlur == (Pixel *)-1) {
        perror("Error attaching shared memory for blurrer");
        return 1;
    }

    // Leer la mitad de la imagen procesada por edge_detector desde la memoria compartida
    int shmIdEdge = shmget(SHM_KEY_EDGE, 0, 0666);
    if (shmIdEdge < 0) {
        perror("Error accessing shared memory for edge_detector");
        shmdt(shmBlur);
        return 1;
    }
    Pixel *shmEdge = (Pixel *)shmat(shmIdEdge, NULL, 0);
    if (shmEdge == (Pixel *)-1) {
        perror("Error attaching shared memory for edge_detector");
        shmdt(shmBlur);
        return 1;
    }

    // Crear una nueva imagen combinada
    int width = 0; // Asumimos que el ancho es conocido o se puede obtener de alguna manera
    int halfHeight = 0; // Asumimos que la mitad de la altura es conocida o se puede obtener de alguna manera
    int height = halfHeight * 2;

    BMP_Image *combinedImage = malloc(sizeof(BMP_Image));
    if (combinedImage == NULL) {
        perror("Error allocating memory for combined image");
        shmdt(shmBlur);
        shmdt(shmEdge);
        return 1;
    }

    combinedImage->header.width_px = width;
    combinedImage->norm_height = height;
    combinedImage->bytes_per_pixel = 3; // Asumimos 3 bytes por píxel (RGB)
    combinedImage->pixels = malloc(height * sizeof(Pixel *));
    if (combinedImage->pixels == NULL) {
        perror("Error allocating memory for combined image pixels");
        free(combinedImage);
        shmdt(shmBlur);
        shmdt(shmEdge);
        return 1;
    }

    for (int i = 0; i < height; i++) {
        combinedImage->pixels[i] = malloc(width * sizeof(Pixel));
        if (combinedImage->pixels[i] == NULL) {
            perror("Error allocating memory for combined image row");
            for (int j = 0; j < i; j++) {
                free(combinedImage->pixels[j]);
            }
            free(combinedImage->pixels);
            free(combinedImage);
            shmdt(shmBlur);
            shmdt(shmEdge);
            return 1;
        }
    }

    // Copiar la mitad superior de la imagen desde shmBlur
    for (int i = 0; i < halfHeight; i++) {
        memcpy(combinedImage->pixels[i], shmBlur + i * width, width * sizeof(Pixel));
    }

    // Copiar la mitad inferior de la imagen desde shmEdge
    for (int i = halfHeight; i < height; i++) {
        memcpy(combinedImage->pixels[i], shmEdge + (i - halfHeight) * width, width * sizeof(Pixel));
    }

    // Guardar la imagen combinada en un archivo
    char *outputFile = malloc(strlen(argv[1]) + strlen("_combinado.bmp") + 1);
    if (outputFile == NULL) {
        perror("Error allocating memory for output file name");
        freeImageMemory(combinedImage);
        shmdt(shmBlur);
        shmdt(shmEdge);
        return 1;
    }
    strcpy(outputFile, argv[1]);
    strcat(outputFile, "_combinado.bmp");

    if (!writeImage(outputFile, combinedImage)) {
        perror("Error writing combined image to file");
        free(outputFile);
        freeImageMemory(combinedImage);
        shmdt(shmBlur);
        shmdt(shmEdge);
        return 1;
    }

    free(outputFile);
    freeImageMemory(combinedImage);
    shmdt(shmBlur);
    shmdt(shmEdge);

    printf("Image combined and saved successfully.\n");
    return 0;
}