#include "bmp.h"

// Inverse DCT
void perform_idct(double input[8][8], double output[8][8]) {
    double Cu, Cv, sum;
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            sum = 0.0;
            for (int u = 0; u < 8; u++) {
                for (int v = 0; v < 8; v++) {
                    Cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
                    Cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
                    sum += Cu * Cv * input[u][v] * 
                           cos((2 * x + 1) * u * PI / 16.0) * 
                           cos((2 * y + 1) * v * PI / 16.0);
                }
            }
            output[x][y] = 0.25 * sum;
        }
    }
}

// Write BMP file
int write_bmp(const char *filename, Pixel **pixels, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error opening output BMP file: %s\n", filename);
        return 1;
    }
    
    int padding = (4 - (width * 3) % 4) % 4;
    int dataSize = (width * 3 + padding) * height;
    
    BITMAPFILEHEADER fh = {
        .bfType = 0x4D42,
        .bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dataSize,
        .bfReserved1 = 0,
        .bfReserved2 = 0,
        .bfOffBits = 54
    };
    
    BITMAPINFOHEADER ih = {
        .biSize = 40,
        .biWidth = width,
        .biHeight = height,
        .biPlanes = 1,
        .biBitCount = 24,
        .biCompression = 0,
        .biSizeImage = dataSize,
        .biXPelsPerMeter = 2835,
        .biYPelsPerMeter = 2835,
        .biClrUsed = 0,
        .biClrImportant = 0
    };
    
    if (fwrite(&fh, sizeof(BITMAPFILEHEADER), 1, fp) != 1 ||
        fwrite(&ih, sizeof(BITMAPINFOHEADER), 1, fp) != 1) {
        fprintf(stderr, "Error writing BMP header\n");
        fclose(fp);
        return 1;
    }
    
    for (int i = height - 1; i >= 0; i--) {
        if (fwrite(pixels[i], sizeof(Pixel), width, fp) != (size_t)width) {
            fprintf(stderr, "Error writing pixel data\n");
            fclose(fp);
            return 1;
        }
        for (int k = 0; k < padding; k++) fputc(0, fp);
    }
    
    fclose(fp);
    return 0;
}

// YCbCr to RGB conversion
void ycbcr_to_rgb(double y, double cb, double cr, unsigned char *r, unsigned char *g, unsigned char *b) {
    // Standard BT.601 conversion
    double R = y + 1.402 * cr;
    double G = y - 0.344136 * cb - 0.714136 * cr;
    double B = y + 1.772 * cb;
    
    *r = clamp(R);
    *g = clamp(G);
    *b = clamp(B);
}

// Calculate PSNR
double calculate_psnr(const char *orig_file, Pixel **pixels, int width, int height) {
    FILE *fp = fopen(orig_file, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening original BMP file for PSNR calculation\n");
        return 0.0;
    }
    
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    
    if (fread(&fh, sizeof(fh), 1, fp) != 1 ||
        fread(&ih, sizeof(ih), 1, fp) != 1) {
        fprintf(stderr, "Error reading original BMP header\n");
        fclose(fp);
        return 0.0;
    }
    
    int padding = (4 - (width * 3) % 4) % 4;
    double mse = 0.0;
    long long count = 0;
    
    for (int i = 0; i < height; i++) {
        int row_idx = (ih.biHeight > 0) ? (height - 1 - i) : i;
        Pixel *row = (Pixel *)malloc(width * sizeof(Pixel));
        
        if (fread(row, sizeof(Pixel), width, fp) != (size_t)width) {
            fprintf(stderr, "Error reading original BMP pixel data\n");
            free(row);
            fclose(fp);
            return 0.0;
        }
        fseek(fp, padding, SEEK_CUR);
        
        for (int j = 0; j < width; j++) {
            double err_r = row[j].R - pixels[row_idx][j].R;
            double err_g = row[j].G - pixels[row_idx][j].G;
            double err_b = row[j].B - pixels[row_idx][j].B;
            mse += err_r*err_r + err_g*err_g + err_b*err_b;
            count++;
        }
        free(row);
    }
    fclose(fp);
    
    mse /= (3.0 * count);
    if (mse < 0.0001) return 999.0;
    return 10.0 * log10((255.0 * 255.0) / mse);
}

