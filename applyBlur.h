#include <stdio.h>
#include <stdlib.h>
#include "bmp.h"


void *blurSection(void *arg);
void applyBlur(BMP_Image *image, int numThreads);