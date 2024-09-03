//bmp.c
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "bmp.h"

/* Función para imprimir mensajes de error */
void printError(int error){
  switch(error){
    case ARGUMENT_ERROR:
      printf("Usage:ex5 <source> <destination>\n");
      break;
    case FILE_ERROR:
      printf("Unable to open file!\n");
      break;
    case MEMORY_ERROR:
      printf("Unable to allocate memory!\n");
      break;
    case VALID_ERROR:
      printf("BMP file not valid!\n");
      break;
    default:
      break;
  }
}

BMP_Image* createBMPImage(FILE* fptr) {
    BMP_Image* image = (BMP_Image*)malloc(sizeof(BMP_Image));
    if (image == NULL) {
        printError(MEMORY_ERROR);
        return NULL;
    }

    if (fptr != NULL) {
        fread(&image->header, sizeof(BMP_Header), 1, fptr);
        image->norm_height = abs(image->header.height_px);
        image->bytes_per_pixel = image->header.bits_per_pixel / 8;
        image->pixels = (Pixel**)malloc(image->norm_height * sizeof(Pixel*));
        for (int i = 0; i < image->norm_height; i++) {
            image->pixels[i] = (Pixel*)malloc(image->header.width_px * sizeof(Pixel));
        }
    } else {
        image->pixels = NULL;
    }

    return image;
}

void readImageData(FILE *srcFile, BMP_Image *dataImage, int dataSize) {
    fseek(srcFile, dataImage->header.offset, SEEK_SET);
    for (int i = 0; i < dataImage->norm_height; i++) {
        fread(dataImage->pixels[i], dataImage->bytes_per_pixel, dataImage->header.width_px, srcFile);
    }
}

BMP_Image* readImage(FILE *srcFile) {
    BMP_Image *image = (BMP_Image *)malloc(sizeof(BMP_Image));
    if (image == NULL) {
        printError(MEMORY_ERROR);
        return NULL;
    }

    fread(&image->header, sizeof(BMP_Header), 1, srcFile);
    if (checkBMPValid(&image->header) != TRUE) {
        free(image);
        printError(VALID_ERROR);
        return NULL;
    }

    image->norm_height = abs(image->header.height_px);
    image->bytes_per_pixel = image->header.bits_per_pixel / 8;
    image->pixels = (Pixel **)malloc(image->norm_height * sizeof(Pixel *));
    if (image->pixels == NULL) {
        free(image);
        printError(MEMORY_ERROR);
        return NULL;
    }

    for (int i = 0; i < image->norm_height; i++) {
        image->pixels[i] = (Pixel *)malloc(image->header.width_px * sizeof(Pixel));
        if (image->pixels[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(image->pixels[j]);
            }
            free(image->pixels);
            free(image);
            printError(MEMORY_ERROR);
            return NULL;
        }
    }

    readImageData(srcFile, image, image->header.imagesize);
    return image;
}

bool writeImage(char *destFileName, BMP_Image *dataImage) {
    FILE *destFile = fopen(destFileName, "wb");
    if (destFile == NULL) {
        printError(FILE_ERROR);
        return FALSE;
    }

    fwrite(&dataImage->header, sizeof(BMP_Header), 1, destFile);
    fseek(destFile, dataImage->header.offset, SEEK_SET);
    for (int i = 0; i < dataImage->norm_height; i++) {
        fwrite(dataImage->pixels[i], dataImage->bytes_per_pixel, dataImage->header.width_px, destFile);
    }

    fclose(destFile);
    return TRUE;
}

void freeImage(BMP_Image* image) {
    for (int i = 0; i < image->norm_height; i++) {
        free(image->pixels[i]);
    }
    free(image->pixels);
    free(image);
}

int checkBMPValid(BMP_Header* header) {
    if (header->type != 0x4d42) {
        return FALSE;
    }
    if (header->bits_per_pixel != 24 && header->bits_per_pixel != 32) {
        return FALSE;
    }
    if (header->planes != 1) {
        return FALSE;
    }
    if (header->compression != 0) {
        return FALSE;
    }
    return TRUE;
}

void printBMPHeader(BMP_Header* header) {
    printf("file type (should be 0x4d42): %x\n", header->type);
    printf("file size: %d\n", header->size);
    printf("offset to image data: %d\n", header->offset);
    printf("header size: %d\n", header->header_size);
    printf("width_px: %d\n", header->width_px);
    printf("height_px: %d\n", header->height_px);
    printf("planes: %d\n", header->planes);
    printf("bits: %d\n", header->bits_per_pixel);
}

void printBMPImage(BMP_Image* image) {
    printf("data size is %ld\n", sizeof(image->pixels));
    printf("norm_height size is %d\n", image->norm_height);
    printf("bytes per pixel is %d\n", image->bytes_per_pixel);
}
