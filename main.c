#include<stdio.h>
#include<string.h>
#include<math.h>
#include<stdlib.h>

typedef struct Pixel_Data {
    // Some image filters may need to temporarily store negative values for r,g,b
    int r, g, b;
} Pixel_Data;

typedef struct PPM_Image_Buffer {
    Pixel_Data* data;
    // Could be unsigned int. be carefull with implicit casting!!!!!!!!!!
    // (int a) * (unsigned int b)  - a is converted to unsigned int - BAD BUGS
    int rown, coln, max_val;
} PPM_Image_Buffer;

int read_ppm_color_bitmap(char* filename, PPM_Image_Buffer* buf) {
    // Open file
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "Could not open file\n");
        return -1;
    }

    // Read image format
    char format_str[10];
    fscanf(f, "%s", format_str);
    if (!strcmp(format_str, "P3\n"))
    {
        fprintf(stderr, "Invalid format string %s\n", format_str);
        return -1;
    }

    // Read image dimensions and value_range
    fscanf(f, "%d %d %d",&(buf->coln), &(buf->rown), &(buf->max_val));
    if (!(buf->coln) || !(buf->rown) || (buf->max_val) != 255)
    {
        fprintf(stderr, "Invalid image dimensions/value_range %u %u %u",
         buf->coln, buf->rown, buf->max_val);
        return -1;
    }

    // Read image data
    unsigned int N = buf->coln * buf->rown + buf->coln + 1;
    buf->data = (Pixel_Data*)(malloc(N*sizeof(Pixel_Data)));
    if(!buf->data) {
        perror("Could not malloc\n");
        return -1;
    }
    Pixel_Data* p = buf->data;
    for (unsigned int i = 0; i < N; i++, p++)
    {
        fscanf(f, "%d %d %d", &(p->r), &(p->g), &(p->b));
    }

    fclose(f);
    return 0;
}
int write_ppm_color_bitmap(char *filename, PPM_Image_Buffer *buf)
{
    // Open file for writing
    FILE *f = fopen(filename, "w");
    if (!f)
    {
        perror("Could not open file\n");
        return -1;
    }

    // Write format/ dimensions/max value into file
    fprintf(f, "P3\n%d %d\n%d\n", buf->coln, buf->rown, buf->max_val);

    // Write image data into file
    unsigned int N = (buf->coln)*(buf->rown);
    Pixel_Data* p = buf->data;
    for (int i = 0; i < N; i++, p++)
    {
        fprintf(f, "%u %u %u\n", p->r, p->g, p->b);
    }
    fclose(f);

    // Deallocate Pixel_Data array
    free(buf->data);
    return 0;
}
void foreach_pixel(
        PPM_Image_Buffer* buf,
        void (*f)(Pixel_Data *p_, int col, int row, PPM_Image_Buffer* buf)
    ) {

    int N = (buf->coln)*(buf->rown);
    Pixel_Data* p = buf->data;
    for(int i = 0; i < N; i++, p++) {
        // Call function on current pixel. Col and row are calculated from buffer index
        f(p, i%(buf->coln), i/(buf->coln), buf);
    }
}
void to_grayscale(Pixel_Data *p, int col, int row, PPM_Image_Buffer* buf) {
    double g = 0.3*(p->r) + 0.59*(p->g) + 0.11*(p->b);
    if(g > buf->max_val) g = buf->max_val;
    p->r = g;
    p->g = g;
    p->b = g;
}

// rgb_mask = 111 - keep all
// rgb_mask = 110 - remove red
// rgb_mask = 101 - remove green 
// rgb_mask = 011 - remove blue
// rgb_mask = 001 - remove blue/green
// Keep all by default
unsigned int rgb_mask = 111;
void remove_color_component(Pixel_Data *p, int col, int row, PPM_Image_Buffer* buf) {
    p->r = (rgb_mask&(1<<0))?(p->r):0;
    p->g = (rgb_mask&(1<<1))?(p->g):0;
    p->b = (rgb_mask&(1<<2))?(p->b):0;
}

// For each pixel chooses r,g,b values that are either 0 or max_val
// The generated error is distributed to 4 other pixels
void dither_pixel_floyd_steinberg(Pixel_Data *p, int col, int row, PPM_Image_Buffer *buf) {
    int err_r, err_g, err_b,
        new_r, new_g, new_b;
    // Either max_val or 0 for each color component
    // This makes at most 2^3 = 8 colors in resulting image
    new_r = (p->r > (buf->max_val/2))?(buf->max_val):0;
    new_g = (p->g > (buf->max_val/2))?(buf->max_val):0;
    new_b = (p->b > (buf->max_val/2))?(buf->max_val):0;

    // Calculate error
    err_r = p->r - new_r;
    err_g = p->g - new_g;
    err_b = p->b - new_b;

    // Store new values
    p->r = new_r;
    p->g = new_g;
    p->b = new_b;

    // Propagate error
    int offset_row[] = {0,  1, 1, 1},
        offset_col[] = {1, -1, 0, 1},
        coeff[]      = {7,  3, 5, 1}; // Floyd-Steinberg dithering coefficients
   for(int i = 0; i < 4; i++) {
        if (!(col + offset_col[i] >= 0 &&
              col + offset_col[i] < buf->coln &&
              row + offset_row[i] < buf->rown))
        {
            // Do not propagate if target pixel is outside of image
            continue;
        }
        // Add err/coeff[i] to pixel with
        // target_col = col + offset_col[i]
        // target_row = row + offset_row[i]
        Pixel_Data *target_pixel = &(
                buf->data[
                    col + offset_col[i] +
                    ((buf->coln)* (row + offset_row[i]))
                    ]
                );
        target_pixel->r += (err_r*(coeff[i]/16.));
        target_pixel->g += (err_g*(coeff[i]/16.));
        target_pixel->b += (err_b*(coeff[i]/16.));
    }
}

// Example function to demonstrate format
void print_color_ppm_1() {
    printf("P3\n");
    printf("3 2\n");
    printf("255\n");
    printf("255 0 0\n");
    printf("0 255 0\n");
    printf("0 0 255\n");
    printf("255 255 0\n");
    printf("255 0 255\n");
    printf("0 255 255\n");
}
int main() {
    PPM_Image_Buffer img1;
    read_ppm_color_bitmap("img4.ppm", &img1);
    // Grayscale
    // foreach_pixel(&img1, to_grayscale);

    // Remove green and blue components
    // rgb_mask = 1;
    // foreach_pixel(&img1, remove_color_component);

    // Remove blue components
    // rgb_mask = 3;
    // foreach_pixel(&img1, remove_color_component);

    // Dither with 8 colors
    // red, green, blue, yellow, purple, lightblue, white, black
    // All possible colors in format rgb(255/0, 255/0, 255/0)
    foreach_pixel(&img1, dither_pixel_floyd_steinberg);

    // Dither with black&white
    // foreach_pixel(&img1, to_grayscale);
    // foreach_pixel(&img1, dither_pixel_floyd_steinberg);

    write_ppm_color_bitmap("img6.ppm", &img1);
    return 0;
}