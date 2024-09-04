#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "bmp.h"
#include "applyBlur.h"
#include "myutils.h"

void* blurSection(void* arg) {
    ThreadData *data = (ThreadData*)arg;
    BMP_Image *image = data->image;
    int startRow = data->startRow;
    int endRow = data->endRow;
    int width = image->header.width_px;
    int height = image->header.height_px;

    for (int y = startRow; y < endRow; y++) {
        for (int x = 0; x < width; x++) {
            int blue = 0, green = 0, red = 0, alpha = 0, count = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy;
                    int nx = x + dx;
                    if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                        blue += image->pixels[ny][nx].blue;
                        green += image->pixels[ny][nx].green;
                        red += image->pixels[ny][nx].red;
                        alpha += image->pixels[ny][nx].alpha;
                        count++;
                    }
                }
            }
            image->pixels[y][x].blue = blue / count;
            image->pixels[y][x].green = green / count;
            image->pixels[y][x].red = red / count;
            image->pixels[y][x].alpha = alpha / count;
        }
    }
    return NULL;
}

void applyBlur(BMP_Image *image, int numThreads) {
    int height = image->header.height_px;
    int halfHeight = height / 2;
    pthread_t threads[numThreads];
    ThreadData threadData[numThreads];

    int rowsPerThread = halfHeight / numThreads;
    for (int i = 0; i < numThreads; i++) {
        threadData[i].image = image;
        threadData[i].startRow = i * rowsPerThread;
        threadData[i].endRow = (i == numThreads - 1) ? halfHeight : (i + 1) * rowsPerThread;
        pthread_create(&threads[i], NULL, blurSection, &threadData[i]);
    }

    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }
}