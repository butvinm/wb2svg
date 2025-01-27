/**
You can include only declarations:

    #include "wb2svg.h"

Then in single file (e.g. main.c), include implementation:

    #define WB2SVG_IMPLEMENTATION
    #include "wb2svg.h"

Usage:
    wb2svg_rgba* pixels = ; // pixels from any source
    wb2svg_img img = { .pixels = pixels, .width = width, .height = height };

    char* svg = malloc(MAX_SVG_SIZE);
    if (wb2svg_wb2svg(img, svg, MAX_SVG_SIZE) < 0) {
        fprintf(stderr, "ERROR: buffer size exceeded.");
    } else {
        printf("%s", svg);
    }
*/

#ifndef WB2SVG_H
#define WB2SVG_H

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef WB2SVG_DEBUG
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif // WB2SVG_DEBUG

typedef struct {
    uint8_t r, g, b, a;
} wb2svg_rgba;


typedef struct {
    wb2svg_rgba *pixels;
    int width;
    int height;
} wb2svg_img;


wb2svg_img wb2svg_img_alloc(int width, int height);


int wb2svg_wb2svg(wb2svg_img img, char* buffer, int buffer_size);

#endif // WB2SVG_H


#ifdef WB2SVG_IMPLEMENTATION

#define WB2SVG__RETURN(res) do { result = res; goto defer; } while(0)

#define WB2SVG__IMG_AT(img, row, col) (img).pixels[(row)*(img).width + (col)]
#define WB2SVG__IMG_WITHIN(img, row, col) \
    (0 <= (col) && (col) < (img).width && 0 <= (row) && (row) < (img).height)


wb2svg_img wb2svg_img_alloc(int width, int height) {
    wb2svg_img img = {0};
    img.pixels = malloc(sizeof(wb2svg_rgba)*width*height);
    assert(img.pixels != NULL);
    img.width = width;
    img.height = height;
    return img;
}


typedef struct {
    float h; // Hue        (0.0-360.0 degrees)
    float s; // Saturation (0.0-1.0)
    float v; // Value      (0.0-1.0)
} wb2svg__hsv;


static wb2svg__hsv wb2svg__rgb_to_hsv(wb2svg_rgba rgb) {
    wb2svg__hsv hsv;
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


#define WB2SVG__BLACK (wb2svg_rgba){ .r = 0, .g = 0, .b = 0, .a = 255 }
#define WB2SVG__WHITE (wb2svg_rgba){ .r = 255, .g = 255, .b = 255, .a = 255 }
#define WB2SVG__RED   (wb2svg_rgba){ .r = 255, .g = 0, .b = 0, .a = 255 }
#define WB2SVG__GREEN (wb2svg_rgba){ .r = 0, .g = 255, .b = 0, .a = 255 }
#define WB2SVG__BLUE  (wb2svg_rgba){ .r = 0, .g = 0, .b = 255, .a = 255 }


static wb2svg_rgba wb2svg__quantize_rgb(wb2svg_rgba rgb) {
    wb2svg__hsv hsv = wb2svg__rgb_to_hsv(rgb);

    const float value_threshold_low = 0.2f;
    if (hsv.v <= value_threshold_low) {
        return WB2SVG__BLACK;
    }

    const float saturation_threshold = 0.2f;
    const float value_threshold_high = 0.6f;
    if (hsv.v >= value_threshold_high && hsv.s <= saturation_threshold) {
        return WB2SVG__WHITE;
    }

    if (hsv.s > saturation_threshold) {
        if (hsv.h >= 0 && hsv.h < 60) {
            return WB2SVG__RED;
        } else if (hsv.h >= 60 && hsv.h < 180) {
            return WB2SVG__GREEN;
        } else if (hsv.h >= 180 && hsv.h < 300) {
            return WB2SVG__BLUE;
        } else {
            return WB2SVG__RED;
        }
    }

    return WB2SVG__WHITE;
}


static void wb2svg__quantize(wb2svg_img img) {
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            WB2SVG__IMG_AT(img, y, x) = wb2svg__quantize_rgb(WB2SVG__IMG_AT(img, y, x));
        }
    }
}


static wb2svg_rgba wb2svg__gauss_filter_at(wb2svg_img img, int cx, int cy) {
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
            wb2svg_rgba c = WB2SVG__IMG_WITHIN(img, y, x) ? WB2SVG__IMG_AT(img, y, x) : WB2SVG__BLACK;
            sx_r += c.r*g[dy + 2][dx + 2];
            sx_g += c.g*g[dy + 2][dx + 2];
            sx_b += c.b*g[dy + 2][dx + 2];
        }
    }
    return (wb2svg_rgba){
        .r = floor(sx_r / 159),
        .g = floor(sx_g / 159),
        .b = floor(sx_b / 159),
        .a = 255
    };
}


static void wb2svg__gauss_filter(wb2svg_img img, wb2svg_img blur) {
    assert(img.width == blur.width);
    assert(img.height == blur.height);
    for (int cy = 0; cy < img.height; ++cy) {
        for (int cx = 0; cx < img.width; ++cx) {
            WB2SVG__IMG_AT(blur, cy, cx) = wb2svg__gauss_filter_at(img, cx, cy);
        }
    }
}


#define WB2SVG__IS_WHITE(pixel) (pixel.r == 255 && pixel.g == 255 && pixel.b == 255)
#define MARKER_AT(marker, y, x)


