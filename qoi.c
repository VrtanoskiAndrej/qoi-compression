#include "qoi.h"

#ifndef DEBUG
#define DPRINTF(...)
#else
#define DPRINTF(...) fprintf(stderr, __VA_ARGS__);
#endif

#define SWAP(num)                                                 \
	(((num >> 24) & 0x000000ff) | ((num << 8) & 0x00ff0000) | \
	 ((num >> 8) & 0x0000ff00) | ((num << 24) & 0xff000000))

pixel_t pixel_map[64];

inline uint32_t qoi_hash(pixel_t px)
{
	return px.rgba.red * 3 + px.rgba.green * 5 + px.rgba.blue * 7 +
	       px.rgba.alpha * 11;
}

static void *memset32(void *m, uint32_t val, size_t count)
{
	uint32_t *buf = m;
	while (count--)
		*buf++ = val;
	return m;
}

QOI_FILE *read_qoi(char *filename)
{
	QOI_FILE *file = malloc(sizeof(QOI_FILE));
	QOI_HEADER header;

	FILE *fp = fopen(filename, "r");
	fread(&header, QOI_HEADER_SIZE, 1, fp);
	if (SWAP(header.magic) != QOI_MAGIC) {
		printf("invalid QOI_MAGIC\n");
		fclose(fp);
		exit(-1);
	}

	header.width = SWAP(header.width);
	header.height = SWAP(header.height);

	memset32(pixel_map, 0xFF000000, 64);
	uint32_t num_pixels = header.height * header.width;
	uint32_t maxsize = num_pixels * (header.channels + 1) + QOI_HEADER_SIZE;
	uint8_t *bytes = malloc(maxsize);
	pixel_t *image = malloc(num_pixels * sizeof(pixel_t));

	fread(bytes, maxsize, 1, fp);

	uint32_t image_idx = 0;
	uint32_t byte_ptr = 0;

	pixel_t px = (pixel_t)((uint32_t)0xFF000000);
	while (image_idx < num_pixels) {
		uint8_t curr_byte = bytes[byte_ptr++];

		// ********* QOI_OP_RGBA ********* //
		if (curr_byte == QOI_OP_RGBA) {
			px.rgba.red = bytes[byte_ptr++];
			px.rgba.green = bytes[byte_ptr++];
			px.rgba.blue = bytes[byte_ptr++];
			px.rgba.alpha = bytes[byte_ptr++];
			DPRINTF("QOI_OP_RGBA 0x%08x\n", px.v);
		}

		// ********* QOI_OP_RGB ********* //
		else if (curr_byte == QOI_OP_RGB) {
			px.rgba.red = bytes[byte_ptr++];
			px.rgba.green = bytes[byte_ptr++];
			px.rgba.blue = bytes[byte_ptr++];
			DPRINTF("QOI_OP_RGB 0x%08x\n", px.v);
		}

		// ********* QOI_OP_RUN ********* //
		else if ((curr_byte & QOI_MASK) == QOI_OP_RUN) {
			DPRINTF("QOI_OP_RUN %d\n", (curr_byte & 0x3f) + 1);
			memset32(&image[image_idx], px.v,
				 (curr_byte & 0x3f) + 1);
			image_idx += (curr_byte & 0x3f) + 1;
			continue;
		}

		// ********* QOI_OP_DIFF ********* //
		else if ((curr_byte & QOI_MASK) == QOI_OP_DIFF) {
			px.rgba.red += ((curr_byte >> 4) & 0x03) - 2;
			px.rgba.green += ((curr_byte >> 2) & 0x03) - 2;
			px.rgba.blue += (curr_byte & 0x03) - 2;
			DPRINTF("QOI_OP_DIFF dr: %d dg: %d db: %d 0x%08x\n",
				((curr_byte >> 4) & 0x03) - 2,
				((curr_byte >> 2) & 0x03) - 2,
				(curr_byte & 0x03) - 2, px.v);
		}

		// ********* QOI_OP_LUMA ********* //
		else if ((curr_byte & QOI_MASK) == QOI_OP_LUMA) {
			uint8_t next_byte = bytes[byte_ptr++];
			uint8_t dg = (curr_byte & 0x3f) - 32;
			px.rgba.red += dg - 8 + ((next_byte >> 4) & 0x0f);
			px.rgba.green += dg;
			px.rgba.blue += dg - 8 + (next_byte & 0x0f);
			DPRINTF("QOI_OP_LUMA 0x%08x\n", px.v);
		}

		// ********* QOI_OP_INDEX ********* //
		else if ((curr_byte & QOI_MASK) == QOI_OP_INDEX) {
			px = pixel_map[curr_byte & 0x3f];
			DPRINTF("QOI_OP_INDEX %d\n", curr_byte & 0x3f);
		}

		image[image_idx++] = px;
		pixel_map[qoi_hash(px) % 64] = px;
	}

	file->header = header;
	file->data = (uint8_t *)image;
	fclose(fp);

	return file;
}

