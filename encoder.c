#include "bmp.h"

// Standard JPEG quantization matrices
static const int std_qtable_Y[8][8] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

static const int std_qtable_C[8][8] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

// Forward DCT
void perform_dct(double input[8][8], double output[8][8]) {
    double Cu, Cv;
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            Cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            Cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
            
            double sum = 0.0;
            for (int x = 0; x < 8; x++) {
                for (int y = 0; y < 8; y++) {
                    sum += input[x][y] * 
                           cos((2 * x + 1) * u * PI / 16.0) * 
                           cos((2 * y + 1) * v * PI / 16.0);
                }
            }
            output[u][v] = 0.25 * Cu * Cv * sum;
        }
    }
}

// Read BMP file
Pixel** read_bmp(const char *filename, int *width, int *height) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return NULL;
    }
    
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    
    if (fread(&fh, sizeof(BITMAPFILEHEADER), 1, fp) != 1 ||
        fread(&ih, sizeof(BITMAPINFOHEADER), 1, fp) != 1) {
        fprintf(stderr, "Error reading BMP header\n");
        fclose(fp);
        return NULL;
    }
    
    *width = ih.biWidth;
    *height = abs(ih.biHeight);
    
    int padding = (4 - (*width * 3) % 4) % 4;
    Pixel **pixels = (Pixel **)malloc(*height * sizeof(Pixel *));
    
    for (int i = 0; i < *height; i++) {
        pixels[i] = (Pixel *)malloc(*width * sizeof(Pixel));
        int row_idx = (ih.biHeight > 0) ? (*height - 1 - i) : i;
        
        if (fread(pixels[row_idx], sizeof(Pixel), *width, fp) != (size_t)*width) {
            fprintf(stderr, "Error reading BMP pixel data\n");
            fclose(fp);
            return NULL;
        }
        fseek(fp, padding, SEEK_CUR);
    }
    
    fclose(fp);
    return pixels;
}

// RGB to YCbCr conversion (BT.601) - FIX #2: Added +128 offset for Cb and Cr
void rgb_to_ycbcr(Pixel pixel, double *y, double *cb, double *cr) {
    double r = pixel.R;
    double g = pixel.G;
    double b = pixel.B;
    
    // YCbCr conversion with proper DC offset
    *y = 0.299 * r + 0.587 * g + 0.114 * b;
    *cb = -0.168736 * r - 0.331264 * g + 0.5 * b + 128.0;     // FIX: +128
    *cr = 0.5 * r - 0.418688 * g - 0.081312 * b + 128.0;      // FIX: +128
}

// Method 0: Extract RGB channels
int method_0_encoder(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr, "Usage: encoder 0 <bmp> <R.txt> <G.txt> <B.txt> <dim.txt>\n");
        return 1;
    }
    
    int width, height;
    Pixel **pixels = read_bmp(argv[2], &width, &height);
    if (!pixels) return 1;
    
    FILE *fr = fopen(argv[3], "w");
    FILE *fg = fopen(argv[4], "w");
    FILE *fb = fopen(argv[5], "w");
    
    if (!fr || !fg || !fb) {
        fprintf(stderr, "Error opening output files\n");
        return 1;
    }
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (j > 0) { 
                fprintf(fr, " "); 
                fprintf(fg, " "); 
                fprintf(fb, " "); 
            }
            fprintf(fr, "%d", pixels[i][j].R);
            fprintf(fg, "%d", pixels[i][j].G);
            fprintf(fb, "%d", pixels[i][j].B);
        }
        fprintf(fr, "\n");
        fprintf(fg, "\n");
        fprintf(fb, "\n");
    }
    
    fclose(fr); 
    fclose(fg); 
    fclose(fb);
    
    FILE *fdim = fopen(argv[6], "w");
    fprintf(fdim, "%d %d\n", width, height);
    fclose(fdim);
    
    for (int i = 0; i < height; i++) free(pixels[i]);
    free(pixels);
    
    printf("Method 0 Encoder Complete\n");
    return 0;
}

