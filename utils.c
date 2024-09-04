#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"
#include "utils.h"


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
    printf("Accessing shared memory for key: %i\n", shmKey);
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


int writeImageToSharedMemory(BMP_Image *image, key_t shmKey, int half) {
    printf("Writing shared memory for key: %i\n", shmKey);
    int shmId = shmget(shmKey, 0, 0666);
    if (shmId < 0) {
        perror("Error accessing shared memory");
        return 1;
    }
    Pixel *shm = (Pixel *)shmat(shmId, NULL, 0);
    if (shm == (Pixel *)-1) {
        perror("Error attaching shared memory");
        return 1;
    }
    int width = image->header.width_px;
    int height = image->norm_height;
    int halfHeight = height / 2;

    if (half == UPPER_HALF) {
        for (int i = 0; i < halfHeight; i++) {
            memcpy(shm + i * width, image->pixels[i], width * sizeof(Pixel));
        }
    } else if (half == LOWER_HALF) {
        for (int i = halfHeight; i < height; i++) {
            memcpy(shm + (i - halfHeight) * width, image->pixels[i], width * sizeof(Pixel));
        }
    } else {
        fprintf(stderr, "Invalid half specified. Use UPPER_HALF or LOWER_HALF.\n");
        return 1;
    }

    // No liberar la memoria compartida
    // shmdt(shm);
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

int executeCommand(const char *command, char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        // Error en fork
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Proceso hijo
        printf("Executing command: %s\n", command); // Mensaje de depuración
        execvp(command, argv);
        // Si execvp falla
        perror("execvp");
        exit(1);
    } else {
        // Proceso padre
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return 1;
        }
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                fprintf(stderr, "Error executing %s\n", command);
                return 1;
            }
        } else {
            fprintf(stderr, "%s did not terminate normally\n", command);
            return 1;
        }
    }
    return 0;
}