// Method 0: RGB channel reconstruction
int method_0_decoder(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr, "Usage: decoder 0 <out.bmp> <R.txt> <G.txt> <B.txt> <dim.txt>\n");
        return 1;
    }
    
    FILE *fdim = fopen(argv[6], "r");
    if (!fdim) {
        fprintf(stderr, "Error opening dim.txt\n");
        return 1;
    }
    
    int width, height;
    if (fscanf(fdim, "%d %d", &width, &height) != 2) {
        fprintf(stderr, "Error reading dimensions\n");
        fclose(fdim);
        return 1;
    }
    fclose(fdim);
    
    FILE *fr = fopen(argv[3], "r");
    FILE *fg = fopen(argv[4], "r");
    FILE *fb = fopen(argv[5], "r");
    
    if (!fr || !fg || !fb) {
        fprintf(stderr, "Error opening RGB text files\n");
        return 1;
    }
    
    Pixel **pixels = (Pixel **)malloc(height * sizeof(Pixel *));
    for (int i = 0; i < height; i++) {
        pixels[i] = (Pixel *)malloc(width * sizeof(Pixel));
        for (int j = 0; j < width; j++) {
            int r, g, b;
            if (fscanf(fr, "%d", &r) != 1 ||
                fscanf(fg, "%d", &g) != 1 ||
                fscanf(fb, "%d", &b) != 1) {
                fprintf(stderr, "Error reading RGB values\n");
                return 1;
            }
            pixels[i][j].R = (unsigned char)r;
            pixels[i][j].G = (unsigned char)g;
            pixels[i][j].B = (unsigned char)b;
        }
    }
    
    fclose(fr); 
    fclose(fg); 
    fclose(fb);
    
    if (write_bmp(argv[2], pixels, width, height)) {
        return 1;
    }
    
    for (int i = 0; i < height; i++) free(pixels[i]);
    free(pixels);
    
    printf("Method 0 Decoder Complete\n");
    return 0;
}

