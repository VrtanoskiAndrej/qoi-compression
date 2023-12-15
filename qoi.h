#ifndef _QOI_H_
#define _QOI_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image.h"

#define QOI_MAGIC                                                \
	(((unsigned int)'q') << 24 | ((unsigned int)'o') << 16 | \
	 ((unsigned int)'i') << 8 | ((unsigned int)'f'))

#define QOI_HEADER_SIZE 14
#define QOI_PIXELS_MAX ((unsigned int)400000000)

#define QOI_OP_INDEX 0x00 /* 00xxxxxx */
#define QOI_OP_DIFF 0x40 /* 01xxxxxx */
#define QOI_OP_LUMA 0x80 /* 10xxxxxx */
#define QOI_OP_RUN 0xc0 /* 11xxxxxx */
#define QOI_OP_RGB 0xfe /* 11111110 */
#define QOI_OP_RGBA 0xff /* 11111111 */
#define QOI_MASK 0xc0 /* 11000000 */

#pragma pack(push) // save the original data alignment
#pragma pack(1) // Set data alignment to 1 byte boundary

typedef struct {
	uint32_t magic; // magic bytes "qoif"
	uint32_t width; // image width in pixels (BE)
	uint32_t height; // image height in pixels (BE)
	uint8_t channels; // 3 = RGB, 4 = RGBA
	uint8_t colorspace; // 0 = sRGB with linear alpha
} QOI_HEADER;

typedef union {
	struct {
		uint8_t red;
		uint8_t green;
		uint8_t blue;
		uint8_t alpha;
	} rgba;
	uint32_t v;
} pixel_t;

#pragma pack(pop) // restore the previous pack setting

typedef struct {
	QOI_HEADER header;
	uint8_t *data;
} QOI_FILE;

uint32_t qoi_hash(pixel_t);

image_t *read_image_qoi(char *);
int write_image_qoi(char *, image_t *);

QOI_FILE *read_qoi(char *);
int write_qoi(char *, QOI_FILE *);

#endif // _QOI_H_