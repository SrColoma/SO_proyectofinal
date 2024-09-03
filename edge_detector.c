#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"

#define SHM_KEY_IMAGE 1234
#define SHM_KEY_EDGE 91011

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
    int width = image->header.width_px;
    int height = image->norm_height;

    // Kernel de detección de bordes (Sobel)
    int kernelX[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    int kernelY[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };

    for (int y = task->startRow; y < task->endRow; y++) {
        for (int x = 0; x < width; x++) {
            int gxRed = 0, gyRed = 0;
            int gxGreen = 0, gyGreen = 0;
            int gxBlue = 0, gyBlue = 0;

            for (int ky = 0; ky < 3; ky++) {
                for (int kx = 0; kx < 3; kx++) {
                    int ny = y + ky - 1;
                    int nx = x + kx - 1;

                    if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                        Pixel *pixel = &image->pixels[ny][nx];
                        gxRed += pixel->red * kernelX[ky][kx];
                        gyRed += pixel->red * kernelY[ky][kx];
                        gxGreen += pixel->green * kernelX[ky][kx];
                        gyGreen += pixel->green * kernelY[ky][kx];
                        gxBlue += pixel->blue * kernelX[ky][kx];
                        gyBlue += pixel->blue * kernelY[ky][kx];
                    }
                }
            }

            Pixel *edgePixel = &edgeImage->pixels[y][x];
            edgePixel->red = (unsigned char) sqrt(gxRed * gxRed + gyRed * gyRed);
            edgePixel->green = (unsigned char) sqrt(gxGreen * gxGreen + gyGreen * gyGreen);
            edgePixel->blue = (unsigned char) sqrt(gxBlue * gxBlue + gyBlue * gyBlue);
        }
    }

    return NULL;
}

void applyEdgeDetection(BMP_Image *image, int numThreads) {
    int width = image->header.width_px;
    int height = image->norm_height;

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
    int rowsPerThread = height / numThreads;

    for (int i = 0; i < numThreads; i++) {
        tasks[i].image = image;
        tasks[i].edgeImage = edgeImage;
        tasks[i].startRow = i * rowsPerThread;
        tasks[i].endRow = (i == numThreads - 1) ? height : (i + 1) * rowsPerThread;
        pthread_create(&threads[i], NULL, edgeSection, &tasks[i]);
    }

    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < height; i++) {
        memcpy(image->pixels[i], edgeImage->pixels[i], width * sizeof(Pixel));
    }

    for (int i = 0; i < height; i++) {
        free(edgeImage->pixels[i]);
    }
    free(edgeImage->pixels);
    free(edgeImage);
}

void freeImageMemory(BMP_Image *image) {
    for (int i = 0; i < image->norm_height; i++) {
        free(image->pixels[i]);
    }
    free(image->pixels);
    free(image);
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
        // Leer imagen desde memoria compartida
        int shmIdImage = shmget(SHM_KEY_IMAGE, 0, 0666);
        if (shmIdImage < 0) {
            perror("Error accessing shared memory for image");
            return 1;
        }
        image = (BMP_Image *)shmat(shmIdImage, NULL, 0);
        if (image == (BMP_Image *)-1) {
            perror("Error attaching shared memory for image");
            return 1;
        }
    } else {
        // Leer imagen desde archivo
        FILE *srcFile = fopen(argv[1], "rb");
        if (srcFile == NULL) {
            printError(FILE_ERROR);
            return 1;
        }

        image = readImage(srcFile);
        if (image == NULL) {
            fclose(srcFile);
            printError(MEMORY_ERROR);
            return 1;
        }
        fclose(srcFile);
    }

    // Aplicar detección de bordes
    applyEdgeDetection(image, numThreads);

    if (useShm) {
        // Escribir imagen procesada en memoria compartida
        int shmIdEdge = shmget(SHM_KEY_EDGE, 0, 0666);
        if (shmIdEdge < 0) {
            perror("Error accessing shared memory for edge_detector");
            if (!useShm) freeImageMemory(image);
            return 1;
        }
        Pixel *shmEdge = (Pixel *)shmat(shmIdEdge, NULL, 0);
        if (shmEdge == (Pixel *)-1) {
            perror("Error attaching shared memory for edge_detector");
            if (!useShm) freeImageMemory(image);
            return 1;
        }
        int width = image->header.width_px;
        int halfHeight = image->norm_height / 2;
        for (int i = halfHeight; i < image->norm_height; i++) {
            memcpy(shmEdge + (i - halfHeight) * width, image->pixels[i], width * sizeof(Pixel));
        }
        // No liberar la memoria compartida
        // shmdt(shmEdge);
    } else {
        // Construye la ruta de salida
        char *dot = strrchr(argv[1], '.');
        size_t baseLength = dot ? (size_t)(dot - argv[1]) : strlen(argv[1]);
        char *outputFile = malloc(baseLength + strlen("_edges.bmp") + 1);
        if (outputFile == NULL) {
            printError(MEMORY_ERROR);
            freeImage(image);
            return 1;
        }
        strncpy(outputFile, argv[1], baseLength);
        outputFile[baseLength] = '\0';
        strcat(outputFile, "_edges.bmp");

        // Guarda la imagen procesada
        if (!writeImage(outputFile, image)) {
            printError(FILE_ERROR);
            free(outputFile);
            freeImage(image);
            return 1;
        }

        free(outputFile);
    }

    if (!useShm) freeImageMemory(image);

    printf("Edge detection completed successfully.\n");
    return 0;
}