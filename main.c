#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include "bmp.h"
#include "myutils.h"
#include "applyBlur.h"
#include "applyEdgeDetection.h"
#include "publishImage.h"

#define SHM_KEY1 1234
#define SHM_KEY2 5678


void manageArgs(int argc, char *argv[], char **inputFile, int *numThreads) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <num_threads>\n", argv[0]);
        exit(1);
    }
    *inputFile = argv[1];
    *numThreads = atoi(argv[2]);
    if (*numThreads <= 0) {
        fprintf(stderr, "Number of threads must be a positive integer.\n");
        exit(1);
    }
}

void verifyFile(const char *filename) {
    if (access(filename, F_OK) == -1) {
        fprintf(stderr, "Input file %s does not exist\n", filename);
        exit(1);
    }
}


int main(int argc, char *argv[]) {
    char *inputFile;
    int numThreads;
    manageArgs(argc, argv, &inputFile, &numThreads);
    verifyFile(inputFile);

    // Read the image from file
    FILE *file = fopen(inputFile, "rb");
    if (!file) {
        perror("fopen");
        exit(1);
    }
    BMP_Image *image = readImage(file);
    fclose(file);

    // Create shared memory for the blurred image
    int shmId1 = shmget(SHM_KEY1, sizeof(BMP_Image), IPC_CREAT | 0666);
    if (shmId1 < 0) {
        perror("shmget");
        exit(1);
    }
    BMP_Image *sharedImage1 = (BMP_Image *)shmat(shmId1, NULL, 0);
    if (sharedImage1 == (BMP_Image *)-1) {
        perror("shmat");
        exit(1);
    }

    // Create shared memory for the edge-detected image
    int shmId2 = shmget(SHM_KEY2, sizeof(BMP_Image), IPC_CREAT | 0666);
    if (shmId2 < 0) {
        perror("shmget");
        exit(1);
    }
    BMP_Image *sharedImage2 = (BMP_Image *)shmat(shmId2, NULL, 0);
    if (sharedImage2 == (BMP_Image *)-1) {
        perror("shmat");
        exit(1);
    }

    // Copy the image to both shared memory segments
    *sharedImage1 = *image;
    *sharedImage2 = *image;

    // Apply blur to the first shared image
    printf("Applying blur to the first shared image...\n");
    applyBlur(sharedImage1, numThreads);

    // Apply edge detection to the second shared image
    printf("Applying edge detection to the second shared image...\n");
    applyEdgeDetection(sharedImage2, numThreads);

    // Combine the images
    BMP_Image *combinedImage = (BMP_Image *)malloc(sizeof(BMP_Image));
    *combinedImage = *image;
    printf("Combining images...\n");
    combineImages(sharedImage1, sharedImage2, combinedImage);

    // Save the combined image to a file
    printf("Saving the combined image...\n");
    if (!writeImage("combined_image.bmp", combinedImage)) {
        fprintf(stderr, "Failed to save the combined image\n");
        exit(1);
    }

    // Detach and free shared memory
    shmdt(sharedImage1);
    shmdt(sharedImage2);
    shmctl(shmId1, IPC_RMID, NULL);
    shmctl(shmId2, IPC_RMID, NULL);

    // Free the combined image
    freeImage(combinedImage);

    printf("Process completed successfully.\n");
    return 0;
}
