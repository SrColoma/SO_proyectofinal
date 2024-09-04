#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"
#include "utils.h"


#define KERNEL_SIZE 3

typedef struct {
    BMP_Image *image;
    BMP_Image *edgeImage;
    int startRow;
    int endRow;
} EdgeTask;

void *edgeSection(void *arg) {
    EdgeTask *task = (EdgeTask *)arg;
    BMP_Image *image = task->image;
    BMP_Image *edgeImage = task->edgeImage;
    int startRow = task->startRow;
    int endRow = task->endRow;

    int width = image->header.width_px;
    int height = image->norm_height;

    // Kernel de detección de bordes (Sobel)
    int kernelX[KERNEL_SIZE][KERNEL_SIZE] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };

    int kernelY[KERNEL_SIZE][KERNEL_SIZE] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1}
    };

    for (int y = startRow; y < endRow; y++) {
        for (int x = 0; x < width; x++) {
            float gradientXRed = 0.0, gradientXGreen = 0.0, gradientXBlue = 0.0;
            float gradientYRed = 0.0, gradientYGreen = 0.0, gradientYBlue = 0.0;

            // Aplicar el kernel de detección de bordes
            for (int ky = 0; ky < KERNEL_SIZE; ky++) {
                for (int kx = 0; kx < KERNEL_SIZE; kx++) {
                    int pixelY = y + ky - 1;
                    int pixelX = x + kx - 1;

                    // Verificar límites de la imagen
                    if (pixelY >= 0 && pixelY < height && pixelX >= 0 && pixelX < width) {
                        Pixel *pixel = &image->pixels[pixelY][pixelX];
                        gradientXRed += pixel->red * kernelX[ky][kx];
                        gradientXGreen += pixel->green * kernelX[ky][kx];
                        gradientXBlue += pixel->blue * kernelX[ky][kx];
                        gradientYRed += pixel->red * kernelY[ky][kx];
                        gradientYGreen += pixel->green * kernelY[ky][kx];
                        gradientYBlue += pixel->blue * kernelY[ky][kx];
                    }
                }
            }

            // Calcular la magnitud del gradiente
            float magnitudeRed = sqrt(gradientXRed * gradientXRed + gradientYRed * gradientYRed);
            float magnitudeGreen = sqrt(gradientXGreen * gradientXGreen + gradientYGreen * gradientYGreen);
            float magnitudeBlue = sqrt(gradientXBlue * gradientXBlue + gradientYBlue * gradientYBlue);

            // Asignar el valor de detección de bordes al píxel correspondiente en la imagen de bordes
            Pixel *edgePixel = &edgeImage->pixels[y][x];
            edgePixel->red = (uint8_t)fmin(magnitudeRed, 255);
            edgePixel->green = (uint8_t)fmin(magnitudeGreen, 255);
            edgePixel->blue = (uint8_t)fmin(magnitudeBlue, 255);
        }
    }

    return NULL;
}

void applyEdgeDetection(BMP_Image *image, int numThreads) {
    int width = image->header.width_px;
    int height = image->norm_height;
    int halfHeight = height / 2;

    BMP_Image *edgeImage = malloc(sizeof(BMP_Image));
    if (edgeImage == NULL) {
        perror("Error allocating memory for edge image");
        return;
    }

    *edgeImage = *image;
    edgeImage->pixels = malloc(height * sizeof(Pixel *));
    if (edgeImage->pixels == NULL) {
        perror("Error allocating memory for edge image pixels");
        free(edgeImage);
        return;
    }

    for (int i = 0; i < height; i++) {
        edgeImage->pixels[i] = malloc(width * sizeof(Pixel));
        if (edgeImage->pixels[i] == NULL) {
            perror("Error allocating memory for edge image row");
            for (int j = 0; j < i; j++) {
                free(edgeImage->pixels[j]);
            }
            free(edgeImage->pixels);
            free(edgeImage);
            return;
        }
    }

    pthread_t threads[numThreads];
    EdgeTask tasks[numThreads];
    int rowsPerThread = halfHeight / numThreads;

    for (int i = 0; i < numThreads; i++) {
        tasks[i].image = image;
        tasks[i].edgeImage = edgeImage;
        tasks[i].startRow = halfHeight + i * rowsPerThread;
        tasks[i].endRow = (i == numThreads - 1) ? height : halfHeight + (i + 1) * rowsPerThread;
        pthread_create(&threads[i], NULL, edgeSection, &tasks[i]);
    }

    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = halfHeight; i < height; i++) {
        memcpy(image->pixels[i], edgeImage->pixels[i], width * sizeof(Pixel));
    }

    for (int i = 0; i < height; i++) {
        free(edgeImage->pixels[i]);
    }
    free(edgeImage->pixels);
    free(edgeImage);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s <input_file|shm> [num_threads] [use_shm]\n", argv[0]);
        return 1;
    }

    int numThreads = 1; // Valor predeterminado
    if (argc >= 3) {
        numThreads = atoi(argv[2]);
        if (numThreads <= 0) {
            fprintf(stderr, "Number of threads must be a positive integer.\n");
            return 1;
        }
    }

    int useShm = 0; // No usar memoria compartida por defecto
    if (argc == 4 && strcmp(argv[3], "shm") == 0) {
        useShm = 1;
    }

    BMP_Image *image = NULL;
    if (useShm) {
        image = readImageFromSharedMemory(SHM_KEY_IMAGE);
    } else {
        image = readImageFromFile(argv[1]);
    }
    if (image == NULL) {
        fprintf(stderr, "Error reading image.\n");
        return 1;
    }

    // Aplicar detección de bordes
    applyEdgeDetection(image, numThreads);

    int result = 0;
    if (useShm) {
        result = writeImageToSharedMemory(image, SHM_KEY_IMAGE, LOWER_HALF);
    } else {
        result = saveImageToFile(image, argv[1]);
    }
    if (result != 0) {
        fprintf(stderr, "Error writing image.\n");
        return 1;
    }

    if (!useShm) freeImageMemory(image);

    printf("Edge detection completed successfully.\n");
    return result;
}