static void wb2svg__guo_hall_thinning_iteration(wb2svg_img img, bool* marker, int iter) {
    memset(marker, false, sizeof(bool) * img.width * img.height);
    for (int y = 1; y < img.height; y++) {
        for (int x = 1; x < img.width; x++) {
            bool p2 = !WB2SVG__IS_WHITE(WB2SVG__IMG_AT(img, y-1, x));
            bool p3 = !WB2SVG__IS_WHITE(WB2SVG__IMG_AT(img, y-1, x+1));
            bool p4 = !WB2SVG__IS_WHITE(WB2SVG__IMG_AT(img, y, x+1));
            bool p5 = !WB2SVG__IS_WHITE(WB2SVG__IMG_AT(img, y+1, x+1));
            bool p6 = !WB2SVG__IS_WHITE(WB2SVG__IMG_AT(img, y+1, x));
            bool p7 = !WB2SVG__IS_WHITE(WB2SVG__IMG_AT(img, y+1, x-1));
            bool p8 = !WB2SVG__IS_WHITE(WB2SVG__IMG_AT(img, y, x-1));
            bool p9 = !WB2SVG__IS_WHITE(WB2SVG__IMG_AT(img, y-1, x-1));

            int C = (!p2 & (p3 | p4)) + (!p4 & (p5 | p6))
                  + (!p6 & (p7 | p8)) + (!p8 & (p9 | p2));
            int N1 = (p9 | p2) + (p3 | p4) + (p5 | p6) + (p7 | p8);
            int N2 = (p2 | p3) + (p4 | p5) + (p6 | p7) + (p8 | p9);
            int N = N1 < N2 ? N1 : N2;
            int m = iter == 0 ? ((p6 | p7 | !p9) & p8) : ((p2 | p3 | !p5) & p4);

            if (C == 1 && (N >= 2 && N <= 3) & (m == 0)) {
                marker[y*img.width + x] = true;
            }
        }
    }

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            if (marker[y*img.width + x]) {
                WB2SVG__IMG_AT(img, y, x) = WB2SVG__WHITE;
            }
        }
    }
}


static void wb2svg__guo_hall_thinning(wb2svg_img img) {
    bool* marker = calloc(img.width * img.height, sizeof(bool));
    for (int i = 0; i < 3; ++i) {
        wb2svg__guo_hall_thinning_iteration(img, marker, 0);
        wb2svg__guo_hall_thinning_iteration(img, marker, 1);
    };
    free(marker);
}


static void wb2svg__preprocess(wb2svg_img img, wb2svg_img processed) {
    assert(img.width == processed.width);
    assert(img.height == processed.height);

    wb2svg__gauss_filter(img, processed);
    wb2svg__quantize(processed);
    wb2svg__guo_hall_thinning(processed);
    #ifdef WB2SVG_DEBUG
        if (!stbi_write_png("thin.png", processed.width, processed.height, 4, processed.pixels, processed.width * sizeof(uint32_t))) {
            fprintf(stderr, "ERROR: could not save file out/thin.png\n");
        }
    #endif // WB2SVG_DEBUG
}


static void wb2svg__appendf(char* buffer, int buffer_size, int* cursor, const char* format, ...) {
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


int wb2svg_wb2svg(wb2svg_img img, char* buffer, int buffer_size) {
    int result = 0;
    int cursor = 0;

    if (!buffer || buffer_size <= 0) return -1;

    wb2svg_img processed = wb2svg_img_alloc(img.width, img.height);
    wb2svg__preprocess(img, processed);

    wb2svg__appendf(
        buffer, buffer_size, &cursor,
        "<svg width=\"%d\" height=\"%d\" xmlns=\"http://www.w3.org/2000/svg\">",
        processed.width, processed.height
    );
    if (cursor < 0) WB2SVG__RETURN(cursor);

    bool path_emitted = false;
    int passed_y = 0;
    do {
        path_emitted = false;
        for (int cy = passed_y; cy < processed.height; ++cy) {
            for (int cx = 0; cx < processed.width; ++cx) {
                if (!WB2SVG__IS_WHITE(WB2SVG__IMG_AT(processed, cy, cx))) {
                    wb2svg_rgba color = WB2SVG__IMG_AT(processed, cy, cx);
                    wb2svg__appendf(
                        buffer, buffer_size, &cursor,
                        "<path fill=\"none\" stroke=\"rgb(%d, %d, %d)\" d=\"M %d %d ",
                        color.r, color.g, color.b, cx, cy
                    );
                    if (cursor < 0) WB2SVG__RETURN(cursor);

                    while (!WB2SVG__IS_WHITE(WB2SVG__IMG_AT(processed, cy, cx))) {
                        WB2SVG__IMG_AT(processed, cy, cx) = WB2SVG__WHITE;
                        for (int dy = -1; dy < 2; ++dy) {
                            for (int dx = -1; dx < 2; ++dx) {
                                if (dy == 0 && dx == 0) continue;

                                int ny = cy + dy;
                                int nx = cx + dx;
                                if (!WB2SVG__IMG_WITHIN(processed, ny, nx)) continue;

                                if (!WB2SVG__IS_WHITE(WB2SVG__IMG_AT(processed, ny, nx))) {
                                    wb2svg__appendf(
                                        buffer, buffer_size, &cursor,
                                        "L %d %d ", nx, ny
                                    );
                                    if (cursor < 0) WB2SVG__RETURN(cursor);

                                    cy = ny;
                                    cx = nx;
                                    goto next;
                                }
                            }
                        }
                        next: continue;
                    }

                    wb2svg__appendf(
                        buffer, buffer_size, &cursor,
                        "\" />"
                    );
                    if (cursor < 0) WB2SVG__RETURN(cursor);

                    path_emitted = true;
                    break;
                }
            }
            passed_y++;
        }
    } while (path_emitted);

    wb2svg__appendf(buffer, buffer_size, &cursor, "</svg>");
    if (cursor < 0) WB2SVG__RETURN(cursor);

    result = cursor;
defer:
    free(processed.pixels);
    return result;
}

#endif // WB2SVG_IMPLEMENTATION
