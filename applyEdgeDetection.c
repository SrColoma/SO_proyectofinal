#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "bmp.h"
#include "myutils.h"
#include "applyEdgeDetection.h"

void* edgeDetectionSection(void* arg) {
    ThreadData *data = (ThreadData*)arg;
    BMP_Image *image = data->image;
    int startRow = data->startRow;
    int endRow = data->endRow;
    int width = image->header.width_px;
    int height = image->header.height_px;

    int gx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };

    int gy[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };

    for (int y = startRow; y < endRow; y++) {
        for (int x = 0; x < width; x++) {
            int sumXBlue = 0, sumXGreen = 0, sumXRed = 0;
            int sumYBlue = 0, sumYGreen = 0, sumYRed = 0;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy;
                    int nx = x + dx;
                    if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                        sumXBlue += image->pixels[ny][nx].blue * gx[dy + 1][dx + 1];
                        sumXGreen += image->pixels[ny][nx].green * gx[dy + 1][dx + 1];
                        sumXRed += image->pixels[ny][nx].red * gx[dy + 1][dx + 1];

                        sumYBlue += image->pixels[ny][nx].blue * gy[dy + 1][dx + 1];
                        sumYGreen += image->pixels[ny][nx].green * gy[dy + 1][dx + 1];
                        sumYRed += image->pixels[ny][nx].red * gy[dy + 1][dx + 1];
                    }
                }
            }

            int newBlue = sqrt(sumXBlue * sumXBlue + sumYBlue * sumYBlue);
            int newGreen = sqrt(sumXGreen * sumXGreen + sumYGreen * sumYGreen);
            int newRed = sqrt(sumXRed * sumXRed + sumYRed * sumYRed);

            image->pixels[y][x].blue = (newBlue > 255) ? 255 : newBlue;
            image->pixels[y][x].green = (newGreen > 255) ? 255 : newGreen;
            image->pixels[y][x].red = (newRed > 255) ? 255 : newRed;
        }
    }
    return NULL;
}

void applyEdgeDetection(BMP_Image *image, int numThreads) {
    int height = image->header.height_px;
    int halfHeight = height / 2;
    pthread_t threads[numThreads];
    ThreadData threadData[numThreads];

    int rowsPerThread = (height - halfHeight) / numThreads;
    for (int i = 0; i < numThreads; i++) {
        threadData[i].image = image;
        threadData[i].startRow = halfHeight + i * rowsPerThread;
        threadData[i].endRow = (i == numThreads - 1) ? height : halfHeight + (i + 1) * rowsPerThread;
        pthread_create(&threads[i], NULL, edgeDetectionSection, &threadData[i]);
    }

    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }
}