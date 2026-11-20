#include "linux_gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(VILAOS_HAVE_LINUX_GFX) && !defined(_WIN32)

#include <png.h>

int linux_png_load(const char *path, uint8_t **rgba, int *w, int *h)
{
    if (!path || !rgba || !w || !h)
        return -1;
    *rgba = NULL;
    *w = *h = 0;
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return -1;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return -1;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        free(*rgba);
        *rgba = NULL;
        return -1;
    }
    png_init_io(png, fp);
    png_read_info(png, info);
    int width = (int)png_get_image_width(png, info);
    int height = (int)png_get_image_height(png, info);
    png_byte color = png_get_color_type(png, info);
    png_byte depth = png_get_bit_depth(png, info);
    if (depth == 16)
        png_set_strip_16(png);
    if (color == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (color == PNG_COLOR_TYPE_RGB || color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    size_t rowbytes = png_get_rowbytes(png, info);
    uint8_t *buf = (uint8_t *)malloc((size_t)height * rowbytes);
    if (!buf) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return -1;
    }
    png_bytep *rows = (png_bytep *)malloc(sizeof(png_bytep) * (size_t)height);
    if (!rows) {
        free(buf);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return -1;
    }
    for (int y = 0; y < height; y++)
        rows[y] = buf + (size_t)y * rowbytes;
    png_read_image(png, rows);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    *rgba = buf;
    *w = width;
    *h = height;
    return 0;
}

void linux_png_free(uint8_t *rgba)
{
    free(rgba);
}

#else

int linux_png_load(const char *path, uint8_t **rgba, int *w, int *h)
{
    (void)path;
    if (rgba)
        *rgba = NULL;
    if (w)
        *w = 0;
    if (h)
        *h = 0;
    return -1;
}

void linux_png_free(uint8_t *rgba)
{
    free(rgba);
}

#endif
