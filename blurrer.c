#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include "bmp.h"
#include "utils.h"

#define KERNEL_SIZE 3

typedef struct {
    BMP_Image *image;
    BMP_Image *blurredImage;
    int startRow;
    int endRow;
} BlurTask;

// Declarar el mutex global
pthread_mutex_t mutex;

void *blurSection(void *arg) {
    BlurTask *task = (BlurTask *)arg;
    BMP_Image *image = task->image;
    BMP_Image *blurredImage = task->blurredImage;
    int startRow = task->startRow;
    int endRow = task->endRow;

    int width = image->header.width_px;
    int height = image->norm_height;
    // printf("----------soy un hilo ejecutando  blur section ----------------\n");
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
    int halfHeight = height / 2;

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
    int rowsPerThread = halfHeight / numThreads;

    // Inicializar el mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Error initializing mutex");
        return;
    }

    for (int i = 0; i < numThreads; i++) {
        tasks[i].image = image;
        tasks[i].blurredImage = blurredImage;
        tasks[i].startRow = i * rowsPerThread;
        tasks[i].endRow = (i == numThreads - 1) ? halfHeight : (i + 1) * rowsPerThread;
        int ret = pthread_create(&threads[i], NULL, blurSection, &tasks[i]);
        if (ret != 0) {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(ret));
            return;
        }
        printf("Hilo %i creado correctamente\n", i);
    }

    for (int i = 0; i < numThreads; i++) {
        printf("-----------------uniendo hilo %i\n", i);
        int ret = pthread_join(threads[i], NULL);
        if (ret != 0) {
            fprintf(stderr, "Error joining thread %d: %s\n", i, strerror(ret));
            return;
        }
        printf("Hilo %i unido correctamente\n", i);
    }

    printf("--------si esto se lee todo va bien--------------\n");

    // Copiar la mitad desenfocada de la imagen de vuelta a la imagen original
    for (int i = 0; i < halfHeight; i++) {
        memcpy(image->pixels[i], blurredImage->pixels[i], width * sizeof(Pixel));
    }

    for (int i = 0; i < height; i++) {
        free(blurredImage->pixels[i]);
    }
    free(blurredImage->pixels);
    free(blurredImage);

    // Destruir el mutex
    pthread_mutex_destroy(&mutex);
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

    // Aplicar desenfoque
    applyBlur(image, numThreads);

    int result = 0;
    if (useShm) {
        result = writeImageToSharedMemory(image, SHM_KEY_IMAGE, UPPER_HALF);
    } else {
        result = saveImageToFile(image, argv[1]);
    }
    if (result != 0) {
        fprintf(stderr, "Error writing image.\n");
        return 1;
    }

    if (!useShm) freeImageMemory(image);

    printf("Blurring completed successfully.\n");
    return result;
}