#include "bmp.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>


void *edgeSection(void *arg);

void applyEdgeDetection(BMP_Image *image, int numThreads);