#include <stdlib.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
} Img;

#define IMG_AT(img, row, col) (img).pixels[(row)*(img).width + (col)]

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
    mat.items = malloc(sizeof(float)*width*height);
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
            uint32_t pixel = (255  << (8*3))
                           + (item << (8*2))
                           + (item << (8*1))
                           + (item << (8*0));
            IMG_AT(img, y, x) = pixel;
        }
    }
}

static float rgb_to_lum(uint32_t rgb) {
    float r = ((rgb >> (8*0)) & 0xFF)/255.0;
    float g = ((rgb >> (8*1)) & 0xFF)/255.0;
    float b = ((rgb >> (8*2)) & 0xFF)/255.0;
    return 0.2126*r + 0.7152*g + 0.0722*b;
}

static void luminance(Img img, Mat lum) {
    assert(img.width == lum.width);
    assert(img.height == lum.height);
    for (int y = 0; y < lum.height; ++y) {
        for (int x = 0; x < lum.width; ++x) {
            MAT_AT(lum, y, x) = rgb_to_lum(IMG_AT(img, y, x));
        }
    }
}

static float gauss_filter_at(Mat mat, int cx, int cy) {
    static float g[5][5] = {
        {2.0,  4.0,  5.0,  4.0,  2.0},
        {4.0,  9.0,  12.0, 9.0,  4.0},
        {5.0,  12.0, 15.0, 12.0, 5.0},
        {4.0,  9.0,  12.0, 9.0,  4.0},
        {2.0,  4.0,  5.0,  4.0,  2.0}
    };

    float sx = 0.0;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            int x = cx + dx;
            int y = cy + dy;
            float c = MAT_WITHIN(mat, y, x) ? MAT_AT(mat, y, x) : 0.0;
            sx += c*g[dy + 2][dx + 2];
        }
    }
    return sx / 159;
}

static void gauss_filter(Mat mat, Mat grad) {
    assert(mat.width == grad.width);
    assert(mat.height == grad.height);
    for (int cy = 0; cy < mat.height; ++cy) {
        for (int cx = 0; cx < mat.width; ++cx) {
            MAT_AT(grad, cy, cx) = gauss_filter_at(mat, cx, cy);
        }
    }
}

static float sobel_filter_at(Mat mat, int cx, int cy) {
    static float gx[3][3] = {
        {1.0, 0.0, -1.0},
        {2.0, 0.0, -2.0},
        {1.0, 0.0, -1.0},
    };

    static float gy[3][3] = {
        {1.0, 2.0, 1.0},
        {0.0, 0.0, 0.0},
        {-1.0, -2.0, -1.0},
    };

    float sx = 0.0;
    float sy = 0.0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int x = cx + dx;
            int y = cy + dy;
            float c = MAT_WITHIN(mat, y, x) ? MAT_AT(mat, y, x) : 0.0;
            sx += c*gx[dy + 1][dx + 1];
            sy += c*gy[dy + 1][dx + 1];
        }
    }
    return sqrtf(sx*sx + sy*sy);
}

static void sobel_filter(Mat mat, Mat grad) {
    assert(mat.width == grad.width);
    assert(mat.height == grad.height);
    for (int cy = 0; cy < mat.height; ++cy) {
        for (int cx = 0; cx < mat.width; ++cx) {
            MAT_AT(grad, cy, cx) = sobel_filter_at(mat, cx, cy);
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <file_path>\n", argv[0]);
        return 1;
    }

    const char* file_path = argv[1];

    int width, height;
    uint32_t* pixels = (uint32_t*)stbi_load(file_path, &width, &height, NULL, 4);
    if (pixels == NULL) {
        fprintf(stderr, "ERROR: could not read %s\n", file_path);
        return 1;
    }
    Img img = { .pixels = pixels, .width = width, .height = height };

    Mat lum = mat_alloc(width, height);
    Mat gauss = mat_alloc(width, height);
    Mat grad = mat_alloc(width, height);
    luminance(img, lum);
    gauss_filter(lum, gauss);
    sobel_filter(gauss, grad);

    mat_to_img(lum, img);
    if (!stbi_write_png("out/lum.png", img.width, img.height, 4, img.pixels, img.width * sizeof(uint32_t))) {
        fprintf(stderr, "ERROR: could not save file out/lum.png\n");
        return 1;
    }

    mat_to_img(gauss, img);
    if (!stbi_write_png("out/gauss.png", img.width, img.height, 4, img.pixels, img.width * sizeof(uint32_t))) {
        fprintf(stderr, "ERROR: could not save file out/gauss.png\n");
        return 1;
    }

    mat_to_img(grad, img);
    if (!stbi_write_png("out/grad2.png", img.width, img.height, 4, img.pixels, img.width * sizeof(uint32_t))) {
        fprintf(stderr, "ERROR: could not save file out/grad2.png\n");
        return 1;
    }

    free(lum.items);
    free(gauss.items);
    free(grad.items);
    stbi_image_free(img.pixels);
    return 0;
}
