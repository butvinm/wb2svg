#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define WB2SVG_IMPLEMENTATION
#include "wb2svg.h"


int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <file_path>\n", argv[0]);
        return 1;
    }

    const char* file_path = argv[1];

    int width, height;
    RGBA* pixels = (RGBA*)stbi_load(file_path, &width, &height, NULL, 4);
    if (pixels == NULL) {
        fprintf(stderr, "ERROR: could not read %s\n", file_path);
        return 1;
    }
    Img img = { .pixels = pixels, .width = width, .height = height };

    Img blur = img_alloc(width, height);
    gauss_filter(img, blur);
    if (!stbi_write_png("out/gauss.png", blur.width, blur.height, 4, blur.pixels, blur.width * sizeof(uint32_t))) {
        fprintf(stderr, "ERROR: could not save file out/gauss.png\n");
        return 1;
    }

    Img quantized = img_alloc(width, height);
    quantize(blur, quantized);
    if (!stbi_write_png("out/quantized.png", quantized.width, quantized.height, 4, quantized.pixels, quantized.width * sizeof(uint32_t))) {
        fprintf(stderr, "ERROR: could not save file out/quantized.png\n");
        return 1;
    }

    guo_hall_thinning(quantized);
    if (!stbi_write_png("out/thin.png", quantized.width, quantized.height, 4, quantized.pixels, quantized.width * sizeof(uint32_t))) {
        fprintf(stderr, "ERROR: could not save file out/thin.png\n");
        return 1;
    }

    char* svg = malloc(MAX_SVG_SIZE);
    if (wb2svg(quantized, svg, MAX_SVG_SIZE) < 0) {
        fprintf(stderr, "ERROR: buffer size exceeded\n");
        return 1;
    } else {
        FILE* svg_file = fopen("out/out.svg", "w");
        fprintf(svg_file, "%s", svg);
        fclose(svg_file);
        free(svg);
    }

    stbi_image_free(img.pixels);
    stbi_image_free(blur.pixels);
    stbi_image_free(quantized.pixels);
    return 0;
}
