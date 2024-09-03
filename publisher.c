#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <pthread.h>
#include "bmp.h"
#include "utils.h"
 
#define SHM_KEY_IMAGE 1234
#define SHM_KEY_METADATA 1213
#define SHM_KEY_BLUR 5678
#define SHM_KEY_EDGE 91011


typedef struct {
    BMP_Image *shm_image;
    BMP_Image *image;
    int start;
    int end;
} ThreadData;

void* copyPixels(void *arg);

BMP_Image* createAndAttachSharedMemory(key_t shmKey, int width, int height) {
    int shm_id = shmget(shmKey, sizeof(BMP_Image) + width * height * sizeof(Pixel), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Error creating shared memory");
        return NULL;
    }

    BMP_Image *shm_image = (BMP_Image *)shmat(shm_id, NULL, 0);
    if (shm_image == (BMP_Image *)-1) {
        perror("Error attaching shared memory");
        return NULL;
    }

    return shm_image;
}

ImageMetadata* createAndAttachMetadataSharedMemory(key_t shmKey) {
    int shm_id = shmget(shmKey, sizeof(ImageMetadata), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Error creating shared memory for metadata");
        return NULL;
    }

    ImageMetadata *shm_metadata = (ImageMetadata *)shmat(shm_id, NULL, 0);
    if (shm_metadata == (ImageMetadata *)-1) {
        perror("Error attaching shared memory for metadata");
        return NULL;
    }

    return shm_metadata;
}

void* copyPixels(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int width = data->image->header.width_px;
    for (int i = data->start; i < data->end; i++) {
        data->shm_image->pixels[i] = (Pixel *)(data->shm_image->pixels + data->image->norm_height) + i * width;
        memcpy(data->shm_image->pixels[i], data->image->pixels[i], width * sizeof(Pixel));
    }
    return NULL;
}

void copyImageToSharedMemory(BMP_Image *shm_image, BMP_Image *image, int numThreads) {
    int width = image->header.width_px;
    int height = image->norm_height;
    pthread_t threads[numThreads];
    ThreadData threadData[numThreads];

    memcpy(shm_image, image, sizeof(BMP_Image));
    shm_image->pixels = (Pixel **)(shm_image + 1);

    int linesPerThread = height / numThreads;
    for (int i = 0; i < numThreads; i++) {
        threadData[i].shm_image = shm_image;
        threadData[i].image = image;
        threadData[i].start = i * linesPerThread;
        threadData[i].end = (i == numThreads - 1) ? height : (i + 1) * linesPerThread;
        pthread_create(&threads[i], NULL, copyPixels, &threadData[i]);
    }

    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <num_threads>\n", argv[0]);
        return 1;
    }

    int numThreads = atoi(argv[2]);
    if (numThreads <= 0) {
        fprintf(stderr, "Number of threads must be a positive integer.\n");
        return 1;
    }

    BMP_Image *image = readImageFromFile(argv[1]);
    if (image == NULL) {
        return 1;
    }

    if (verifyImage(image)) {
        return 1;
    }

    int width = image->header.width_px;
    int height = image->norm_height;

    // Publicar metadatos de la imagen
    printf("Publishing SHM_KEY_METADATA metadata...\n");
    ImageMetadata *shm_metadata = createAndAttachMetadataSharedMemory(SHM_KEY_METADATA);
    if (shm_metadata == NULL) {
        freeImage(image);
        return 1;
    }
    shm_metadata->width = width;
    shm_metadata->height = height;
    shm_metadata->norm_height = image->norm_height;



    printf("Publishing SHM_KEY_IMAGE ...\n");
    BMP_Image *shm_image = createAndAttachSharedMemory(SHM_KEY_IMAGE, width, height);
    if (shm_image == NULL) {
        freeImage(image);
        printf("shm_image is null\n");
        return 1;
    }
    printf("Copying image to shared memory SHM_KEY_IMAGE...\n");
    copyImageToSharedMemory(shm_image, image, numThreads);



    printf("Publishing SHM_KEY_BLUR ...\n");
    BMP_Image *shm_blur = createAndAttachSharedMemory(SHM_KEY_BLUR, width, height);
    if (shm_blur == NULL) {
        freeImage(image);
        return 1;
    }
    printf("Copying image to shared memory SHM_KEY_BLUR...\n");
    copyImageToSharedMemory(shm_blur, image, numThreads);



    printf("Publishing SHM_KEY_EDGE ...\n");
    BMP_Image *shm_edge = createAndAttachSharedMemory(SHM_KEY_EDGE, width, height);
    if (shm_edge == NULL) {
        freeImage(image);
        return 1;
    }
    printf("Copying image to shared memory SHM_KEY_EDGE...\n");
    copyImageToSharedMemory(shm_edge, image, numThreads);



    freeImage(image);

    printf("Image and metadata loaded into shared memory successfully.\n");

    return 0;
}