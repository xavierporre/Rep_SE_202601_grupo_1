#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "image_data.h"

#define WIDTH  96
#define HEIGHT 96

// Resultado Sobel: (WIDTH-2) x (HEIGHT-2) = 94x94
static int16_t sobel_h[HEIGHT - 2][WIDTH - 2];
static int16_t sobel_v[HEIGHT - 2][WIDTH - 2];

void apply_sobel(void)
{
    // Kernel Sobel horizontal (Gx)
    // -1  0  1
    // -2  0  2
    // -1  0  1
    //
    // Kernel Sobel vertical (Gy)
    // -1 -2 -1
    //  0  0  0
    //  1  2  1

    for (int y = 1; y < HEIGHT - 1; y++) {
        for (int x = 1; x < WIDTH - 1; x++) {
            int gx = -image_data[y - 1][x - 1] + image_data[y - 1][x + 1]
                     - 2 * image_data[y][x - 1] + 2 * image_data[y][x + 1]
                     - image_data[y + 1][x - 1] + image_data[y + 1][x + 1];

            int gy = -image_data[y - 1][x - 1] - 2 * image_data[y - 1][x] - image_data[y - 1][x + 1]
                     + image_data[y + 1][x - 1] + 2 * image_data[y + 1][x] + image_data[y + 1][x + 1];

            sobel_h[y - 1][x - 1] = (int16_t)gx;
            sobel_v[y - 1][x - 1] = (int16_t)gy;
        }
    }
}

void print_matrix(const char *label, int16_t matrix[HEIGHT - 2][WIDTH - 2])
{
    printf("START_%s\n", label);
    for (int y = 0; y < HEIGHT - 2; y++) {
        for (int x = 0; x < WIDTH - 2; x++) {
            if (x > 0) printf(",");
            printf("%d", matrix[y][x]);
        }
        printf("\n");
    }
    printf("END_%s\n", label);
}

void app_main(void)
{
    printf("Aplicando operador Sobel a imagen %dx%d...\n", WIDTH, HEIGHT);

    apply_sobel();

    printf("Sobel completado. Imprimiendo resultados...\n\n");

    print_matrix("SOBEL_H", sobel_h);
    printf("\n");
    print_matrix("SOBEL_V", sobel_v);

    printf("\nListo.\n");
}
