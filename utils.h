#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"
typedef struct {
    int width;
    int height;
    int norm_height;
} ImageMetadata;

void freeImageMemory(BMP_Image *image);

BMP_Image* readImageFromFile(const char *filename);

int verifyImage(BMP_Image *image);

BMP_Image* readImageFromSharedMemory(key_t shmKey) ;

int writeImageToSharedMemory(BMP_Image *image, key_t shmKey);

int saveImageToFile(BMP_Image *image, const char *filename);