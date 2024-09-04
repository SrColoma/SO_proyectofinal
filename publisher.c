#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <pthread.h>
#include "bmp.h"
#include "utils.h"
 

typedef struct {
    BMP_Image *shm_image;
    BMP_Image *image;
    int start;
    int end;
} ThreadData;

void* copyPixels(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int width = data->image->header.width_px;

    // Ensure we are not accessing out of bounds
    if (data->start < 0 || data->end > data->image->norm_height || data->start >= data->end) {
        fprintf(stderr, "Invalid thread range: start = %d, end = %d\n", data->start, data->end);
        pthread_exit(NULL);
    }

    for (int i = data->start; i < data->end; i++) {
        data->shm_image->pixels[i] = (Pixel *)(data->shm_image->pixels + data->image->norm_height) + i * width;
        memcpy(data->shm_image->pixels[i], data->image->pixels[i], width * sizeof(Pixel));
    }
    return NULL;
}

BMP_Image* createAndAttachSharedMemory(key_t shmKey, int width, int height) {
     printf("Creating and attaching shared memory for key: %i\n", shmKey);
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
    printf("Shared memory created and attached for key: %i\n", shmKey);
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

void copyImageToSharedMemory(BMP_Image *shm_image, BMP_Image *image, int numThreads) {
    if (shm_image == NULL || image == NULL) {
        printf("Error: Null pointer passed to copyImageToSharedMemory.\n");
        return;
    }
    
    printf("Copying image to shared memory...\n");

    int width = image->header.width_px;
    int height = image->norm_height;
    printf("Image dimensions: width = %d, height = %d\n", width, height);

    pthread_t threads[numThreads];
    ThreadData threadData[numThreads];

    printf("Copying BMP_Image structure...\n");
    memcpy(shm_image, image, sizeof(BMP_Image));
    shm_image->pixels = (Pixel **)(shm_image + 1);
    printf("BMP_Image structure copied. shm_image->pixels set.\n");

    int linesPerThread = height / numThreads;
    printf("Lines per thread: %d\n", linesPerThread);

    for (int i = 0; i < numThreads; i++) {
        threadData[i].shm_image = shm_image;
        threadData[i].image = image;
        threadData[i].start = i * linesPerThread;
        threadData[i].end = (i == numThreads - 1) ? height : (i + 1) * linesPerThread;
        printf("Thread %d: start = %d, end = %d\n", i, threadData[i].start, threadData[i].end);
        int ret = pthread_create(&threads[i], NULL, copyPixels, &threadData[i]);
        if (ret != 0) {
            printf("Error: pthread_create failed for thread %d\n", i);
        }
    }

    for (int i = 0; i < numThreads; i++) {
        int ret = pthread_join(threads[i], NULL);
        if (ret != 0) {
            printf("Error: pthread_join failed for thread %d\n", i);
        }
    }
    printf("Image copied to shared memory.\n");
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

    printf("Publishing SHM_KEY_IMAGE ...\n");
    BMP_Image *shm_image = createAndAttachSharedMemory(SHM_KEY_IMAGE, width, height);
    if (shm_image == NULL) {
        freeImage(image);
        printf("shm_image is null\n");
        return 1;
    }
    printf("Copying image to shared memory SHM_KEY_IMAGE...\n");
    copyImageToSharedMemory(shm_image, image, numThreads);

    freeImage(image);

    printf("Image loaded into shared memory successfully.\n");

    return 0;
}