// Method 1: DCT + Quantization
int method_1_encoder(int argc, char *argv[]) {
    if (argc < 13) {
        fprintf(stderr, "Usage: encoder 1 <bmp> <Qt_Y> <Qt_Cb> <Qt_Cr> <dim> <qF_Y.raw> <qF_Cb.raw> <qF_Cr.raw> <eF_Y.raw> <eF_Cb.raw> <eF_Cr.raw>\n");
        return 1;
    }
    
    int width, height;
    Pixel **pixels = read_bmp(argv[2], &width, &height);
    if (!pixels) return 1;
    
    // Output quantization tables
    FILE *fqty = fopen(argv[3], "w");
    FILE *fqtcb = fopen(argv[4], "w");
    FILE *fqtcr = fopen(argv[5], "w");
    
    if (!fqty || !fqtcb || !fqtcr) {
        fprintf(stderr, "Error opening quantization table files\n");
        return 1;
    }
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (j > 0) {
                fprintf(fqty, " ");
                fprintf(fqtcb, " ");
                fprintf(fqtcr, " ");
            }
            fprintf(fqty, "%d", std_qtable_Y[i][j]);
            fprintf(fqtcb, "%d", std_qtable_C[i][j]);
            fprintf(fqtcr, "%d", std_qtable_C[i][j]);
        }
        fprintf(fqty, "\n");
        fprintf(fqtcb, "\n");
        fprintf(fqtcr, "\n");
    }
    
    fclose(fqty); 
    fclose(fqtcb); 
    fclose(fqtcr);
    
    // Output dimensions
    FILE *fdim = fopen(argv[6], "w");
    fprintf(fdim, "%d %d\n", width, height);
    fclose(fdim);
    
    // Open output files for quantized coefficients
    FILE *fqfy = fopen(argv[7], "wb");
    FILE *fqfcb = fopen(argv[8], "wb");
    FILE *fqfcr = fopen(argv[9], "wb");
    FILE *fefy = fopen(argv[10], "wb");
    FILE *fefcb = fopen(argv[11], "wb");
    FILE *fefcr = fopen(argv[12], "wb");
    
    if (!fqfy || !fqfcb || !fqfcr || !fefy || !fefcb || !fefcr) {
        fprintf(stderr, "Error opening coefficient files\n");
        return 1;
    }
    
    // Process each 8x8 block
    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            double ycbcr_y[8][8], ycbcr_cb[8][8], ycbcr_cr[8][8];
            
            // Convert to YCbCr and level shift - FIX #3: All components get level shift
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    int py = by + i;
                    int px = bx + j;
                    
                    if (py >= height) py = height - 1;
                    if (px >= width) px = width - 1;
                    
                    double y, cb, cr;
                    rgb_to_ycbcr(pixels[py][px], &y, &cb, &cr);
                    
                    // Level shift all components to -128...127 range
                    ycbcr_y[i][j] = y - 128.0;
                    ycbcr_cb[i][j] = cb - 128.0;      // FIX: Now cb has +128 from rgb_to_ycbcr
                    ycbcr_cr[i][j] = cr - 128.0;
                }
            }
            
            // DCT
            double dct_y[8][8], dct_cb[8][8], dct_cr[8][8];
            perform_dct(ycbcr_y, dct_y);
            perform_dct(ycbcr_cb, dct_cb);
            perform_dct(ycbcr_cr, dct_cr);
            
            // Quantization (2D result)
            short q_y[8][8], q_cb[8][8], q_cr[8][8];
            for (int u = 0; u < 8; u++) {
                for (int v = 0; v < 8; v++) {
                    q_y[u][v] = (short)round(dct_y[u][v] / std_qtable_Y[u][v]);
                    q_cb[u][v] = (short)round(dct_cb[u][v] / std_qtable_C[u][v]);
                    q_cr[u][v] = (short)round(dct_cr[u][v] / std_qtable_C[u][v]);
                }
            }
            
            // Convert to zig-zag order (1D array)
            short zz_q_y[64], zz_q_cb[64], zz_q_cr[64];
            for (int i = 0; i < 64; i++) {
                int zz_pos = zigzag_order[i];  // Position in 2D
                int u = zz_pos / 8;
                int v = zz_pos % 8;
                zz_q_y[i] = q_y[u][v];
                zz_q_cb[i] = q_cb[u][v];
                zz_q_cr[i] = q_cr[u][v];
            }
            
            // Write quantized coefficients
            fwrite(zz_q_y, sizeof(short), 64, fqfy);
            fwrite(zz_q_cb, sizeof(short), 64, fqfcb);
            fwrite(zz_q_cr, sizeof(short), 64, fqfcr);
            
            // Write unquantized (for error analysis)
            fwrite(zz_q_y, sizeof(short), 64, fefy);
            fwrite(zz_q_cb, sizeof(short), 64, fefcb);
            fwrite(zz_q_cr, sizeof(short), 64, fefcr);
        }
    }
    
    fclose(fqfy); 
    fclose(fqfcb); 
    fclose(fqfcr);
    fclose(fefy); 
    fclose(fefcb); 
    fclose(fefcr);
    
    for (int i = 0; i < height; i++) free(pixels[i]);
    free(pixels);
    
    printf("Method 1 Encoder Complete\n");
    return 0;
}