// Method 2: IDCT + Dequantization + PSNR
int method_2_decoder(int argc, char *argv[]) {
    if (argc < 11) {
        fprintf(stderr, "Usage: decoder 2 <orig.bmp> <out.bmp> <Qt_Y> <Qt_Cb> <Qt_Cr> <dim> <qF_Y.raw> <qF_Cb.raw> <qF_Cr.raw>\n");
        return 1;
    }
    
    // Read quantization tables
    int Q_Y[8][8], Q_Cb[8][8], Q_Cr[8][8];
    FILE *fqy = fopen(argv[3], "r");
    FILE *fqcb = fopen(argv[4], "r");
    FILE *fqcr = fopen(argv[5], "r");
    
    if (!fqy || !fqcb || !fqcr) {
        fprintf(stderr, "Error opening quantization table files\n");
        return 1;
    }
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (fscanf(fqy, "%d", &Q_Y[i][j]) != 1 ||
                fscanf(fqcb, "%d", &Q_Cb[i][j]) != 1 ||
                fscanf(fqcr, "%d", &Q_Cr[i][j]) != 1) {
                fprintf(stderr, "Error reading quantization tables\n");
                return 1;
            }
        }
    }
    fclose(fqy); 
    fclose(fqcb); 
    fclose(fqcr);
    
    // Read dimensions
    int width, height;
    FILE *fdim = fopen(argv[6], "r");
    if (!fdim || fscanf(fdim, "%d %d", &width, &height) != 2) {
        fprintf(stderr, "Error reading dimensions\n");
        return 1;
    }
    fclose(fdim);
    
    // Open quantized coefficient files
    FILE *fqfy = fopen(argv[7], "rb");
    FILE *fqfcb = fopen(argv[8], "rb");
    FILE *fqfcr = fopen(argv[9], "rb");
    
    if (!fqfy || !fqfcb || !fqfcr) {
        fprintf(stderr, "Error opening quantized coefficient files\n");
        return 1;
    }
    
    Pixel **pixels = (Pixel **)malloc(height * sizeof(Pixel *));
    for (int i = 0; i < height; i++) {
        pixels[i] = (Pixel *)malloc(width * sizeof(Pixel));
    }
    
    // Process each 8x8 block
    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            short zz_q_y[64], zz_q_cb[64], zz_q_cr[64];
            
            // Read quantized coefficients (in zig-zag order)
            if (fread(zz_q_y, sizeof(short), 64, fqfy) != 64 ||
                fread(zz_q_cb, sizeof(short), 64, fqfcb) != 64 ||
                fread(zz_q_cr, sizeof(short), 64, fqfcr) != 64) {
                fprintf(stderr, "Error reading quantized coefficients\n");
                return 1;
            }
            
            // Convert from zig-zag order back to 2D - FIX #1: Correct inverse zig-zag
            double dct_y[8][8], dct_cb[8][8], dct_cr[8][8];
            memset(dct_y, 0, sizeof(dct_y));
            memset(dct_cb, 0, sizeof(dct_cb));
            memset(dct_cr, 0, sizeof(dct_cr));
            
            // FIX #1: Reverse zig-zag lookup table
            for (int i = 0; i < 64; i++) {
                int zz_pos = zigzag_order[i];  // Get 2D position from zig-zag index
                int u = zz_pos / 8;
                int v = zz_pos % 8;
                
                // Dequantization
                dct_y[u][v] = zz_q_y[i] * Q_Y[u][v];
                dct_cb[u][v] = zz_q_cb[i] * Q_Cb[u][v];
                dct_cr[u][v] = zz_q_cr[i] * Q_Cr[u][v];
            }
            
            // IDCT
            double ycbcr_y[8][8], ycbcr_cb[8][8], ycbcr_cr[8][8];
            perform_idct(dct_y, ycbcr_y);
            perform_idct(dct_cb, ycbcr_cb);
            perform_idct(dct_cr, ycbcr_cr);
            
            // FIX #3: Proper level shift reversal
            // Encoder did: value - 128.0 before DCT
            // Decoder must do: value + 128.0 after IDCT
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    int py = by + i;
                    int px = bx + j;
                    
                    if (py >= height || px >= width) continue;
                    
                    // Reverse level shift
                    double y = ycbcr_y[i][j] + 128.0;
                    double cb = ycbcr_cb[i][j] + 128.0;
                    double cr = ycbcr_cr[i][j] + 128.0;
                    
                    // YCbCr to RGB conversion
                    ycbcr_to_rgb(y, cb, cr, 
                                 &pixels[py][px].R,
                                 &pixels[py][px].G,
                                 &pixels[py][px].B);
                }
            }
        }
    }
    
    fclose(fqfy); 
    fclose(fqfcb); 
    fclose(fqfcr);
    
    // Write output BMP
    if (write_bmp(argv[2], pixels, width, height)) {
        return 1;
    }
    
    // Calculate and save PSNR
    double psnr = calculate_psnr(argv[1], pixels, width, height);
    printf("PSNR: %.2f dB\n", psnr);
    
    FILE *fpsnr = fopen("psnr.txt", "w");
    if (fpsnr) {
        fprintf(fpsnr, "%.2f\n", psnr);
        fclose(fpsnr);
    }
    
    for (int i = 0; i < height; i++) free(pixels[i]);
    free(pixels);
    
    printf("Method 2 Decoder Complete\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ./decoder <method> ...\n");
        return 1;
    }
    
    int method = atoi(argv[1]);
    
    switch (method) {
        case 0:
            return method_0_decoder(argc, argv);
        case 2:
            return method_2_decoder(argc, argv);
        default:
            fprintf(stderr, "Unknown method: %d\n", method);
            return 1;
    }
}