#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include "bmp.h"
#include "utils.h"

#define SHM_KEY_BLUR 1234
#define SHM_KEY_EDGE 5678
#define SHM_KEY_METADATA 1213

Pixel* readPixelsFromSharedMemory(key_t shmKey, int *shmId) {
    *shmId = shmget(shmKey, 0, 0666);
    if (*shmId < 0) {
        perror("Error accessing shared memory");
        return NULL;
    }
    Pixel *shmPixels = (Pixel *)shmat(*shmId, NULL, 0);
    if (shmPixels == (Pixel *)-1) {
        perror("Error attaching shared memory");
        return NULL;
    }
    return shmPixels;
}

BMP_Image* createCombinedImage(Pixel *shmBlur, Pixel *shmEdge, int width, int halfHeight) {
    int height = halfHeight * 2;
    BMP_Image *combinedImage = malloc(sizeof(BMP_Image));
    if (combinedImage == NULL) {
        perror("Error allocating memory for combined image");
        return NULL;
    }

    combinedImage->header.width_px = width;
    combinedImage->norm_height = height;
    combinedImage->bytes_per_pixel = 3; // Asumimos 3 bytes por píxel (RGB)
    combinedImage->pixels = malloc(height * sizeof(Pixel *));
    if (combinedImage->pixels == NULL) {
        perror("Error allocating memory for combined image pixels");
        free(combinedImage);
        return NULL;
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
            return NULL;
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

    return combinedImage;
}

int saveCombinedImage(const char *outputFileName, BMP_Image *combinedImage) {
    char *outputFile = malloc(strlen(outputFileName) + strlen("_combinado.bmp") + 1);
    if (outputFile == NULL) {
        perror("Error allocating memory for output file name");
        return 1;
    }
    strcpy(outputFile, outputFileName);
    strcat(outputFile, "_combinado.bmp");

    if (!writeImage(outputFile, combinedImage)) {
        perror("Error writing combined image to file");
        free(outputFile);
        return 1;
    }

    free(outputFile);
    return 0;
}

ImageMetadata* attachMetadataSharedMemory(key_t shmKey) {
    int shm_id = shmget(shmKey, sizeof(ImageMetadata), 0666);
    if (shm_id < 0) {
        perror("Error accessing shared memory for metadata");
        return NULL;
    }

    ImageMetadata *shm_metadata = (ImageMetadata *)shmat(shm_id, NULL, 0);
    if (shm_metadata == (ImageMetadata *)-1) {
        perror("Error attaching shared memory for metadata");
        return NULL;
    }

    return shm_metadata;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_file>\n", argv[0]);
        return 1;
    }

    // Obtener los metadatos de la imagen desde la memoria compartida
    ImageMetadata *metadata = attachMetadataSharedMemory(SHM_KEY_METADATA);
    if (metadata == NULL) {
        return 1;
    }

    int width = metadata->width;
    int halfHeight = metadata->norm_height / 2;

    int shmIdBlur, shmIdEdge;
    Pixel *shmBlur = readPixelsFromSharedMemory(SHM_KEY_BLUR, &shmIdBlur);
    if (shmBlur == NULL) {
        shmdt(metadata);
        return 1;
    }

    Pixel *shmEdge = readPixelsFromSharedMemory(SHM_KEY_EDGE, &shmIdEdge);
    if (shmEdge == NULL) {
        shmdt(shmBlur);
        shmdt(metadata);
        return 1;
    }

    BMP_Image *combinedImage = createCombinedImage(shmBlur, shmEdge, width, halfHeight);
    if (combinedImage == NULL) {
        shmdt(shmBlur);
        shmdt(shmEdge);
        shmdt(metadata);
        return 1;
    }

    if (saveCombinedImage(argv[1], combinedImage) != 0) {
        freeImageMemory(combinedImage);
        shmdt(shmBlur);
        shmdt(shmEdge);
        shmdt(metadata);
        return 1;
    }

    freeImageMemory(combinedImage);
    shmdt(shmBlur);
    shmdt(shmEdge);
    shmdt(metadata);

    printf("Image combined and saved successfully.\n");
    return 0;
}