/**
You can include only declarations:

    #include "wb2svg.h"

Then in single file (e.g. main.c), include implementation:

    #define WB2SVG_IMPLEMENTATION
    #include "wb2svg.h"

Usage:
    RGBA* pixels = ; // pixels from any source
    Img img = { .pixels = pixels, .width = width, .height = height };

    char* svg = malloc(SVG_SIZE);
    if (wb2svg(img, svg, SVG_SIZE) < 0) {
        fprintf(stderr, "ERROR: buffer size exceeded.");
    } else {
        printf("%s", svg);
    }
*/

#ifndef WB2SVG_H
#define WB2SVG_H

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>


typedef struct {
    uint8_t r, g, b, a;
} RGBA;


typedef struct {
    RGBA *pixels;
    int width;
    int height;
} Img;


#define IMG_AT(img, row, col) (img).pixels[(row)*(img).width + (col)]
#define IMG_WITHIN(img, row, col) \
    (0 <= (col) && (col) < (img).width && 0 <= (row) && (row) < (img).height)


Img img_alloc(int width, int height) {
    Img img = {0};
    img.pixels = malloc(sizeof(RGBA)*width*height);
    assert(img.pixels != NULL);
    img.width = width;
    img.height = height;
    return img;
}


int wb2svg(Img img, char* buffer, int buffer_size);


#ifdef WB2SVG_IMPLEMENTATION

#define wb2svg__return(res) do { result = res; goto defer; } while(0)


typedef struct {
    float *items;
    int width;
    int height;
} Mat;


#define MAT_AT(mat, row, col) (mat).items[(row)*(mat).width + (col)]
#define MAT_WITHIN(mat, row, col) \
    (0 <= (col) && (col) < (mat).width && 0 <= (row) && (row) < (mat).height)


Mat mat_alloc(int width, int height) {
    Mat mat = {0};
    mat.items = calloc(width*height, sizeof(float));
    assert(mat.items != NULL);
    mat.width = width;
    mat.height = height;
    return mat;
}


static void mat_to_img(Mat mat, Img img) {
    assert(img.width == mat.width);
    assert(img.height == mat.height);
    for (int y = 0; y < mat.height; ++y) {
        for (int x = 0; x < mat.width; ++x) {
            uint32_t item = 255*MAT_AT(mat, y, x);
            IMG_AT(img, y, x) = (RGBA){ .r = item, .g = item, .b = item, .a = 255 };
        }
    }
}


typedef struct {
    float h; // Hue        (0.0-360.0 degrees)
    float s; // Saturation (0.0-1.0)
    float v; // Value      (0.0-1.0)
} HSV;


static HSV rgb_to_hsv(RGBA rgb) {
    HSV hsv;
    float r = rgb.r / 255.0f;
    float g = rgb.g / 255.0f;
    float b = rgb.b / 255.0f;

    float max = fmaxf(r, fmaxf(g, b));
    float min = fminf(r, fminf(g, b));
    float delta = max - min;

    if (delta == 0) {
        hsv.h = 0;
    } else if (max == r) {
        hsv.h = 60.0f * fmodf(((g - b) / delta), 6.0f);
    } else if (max == g) {
        hsv.h = 60.0f * (((b - r) / delta) + 2.0f);
    } else if (max == b) {
        hsv.h = 60.0f * (((r - g) / delta) + 4.0f);
    }

    if (hsv.h < 0) {
        hsv.h += 360.0f;
    }

    hsv.s = (max == 0) ? 0 : (delta / max);
    hsv.v = max;
    return hsv;
}


#define BLACK (RGBA){ .r = 0, .g = 0, .b = 0, .a = 255 }
#define WHITE (RGBA){ .r = 255, .g = 255, .b = 255, .a = 255 }
#define RED (RGBA){ .r = 255, .g = 0, .b = 0, .a = 255 }
#define GREEN (RGBA){ .r = 0, .g = 255, .b = 0, .a = 255 }
#define BLUE (RGBA){ .r = 0, .g = 0, .b = 255, .a = 255 }


static RGBA quantized_color(RGBA rgb) {
    HSV hsv = rgb_to_hsv(rgb);

    const float value_threshold_low = 0.2f;
    if (hsv.v <= value_threshold_low) {
        return BLACK;
    }

    const float saturation_threshold = 0.2f;
    const float value_threshold_high = 0.6f;
    if (hsv.v >= value_threshold_high && hsv.s <= saturation_threshold) {
        return WHITE;
    }

    if (hsv.s > saturation_threshold) {
        if (hsv.h >= 0 && hsv.h < 60) {
            return RED;
        } else if (hsv.h >= 60 && hsv.h < 180) {
            return GREEN;
        } else if (hsv.h >= 180 && hsv.h < 300) {
            return BLUE;
        } else {
            return RED;
        }
    }

    return WHITE;
}


static void quantize(Img img) {
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            IMG_AT(img, y, x) = quantized_color(IMG_AT(img, y, x));
        }
    }
}