int write_qoi(char *filename, QOI_FILE *contents)
{
	// clear indexing heap
	memset32(pixel_map, 0xFF000000, 64);
	uint32_t num_pixels = contents->header.height * contents->header.width;

	// worst case when all pixels are QOI_RGBA with opcode byte
	uint32_t maxsize =
		num_pixels * (contents->header.channels + 1) + QOI_HEADER_SIZE;
	uint8_t *bytes = malloc(maxsize);

	QOI_HEADER header = contents->header;
	header.magic = SWAP(QOI_MAGIC);
	header.height = SWAP(header.height);
	header.width = SWAP(header.width);

	memcpy(bytes, &header, QOI_HEADER_SIZE);
	uint32_t ptr = QOI_HEADER_SIZE;
	uint8_t run = 0;

	pixel_t prev = (pixel_t)((uint32_t)0xFF000000);
	for (uint32_t i = 0; i < num_pixels; i++) {
		pixel_t px = (pixel_t)(((uint32_t *)contents->data)[i]);

		// ********* QOI_OP_RUN ********* //
		if (px.v == prev.v) {
			run++;
			DPRINTF("QOI_OP_RUN\n");
			if (run == 62) {
				bytes[ptr++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}
			continue;
		}

		// qoi_run finished
		if (run > 0) {
			bytes[ptr++] = QOI_OP_RUN | (run - 1);
			run = 0;
		}

		// ********* QOI_OP_INDEX ********* //
		uint32_t index_pos = qoi_hash(px) % 64;
		if (pixel_map[index_pos].v == px.v) {
			DPRINTF("QOI_OP_INDEX\n");
			bytes[ptr++] = QOI_OP_INDEX | index_pos;
			prev = px;
			continue;
		}

		pixel_map[index_pos] = px;

		if (px.rgba.alpha == prev.rgba.alpha) {
			// ********* QOI_OP_DIFF ********* //
			int8_t dr = px.rgba.red - prev.rgba.red;
			int8_t dg = px.rgba.green - prev.rgba.green;
			int8_t db = px.rgba.blue - prev.rgba.blue;
			prev = px;

			if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 &&
			    db >= -2 && db <= 1) {
				DPRINTF("QOI_OP_DIFF\n");
				bytes[ptr++] = QOI_OP_DIFF | (dr + 2) << 4 |
					       (dg + 2) << 2 | (db + 2);
				continue;
			}

			// ********* QOI_OP_LUMA ********* //
			int8_t drg = dr - dg;
			int8_t dbg = db - dg;

			if (dg >= -32 && dg <= 31 && drg >= -8 && drg <= 7 &&
			    dbg >= -8 && dbg <= 7) {
				DPRINTF("QOI_OP_LUMA\n");
				bytes[ptr++] = QOI_OP_LUMA | (dg + 32);
				bytes[ptr++] = (drg + 8) << 4 | (dbg + 8);
				continue;
			}

			// ********* QOI_OP_RGB ********* //
			DPRINTF("QOI_OP_RGB\n");
			bytes[ptr++] = QOI_OP_RGB;
			bytes[ptr++] = px.rgba.red;
			bytes[ptr++] = px.rgba.green;
			bytes[ptr++] = px.rgba.blue;
		} else {
			prev = px;

			// ********* QOI_OP_RGBA ********* //
			DPRINTF("QOI_OP_RGBA\n");
			bytes[ptr++] = QOI_OP_RGBA;
			bytes[ptr++] = px.rgba.red;
			bytes[ptr++] = px.rgba.green;
			bytes[ptr++] = px.rgba.blue;
			bytes[ptr++] = px.rgba.alpha;
		}
	}

	if (run > 0)
		bytes[ptr++] = QOI_OP_RUN | (run - 1);

	FILE *fp = fopen(filename, "w");
	fwrite(bytes, ptr, 1, fp);
	fclose(fp);
	free(bytes);
	return 0;
}

image_t *read_image_qoi(char *filename)
{
	QOI_FILE *qoi_file_ptr = read_qoi(filename);
	image_t *image_ptr = malloc(sizeof(image_t));
	image_ptr->bitstream = qoi_file_ptr->data;
	image_ptr->height = qoi_file_ptr->header.height;
	image_ptr->width = qoi_file_ptr->header.width;
	return image_ptr;
}

int write_image_qoi(char *filename, image_t *image)
{
	QOI_HEADER qoi_header = { QOI_MAGIC, image->width, image->height, 4,
				  0 };
	QOI_FILE qoi_file = { qoi_header, image->bitstream };
	return write_qoi(filename, &qoi_file);
}
