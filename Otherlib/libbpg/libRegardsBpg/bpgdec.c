/*
 * BPG decoder command line utility
 *
 * Copyright (c) 2014 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <getopt.h>
#include <inttypes.h>
#include <string.h>
/* define it to include PNG output */
#define USE_PNG

#ifdef USE_PNG
#include <png.h>
#endif

#include "libbpg.h"



#ifdef USE_PNG


/* structure to store PNG image bytes */
typedef struct mem_encode
{
	uint8_t * buffer;
	size_t size;
}mem_encode;


//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
static void png_write_data_buffer(png_structp png_ptr, png_bytep data,
	png_size_t length)
{
	/* with libpng15 next line causes pointer deference error; use libpng12 */
	struct mem_encode* p = (struct mem_encode*)png_get_io_ptr(png_ptr); /* was png_ptr->io_ptr */
	size_t nsize = p->size + length;

	/* allocate or grow buffer */
	if (p->buffer)
		p->buffer = (uint8_t *)realloc(p->buffer, nsize);
	else
		p->buffer = (uint8_t *)malloc(nsize);

	if (!p->buffer)
		png_error(png_ptr, "Write Error");

	/* copy new bytes to end of buffer */
	memcpy((p->buffer + p->size), data, length);
	p->size += length;
}

static int png_save(BPGDecoderContext *img, mem_encode * mempng, int bit_depth)
{
	BPGImageInfo img_info_s, *img_info = &img_info_s;
	FILE *f;
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep row_pointer;
	int y, color_type, bpp;
	BPGDecoderOutputFormat out_fmt;


	mempng->buffer = 0;
	mempng->size = 0;

	if (bit_depth != 8 && bit_depth != 16) {
		fprintf(stderr, "Only bit_depth = 8 or 16 are supported for PNG output\n");
		return -1;
	}

	bpg_decoder_get_info(img, img_info);

	png_ptr = png_create_write_struct_2(PNG_LIBPNG_VER_STRING,
		NULL,
		NULL,  /* error */
		NULL, /* warning */
		NULL,
		NULL,
		NULL);
	info_ptr = png_create_info_struct(png_ptr);

	png_set_write_fn(png_ptr, (png_voidp)mempng, &png_write_data_buffer, NULL);

	if (setjmp(png_jmpbuf(png_ptr)) != 0) {
		fprintf(stderr, "PNG write error\n");
		return -1;
	}

	if (img_info->has_alpha)
		color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	else
		color_type = PNG_COLOR_TYPE_RGB;

	png_set_IHDR(png_ptr, info_ptr, img_info->width, img_info->height,
		bit_depth, color_type, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);

#if __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
	if (bit_depth == 16) {
		png_set_swap(png_ptr);
	}
#endif

	if (bit_depth == 16) {
		if (img_info->has_alpha)
			out_fmt = BPG_OUTPUT_FORMAT_RGBA64;
		else
			out_fmt = BPG_OUTPUT_FORMAT_RGB48;
	}
	else {
		if (img_info->has_alpha)
			out_fmt = BPG_OUTPUT_FORMAT_RGBA32;
		else
			out_fmt = BPG_OUTPUT_FORMAT_RGB24;
	}

	bpg_decoder_start(img, out_fmt);

	bpp = (3 + img_info->has_alpha) * (bit_depth / 8);
	row_pointer = (png_bytep)png_malloc(png_ptr, img_info->width * bpp);
	for (y = 0; y < img_info->height; y++) {
		bpg_decoder_get_line(img, row_pointer);
		png_write_row(png_ptr, row_pointer);
	}
	png_free(png_ptr, row_pointer);

	png_write_end(png_ptr, NULL);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	return 0;
}

#endif /* USE_PNG */

#ifdef MAIN
int main(int argc, char **argv)
{
    FILE *f;
    BPGDecoderContext *img;
    uint8_t *buf;
    int buf_len, bit_depth, c, show_info;
    const char *outfilename, *filename, *p;
    
    outfilename = "out.png";
    bit_depth = 8;
    show_info = 0;
    for(;;) {
        c = getopt(argc, argv, "ho:b:i");
        if (c == -1)
            break;
        switch(c) {
        case 'h':
        show_help:
            help();
            break;
        case 'o':
            outfilename = optarg;
            break;
        case 'b':
            bit_depth = atoi(optarg);
            break;
        case 'i':
            show_info = 1;
            break;
        default:
            exit(1);
        }
    }

    if (optind >= argc)
        goto show_help;

    filename = argv[optind++];

    if (show_info) {
        bpg_show_info(filename, 1);
        return 0;
    }

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    buf_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    buf = malloc(buf_len);
    if (fread(buf, 1, buf_len, f) != buf_len) {
        fprintf(stderr, "Error while reading file\n");
        exit(1);
    }
    
    fclose(f);

    img = bpg_decoder_open();

    if (bpg_decoder_decode(img, buf, buf_len) < 0) {
        fprintf(stderr, "Could not decode image\n");
        exit(1);
    }
    free(buf);

#ifdef USE_PNG
    p = strrchr(outfilename, '.');
    if (p)
        p++;

    if (p && strcasecmp(p, "ppm") != 0) {
        png_save(img, outfilename, bit_depth);
    } else 
#endif
    {
        ppm_save(img, outfilename);
    }

    bpg_decoder_close(img);

    return 0;
}
#endif





uint8_t * ReadToPNGPicture(uint8_t * buf, size_t buf_len, size_t * buf_out, int bit_depth, int * returnValue)
{
	mem_encode mempng;

	BPGDecoderContext *img;

	img = bpg_decoder_open();

	if (bpg_decoder_decode(img, buf, buf_len) < 0) {
		fprintf(stderr, "Could not decode image\n");
		return 0;
	}

	*returnValue = png_save(img, &mempng, bit_depth);


	bpg_decoder_close(img);

	buf_out = mempng.size;

	return mempng.buffer;
}
