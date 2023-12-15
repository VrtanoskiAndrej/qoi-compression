/** https://github.com/glennrp/libpng/blob/libpng16/example.c */

#include <png.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

#include "qoi.h"
#include "image.h"

#define panic(...)                            \
	{                                     \
		fprintf(stderr, __VA_ARGS__); \
		exit(EXIT_FAILURE);           \
	}

png_image image_png;

image_t *read_png(char *filename)
{
	image_t *image_ptr;
	image_ptr = malloc(sizeof(image_t));
	if (!image_ptr)
		panic("error: read_png malloc");

	// read png header
	if (png_image_begin_read_from_file(&image_png, filename) != 0) {
		png_bytep buffer;
		image_png.format = PNG_FORMAT_RGBA;

		image_ptr->height = image_png.height;
		image_ptr->width = image_png.width;

		buffer = malloc(PNG_IMAGE_SIZE(image_png));
		if (!buffer)
			panic("error: read_png unable to allocate space for png image buffer\n");

		// read png image data
		if (png_image_finish_read(&image_png, NULL, buffer, 0, NULL)) {
			image_ptr->bitstream = buffer;
			return image_ptr;
		} else {
			panic("error: unable to read png file %s\n",
			      image_png.message);
		}
	} else {
		panic("error: %s\n", image_png.message);
	}
	return 0;
}

void process_file(image_t *image)
{
	for (int p = 0; p < (image->height * image->width); p++) {
		uint8_t *pixel = &(image->bitstream[p * 4]);
		int avg = (pixel[0] + pixel[1] + pixel[2]) / 3;
		pixel[0] = avg;
		pixel[1] = avg;
		pixel[2] = avg;
	}
}

void write_png(image_t *image_ptr, char *filename_out)
{
	image_png.height = image_ptr->height;
	image_png.width = image_ptr->width;
	image_png.colormap_entries = 256;
	image_png.format = PNG_FORMAT_RGBA;
	image_png.opaque = 0;

	// write to png file
	if (png_image_write_to_file(&image_png, filename_out, 0,
				    image_ptr->bitstream, 0, NULL) == 0) {
		panic("write error %s\n", image_png.message);
	}
}

int main(int argc, char const *argv[])
{
	memset(&image_png, 0, sizeof(png_image));
	image_png.version = PNG_IMAGE_VERSION;

	image_t *image_ptr;
	char *intermediary = "tmp.qoi";
	clock_t start;
	clock_t png_encode, png_decode, qoi_encode, qoi_decode;

	// PNG DECODING
	start = clock();
	image_ptr = read_png(argv[1]);
	png_decode = clock() - start;

	// QOI ENCODING
	start = clock();
	write_image_qoi(intermediary, image_ptr);
	qoi_encode = clock() - start;

	// QOI DECODE
	start = clock();
	image_ptr = read_image_qoi(intermediary);
	qoi_decode = clock() - start;

	// PNG ENCODE
	start = clock();
	write_png(image_ptr, "out.png");
	png_encode = clock() - start;

	printf("%-8s, %6s, %6s\n", "format", "encode", "decode");
	printf("%-8s, %6d, %6d\n", "png", png_encode, png_decode);
	printf("%-8s, %6d, %6d\n", "qoi", qoi_encode, qoi_decode);
	return 0;
}