// Method 3: DPCM + RLE Entropy Coding
int method_3_encoder(int argc, char *argv[]) {
    if (argc < 10) {
        fprintf(stderr, "Usage: encoder 3 <bmp> <DC_Y> <DC_Cb> <DC_Cr> <AC_Y> <AC_Cb> <AC_Cr> <dim>\n");
        return 1;
    }
    
    int width, height;
    Pixel **pixels = read_bmp(argv[2], &width, &height);
    if (!pixels) return 1;
    
    FILE *fdc_y = fopen(argv[3], "w");
    FILE *fdc_cb = fopen(argv[4], "w");
    FILE *fdc_cr = fopen(argv[5], "w");
    FILE *fac_y = fopen(argv[6], "w");
    FILE *fac_cb = fopen(argv[7], "w");
    FILE *fac_cr = fopen(argv[8], "w");
    
    if (!fdc_y || !fdc_cb || !fdc_cr || !fac_y || !fac_cb || !fac_cr) {
        fprintf(stderr, "Error opening entropy coding output files\n");
        return 1;
    }
    
    // Output dimensions
    FILE *fdim = fopen(argv[9], "w");
    fprintf(fdim, "%d %d\n", width, height);
    fclose(fdim);
    
    int last_dc_y = 0, last_dc_cb = 0, last_dc_cr = 0;
    
    // Process each 8x8 block
    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            double ycbcr_y[8][8], ycbcr_cb[8][8], ycbcr_cr[8][8];
            
            // Convert to YCbCr and level shift
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    int py = by + i;
                    int px = bx + j;
                    
                    if (py >= height) py = height - 1;
                    if (px >= width) px = width - 1;
                    
                    double y, cb, cr;
                    rgb_to_ycbcr(pixels[py][px], &y, &cb, &cr);
                    
                    ycbcr_y[i][j] = y - 128.0;
                    ycbcr_cb[i][j] = cb - 128.0;
                    ycbcr_cr[i][j] = cr - 128.0;
                }
            }
            
            // DCT
            double dct_y[8][8], dct_cb[8][8], dct_cr[8][8];
            perform_dct(ycbcr_y, dct_y);
            perform_dct(ycbcr_cb, dct_cb);
            perform_dct(ycbcr_cr, dct_cr);
            
            // Quantization (2D result)
            short q_y[8][8], q_cb[8][8], q_cr[8][8];
            for (int u = 0; u < 8; u++) {
                for (int v = 0; v < 8; v++) {
                    q_y[u][v] = (short)round(dct_y[u][v] / std_qtable_Y[u][v]);
                    q_cb[u][v] = (short)round(dct_cb[u][v] / std_qtable_C[u][v]);
                    q_cr[u][v] = (short)round(dct_cr[u][v] / std_qtable_C[u][v]);
                }
            }
            
            // DC DPCM
            short dc_diff_y = q_y[0][0] - last_dc_y;
            short dc_diff_cb = q_cb[0][0] - last_dc_cb;
            short dc_diff_cr = q_cr[0][0] - last_dc_cr;
            
            fprintf(fdc_y, "%d ", dc_diff_y);
            fprintf(fdc_cb, "%d ", dc_diff_cb);
            fprintf(fdc_cr, "%d ", dc_diff_cr);
            
            last_dc_y = q_y[0][0];
            last_dc_cb = q_cb[0][0];
            last_dc_cr = q_cr[0][0];
            
            // Convert to zig-zag order
            short zz_q_y[64], zz_q_cb[64], zz_q_cr[64];
            for (int i = 0; i < 64; i++) {
                int zz_pos = zigzag_order[i];
                int u = zz_pos / 8;
                int v = zz_pos % 8;
                zz_q_y[i] = q_y[u][v];
                zz_q_cb[i] = q_cb[u][v];
                zz_q_cr[i] = q_cr[u][v];
            }
            
            // AC RLE (skip DC which is at position 0)
            FILE *fac_files[3] = {fac_y, fac_cb, fac_cr};
            short *zz_arrays[3] = {zz_q_y, zz_q_cb, zz_q_cr};
            
            for (int ch = 0; ch < 3; ch++) {
                int run_length = 0;
                for (int i = 1; i < 64; i++) {
                    if (zz_arrays[ch][i] == 0) {
                        run_length++;
                    } else {
                        while (run_length > 15) {
                            fprintf(fac_files[ch], "(15,0) ");
                            run_length -= 16;
                        }
                        fprintf(fac_files[ch], "(%d,%d) ", run_length, zz_arrays[ch][i]);
                        run_length = 0;
                    }
                }
                // EOB
                fprintf(fac_files[ch], "(0,0) ");
            }
            
            fprintf(fac_y, "\n");
            fprintf(fac_cb, "\n");
            fprintf(fac_cr, "\n");
        }
    }
    
    fclose(fdc_y); 
    fclose(fdc_cb); 
    fclose(fdc_cr);
    fclose(fac_y); 
    fclose(fac_cb); 
    fclose(fac_cr);
    
    for (int i = 0; i < height; i++) free(pixels[i]);
    free(pixels);
    
    printf("Method 3 Encoder Complete\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ./encoder <method> ...\n");
        return 1;
    }
    
    int method = atoi(argv[1]);
    
    switch (method) {
        case 0:
            return method_0_encoder(argc, argv);
        case 1:
            return method_1_encoder(argc, argv);
        case 3:
            return method_3_encoder(argc, argv);
        default:
            fprintf(stderr, "Unknown method: %d\n", method);
            return 1;
    }
}