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

    char* svg = malloc(MAX_SVG_SIZE);
    if (wb2svg(img, svg, MAX_SVG_SIZE) < 0) {
        fprintf(stderr, "ERROR: buffer size exceeded\n");
        free(svg);
        stbi_image_free(img.pixels);
        return 1;
    }

    FILE* svg_file = fopen("out.svg", "w");
    fprintf(svg_file, "%s", svg);
    fclose(svg_file);

    free(svg);
    stbi_image_free(img.pixels);
    return 0;
}
