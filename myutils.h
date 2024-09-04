#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"

#define UPPER_HALF 0
#define LOWER_HALF 1

#define SHM_KEY_IMAGE 1111234

typedef struct {
    BMP_Image *image;
    int startRow;
    int endRow;
} ThreadData;

typedef struct {
    int width;
    int height;
    int norm_height;
} ImageMetadata;

void freeImageMemory(BMP_Image *image);

BMP_Image* readImageFromFile(const char *filename);

int verifyImage(BMP_Image *image);

BMP_Image* readImageFromSharedMemory(key_t shmKey) ;

int writeImageToSharedMemory(BMP_Image *image, key_t shmKey, int half);

int saveImageToFile(BMP_Image *image, const char *filename);

int executeCommand(const char *command, char *const argv[]);

void combineImages(BMP_Image *blurredImage, BMP_Image *edgeDetectedImage, BMP_Image *combinedImage);