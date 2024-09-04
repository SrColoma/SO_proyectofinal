#include <sys/ipc.h>
#include <sys/shm.h>
#include "bmp.h"


int publishImage(BMP_Image *image, key_t shmKey);