#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"


void freeImageMemory(BMP_Image *image) {
    for (int i = 0; i < image->norm_height; i++) {
        free(image->pixels[i]);
    }
    free(image->pixels);
    free(image);
}

BMP_Image* readImageFromFile(const char *filename) {
    FILE *srcFile = fopen(filename, "rb");
    if (srcFile == NULL) {
        printError(FILE_ERROR);
        return NULL;
    }

    BMP_Image *image = readImage(srcFile);
    if (image == NULL) {
        fclose(srcFile);
        printError(MEMORY_ERROR);
        return NULL;
    }
    fclose(srcFile);
    return image;
}

int verifyImage(BMP_Image *image) {
    if (image->pixels == NULL) {
        fprintf(stderr, "Error: image pixels are NULL\n");
        freeImage(image);
        return 1;
    }
    return 0;
}

BMP_Image* readImageFromSharedMemory(key_t shmKey) {
    int shmIdImage = shmget(shmKey, 0, 0666);
    if (shmIdImage < 0) {
        perror("Error accessing shared memory for image");
        return NULL;
    }

    BMP_Image *image = (BMP_Image *)shmat(shmIdImage, NULL, 0);
    if (image == (BMP_Image *)-1) {
        perror("Error attaching shared memory for image");
        return NULL;
    }
    return image;
}

int writeImageToSharedMemory(BMP_Image *image, key_t shmKey) {
    int shmIdEdge = shmget(shmKey, 0, 0666);
    if (shmIdEdge < 0) {
        perror("Error accessing shared memory for edge_detector");
        return 1;
    }
    Pixel *shmEdge = (Pixel *)shmat(shmIdEdge, NULL, 0);
    if (shmEdge == (Pixel *)-1) {
        perror("Error attaching shared memory for edge_detector");
        return 1;
    }
    int width = image->header.width_px;
    int halfHeight = image->norm_height / 2;
    for (int i = halfHeight; i < image->norm_height; i++) {
        memcpy(shmEdge + (i - halfHeight) * width, image->pixels[i], width * sizeof(Pixel));
    }
    // No liberar la memoria compartida
    // shmdt(shmEdge);
    return 0;
}

int saveImageToFile(BMP_Image *image, const char *filename) {
    char *dot = strrchr(filename, '.');
    size_t baseLength = dot ? (size_t)(dot - filename) : strlen(filename);
    char *outputFile = malloc(baseLength + strlen("_filtered.bmp") + 1);
    if (outputFile == NULL) {
        printError(MEMORY_ERROR);
        return 1;
    }
    strncpy(outputFile, filename, baseLength);
    outputFile[baseLength] = '\0';
    strcat(outputFile, "_filtered.bmp");

    if (!writeImage(outputFile, image)) {
        printError(FILE_ERROR);
        free(outputFile);
        return 1;
    }

    free(outputFile);
    return 0;
}