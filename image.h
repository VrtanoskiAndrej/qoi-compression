#ifndef _IMAGE_H
#define _IMAGE_H

/** universal image type */
typedef struct {
	uint8_t *bitstream;
	uint32_t height;
	uint32_t width;
	uint32_t color_type;
	uint32_t bit_depth;
} image_t;

#endif //_IMAGE_H