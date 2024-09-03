#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include "bmp.h"

#define SHM_KEY_IMAGE 1234
#define SHM_KEY_BLUR 5678

#define KERNEL_SIZE 3

typedef struct {
    BMP_Image *image;
    BMP_Image *blurredImage;
    int startRow;
    int endRow;
} BlurTask;

void *blurSection(void *arg) {
    BlurTask *task = (BlurTask *)arg;
    BMP_Image *image = task->image;
    BMP_Image *blurredImage = task->blurredImage;
    int startRow = task->startRow;
    int endRow = task->endRow;

    int width = image->header.width_px;
    int height = image->norm_height;

    // Kernel de desenfoque 3x3
    float kernel[KERNEL_SIZE][KERNEL_SIZE] = {
        {1.0/9, 1.0/9, 1.0/9},
        {1.0/9, 1.0/9, 1.0/9},
        {1.0/9, 1.0/9, 1.0/9}
    };

    for (int y = startRow; y < endRow; y++) {
        for (int x = 0; x < width; x++) {
            float red = 0.0, green = 0.0, blue = 0.0;

            // Aplicar el kernel de desenfoque
            for (int ky = 0; ky < KERNEL_SIZE; ky++) {
                for (int kx = 0; kx < KERNEL_SIZE; kx++) {
                    int pixelY = y + ky - 1;
                    int pixelX = x + kx - 1;

                    // Verificar límites de la imagen
                    if (pixelY >= 0 && pixelY < height && pixelX >= 0 && pixelX < width) {
                        Pixel *pixel = &image->pixels[pixelY][pixelX];
                        red += pixel->red * kernel[ky][kx];
                        green += pixel->green * kernel[ky][kx];
                        blue += pixel->blue * kernel[ky][kx];
                    }
                }
            }

            // Asignar el valor desenfocado al píxel correspondiente en la imagen desenfocada
            Pixel *blurredPixel = &blurredImage->pixels[y][x];
            blurredPixel->red = (uint8_t)red;
            blurredPixel->green = (uint8_t)green;
            blurredPixel->blue = (uint8_t)blue;
        }
    }

    return NULL;
}

void applyBlur(BMP_Image *image, int numThreads) {
    int width = image->header.width_px;
    int height = image->norm_height;

    BMP_Image *blurredImage = malloc(sizeof(BMP_Image));
    if (blurredImage == NULL) {
        perror("Error allocating memory for blurred image");
        return;
    }

    *blurredImage = *image;
    blurredImage->pixels = malloc(height * sizeof(Pixel *));
    if (blurredImage->pixels == NULL) {
        perror("Error allocating memory for blurred image pixels");
        free(blurredImage);
        return;
    }

    for (int i = 0; i < height; i++) {
        blurredImage->pixels[i] = malloc(width * sizeof(Pixel));
        if (blurredImage->pixels[i] == NULL) {
            perror("Error allocating memory for blurred image row");
            for (int j = 0; j < i; j++) {
                free(blurredImage->pixels[j]);
            }
            free(blurredImage->pixels);
            free(blurredImage);
            return;
        }
    }

    pthread_t threads[numThreads];
    BlurTask tasks[numThreads];
    int rowsPerThread = height / numThreads;

    for (int i = 0; i < numThreads; i++) {
        tasks[i].image = image;
        tasks[i].blurredImage = blurredImage;
        tasks[i].startRow = i * rowsPerThread;
        tasks[i].endRow = (i == numThreads - 1) ? height : (i + 1) * rowsPerThread;
        pthread_create(&threads[i], NULL, blurSection, &tasks[i]);
    }

    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < height; i++) {
        memcpy(image->pixels[i], blurredImage->pixels[i], width * sizeof(Pixel));
    }

    for (int i = 0; i < height; i++) {
        free(blurredImage->pixels[i]);
    }
    free(blurredImage->pixels);
    free(blurredImage);
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

    // Aplicar desenfoque
    applyBlur(image, numThreads);

    if (useShm) {
        // Escribir imagen procesada en memoria compartida
        int shmIdBlur = shmget(SHM_KEY_BLUR, 0, 0666);
        if (shmIdBlur < 0) {
            perror("Error accessing shared memory for blurrer");
            if (!useShm) freeImageMemory(image);
            return 1;
        }
        Pixel *shmBlur = (Pixel *)shmat(shmIdBlur, NULL, 0);
        if (shmBlur == (Pixel *)-1) {
            perror("Error attaching shared memory for blurrer");
            if (!useShm) freeImageMemory(image);
            return 1;
        }
        int width = image->header.width_px;
        int halfHeight = image->norm_height / 2;
        for (int i = 0; i < halfHeight; i++) {
            memcpy(shmBlur + i * width, image->pixels[i], width * sizeof(Pixel));
        }
        // No liberar la memoria compartida
        // shmdt(shmBlur);
    } else {
        // Guardar imagen procesada en archivo
        char *dot = strrchr(argv[1], '.');
        size_t baseLength = dot ? (size_t)(dot - argv[1]) : strlen(argv[1]);
        char *outputFile = malloc(baseLength + strlen("_blurred.bmp") + 1);
        if (outputFile == NULL) {
            printError(MEMORY_ERROR);
            freeImageMemory(image);
            return 1;
        }
        strncpy(outputFile, argv[1], baseLength);
        outputFile[baseLength] = '\0';
        strcat(outputFile, "_blurred.bmp");

        if (!writeImage(outputFile, image)) {
            printError(FILE_ERROR);
            free(outputFile);
            freeImageMemory(image);
            return 1;
        }

        free(outputFile);
    }

    if (!useShm) freeImageMemory(image);

    printf("Blurring completed successfully.\n");
    return 0;
}