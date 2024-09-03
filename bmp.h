//bmp.h
#ifndef _BMP_H_
#define _BMP_H_

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define TRUE 1
#define FALSE 0
#define ARGUMENT_ERROR 1
#define FILE_ERROR 2
#define MEMORY_ERROR 3
#define VALID_ERROR 4
#define HEADER_SIZE 54

#pragma pack(1)

typedef struct __attribute__((packed)) BMP_Header {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
    uint32_t header_size;
    int32_t width_px;
    int32_t height_px;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t imagesize;
    int32_t xresolution;
    int32_t yresolution;
    uint32_t ncolours;
    uint32_t importantcolours;
} BMP_Header;

typedef struct __attribute__((packed)) Pixel {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t alpha;
} Pixel;

typedef struct BMP_Image {
    BMP_Header header;
    int norm_height;
    int bytes_per_pixel;
    Pixel **pixels;
} BMP_Image;

void printError(int error);
BMP_Image* createBMPImage(FILE* fptr);
BMP_Image* readImage(FILE *srcFile);
void readImageData(FILE *srcFile, BMP_Image *dataImage, int dataSize);
bool writeImage(char* destFileName, BMP_Image* dataImage);
void freeImage(BMP_Image* image);
int checkBMPValid(BMP_Header* header);
void printBMPHeader(BMP_Header* header);
void printBMPImage(BMP_Image* image);

#endif /* _BMP_H_ */