static RGBA gauss_filter_at(Img img, int cx, int cy) {
    static float g[5][5] = {
        {2.0,  4.0,  5.0,  4.0,  2.0},
        {4.0,  9.0,  12.0, 9.0,  4.0},
        {5.0,  12.0, 15.0, 12.0, 5.0},
        {4.0,  9.0,  12.0, 9.0,  4.0},
        {2.0,  4.0,  5.0,  4.0,  2.0}
    };

    float sx_r = 0.0;
    float sx_g = 0.0;
    float sx_b = 0.0;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            int x = cx + dx;
            int y = cy + dy;
            RGBA c = IMG_WITHIN(img, y, x) ? IMG_AT(img, y, x) : BLACK;
            sx_r += c.r*g[dy + 2][dx + 2];
            sx_g += c.g*g[dy + 2][dx + 2];
            sx_b += c.b*g[dy + 2][dx + 2];
        }
    }
    return (RGBA){
        .r = floor(sx_r / 159),
        .g = floor(sx_g / 159),
        .b = floor(sx_b / 159),
        .a = 255
    };
}


static void gauss_filter(Img img, Img blur) {
    assert(img.width == blur.width);
    assert(img.height == blur.height);
    for (int cy = 0; cy < img.height; ++cy) {
        for (int cx = 0; cx < img.width; ++cx) {
            IMG_AT(blur, cy, cx) = gauss_filter_at(img, cx, cy);
        }
    }
}


#define IS_WHITE(pixel) (pixel.r == 255 && pixel.g == 255 && pixel.b == 255)


static void guo_hall_thinning_iteration(Img img, Mat marker, int iter) {
    memset(marker.items, 0.0, sizeof(float) * img.width * img.height);
    for (int y = 1; y < img.height; y++) {
        for (int x = 1; x < img.width; x++) {
            bool p2 = !IS_WHITE(IMG_AT(img, y-1, x));
            bool p3 = !IS_WHITE(IMG_AT(img, y-1, x+1));
            bool p4 = !IS_WHITE(IMG_AT(img, y, x+1));
            bool p5 = !IS_WHITE(IMG_AT(img, y+1, x+1));
            bool p6 = !IS_WHITE(IMG_AT(img, y+1, x));
            bool p7 = !IS_WHITE(IMG_AT(img, y+1, x-1));
            bool p8 = !IS_WHITE(IMG_AT(img, y, x-1));
            bool p9 = !IS_WHITE(IMG_AT(img, y-1, x-1));

            int C = (!p2 & (p3 | p4)) + (!p4 & (p5 | p6))
                  + (!p6 & (p7 | p8)) + (!p8 & (p9 | p2));
            int N1 = (p9 | p2) + (p3 | p4) + (p5 | p6) + (p7 | p8);
            int N2 = (p2 | p3) + (p4 | p5) + (p6 | p7) + (p8 | p9);
            int N = N1 < N2 ? N1 : N2;
            int m = iter == 0 ? ((p6 | p7 | !p9) & p8) : ((p2 | p3 | !p5) & p4);

            if (C == 1 && (N >= 2 && N <= 3) & (m == 0)) {
                MAT_AT(marker, y, x) = 1.0;
            }
        }
    }

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            if (MAT_AT(marker, y, x) == 1.0) {
                IMG_AT(img, y, x) = WHITE;
            }
        }
    }
}


static void guo_hall_thinning(Img img) {
    Mat marker = mat_alloc(img.width, img.height);
    for (int i = 0; i < 3; ++i) {
        guo_hall_thinning_iteration(img, marker, 0);
        guo_hall_thinning_iteration(img, marker, 1);
    };
    free(marker.items);
}


static void preprocess(Img img, Img processed) {
    assert(img.width == processed.width);
    assert(img.height == processed.height);

    gauss_filter(img, processed);
    quantize(processed);
    guo_hall_thinning(processed);
}


static void appendf(char* buffer, int buffer_size, int* cursor, const char* format, ...) {
    int remaining = buffer_size - *cursor;
    if (remaining <= 0) {
        *cursor = -1;
        return;
    }

    va_list args;
    va_start(args, format);
    int written = vsnprintf(buffer + *cursor, remaining, format, args);
    va_end(args);

    if (written < 0 || written >= remaining) {
        *cursor = -1;
    } else {
        *cursor += written;
    }
}


int wb2svg(Img img, char* buffer, int buffer_size) {
    int result = 0;
    int cursor = 0;

    if (!buffer || buffer_size <= 0) return -1;

    Img processed = img_alloc(img.width, img.height);
    preprocess(img, processed);

    appendf(
        buffer, buffer_size, &cursor,
        "<svg width=\"%d\" height=\"%d\" xmlns=\"http://www.w3.org/2000/svg\">",
        processed.width, processed.height
    );
    if (cursor < 0) wb2svg__return(-1);

    for (int cy = 0; cy < processed.height; cy++) {
        for (int cx = 0; cx < processed.width; cx++) {
            if (IS_WHITE(IMG_AT(processed, cy, cx))) {
                continue;
            } else {
                IMG_AT(processed, cy, cx) = WHITE;
            }
            for (int dy = -1; dy < 2; ++dy) {
                for (int dx = -1; dx < 2; ++dx) {
                    if (dy == 0 || dx == 0) continue;

                    int x = cx + dx;
                    int y = cy + dy;

                    if (IMG_WITHIN(processed, y, x) && !IS_WHITE(IMG_AT(processed, y, x))) {
                        appendf(
                            buffer, buffer_size, &cursor,
                            "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"red\" stroke-width=\"2\" />",
                            cx, cy, x, y
                        );
                        if (cursor < -1) wb2svg__return(-1);
                    }
                }
            }
        }
    }
    appendf(buffer, buffer_size, &cursor, "</svg>");
    if (cursor < -1) wb2svg__return(-1);

defer:
    free(processed.pixels);
    return result;
}

#endif // WB2SVG_IMPLEMENTATION
#endif // WB2SVG_H
