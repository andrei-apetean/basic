#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* fixed cozette glyph dimensions */
#define GLYPH_W 6
#define GLYPH_H 13
#define IPB_SZ 256

#define FIRST_CODEPOINT 32
#define LAST_CODEPOINT  126
#define GLYPH_COUNT (LAST_CODEPOINT - FIRST_CODEPOINT + 1)
#define ATLAS_COLS 15
#define ATLAS_ROWS ((GLYPH_COUNT + ATLAS_COLS - 1) / ATLAS_COLS)
#define ATLAS_W (ATLAS_COLS * GLYPH_W)
#define ATLAS_H (ATLAS_ROWS * GLYPH_H)

static uint8_t atlas[ATLAS_H][ATLAS_W][4];

static void blit_glyph(int codepoint, uint8_t rows[GLYPH_H], int bbx_h, int bbx_yoff)
{
    int idx = codepoint - FIRST_CODEPOINT;
    if (idx < 0 || idx >= GLYPH_COUNT) return;

    int col    = idx % ATLAS_COLS;
    int row    = idx / ATLAS_COLS;
    int ox     = col * GLYPH_W;
    int oy     = row * GLYPH_H;

    // baseline is at GLYPH_H - 3 from top (Cozette has 3px descent)
    // glyph top = baseline - bbx_yoff - bbx_h
    int start_y = (GLYPH_H - 3) - bbx_yoff - bbx_h;

    for (int y = 0; y < bbx_h; y++) {
        int dest_y = oy + start_y + y;
        if (dest_y < oy || dest_y >= oy + GLYPH_H) continue;
        uint8_t byte = rows[y];
        for (int x = 0; x < GLYPH_W; x++) {
            uint8_t lit = (byte >> (7 - x)) & 1;
            atlas[dest_y][ox + x][0] = 255;
            atlas[dest_y][ox + x][1] = 255;
            atlas[dest_y][ox + x][2] = 255;
            atlas[dest_y][ox + x][3] = lit ? 255 : 0;
        }
    }
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        printf("invalid arguments\n");
        printf("usage: %s <input-file> <output-file>\n", argv[0]);
        return -1;
    }
    FILE* cozette_bdf = 0;
    FILE* cozette_bitmap = 0;
    cozette_bdf = fopen(argv[1], "rb");
    if (!cozette_bdf) {
        printf("failed to open file %s\n", argv[1]);
        goto error;
    }
    cozette_bitmap = fopen(argv[2], "wb");
    if (!cozette_bitmap) {
        printf("failed to open file %s\n", argv[2]);
        goto error;
    }
    printf("code points: %d (%d -> %d)\n", GLYPH_COUNT,
            FIRST_CODEPOINT, LAST_CODEPOINT);
    printf("atlas: %d x %d (%d x %d px)\n", ATLAS_COLS, ATLAS_ROWS,
            ATLAS_COLS * GLYPH_W, ATLAS_ROWS * GLYPH_H);

    char line[IPB_SZ] = { 0 };
    int in_bitmap = 0;
    int bitmap_row = 0;
    int bbx_yoff = 0;
    int bbx_h = GLYPH_H;

    uint8_t bitmap[GLYPH_H] = { 0 };

    int current_codepoint = 0;
    while(fgets(line, sizeof(line), cozette_bdf)) {
        if (strncmp(line, "ENCODING", 8) == 0) {
            current_codepoint = atoi(line + 9);
            in_bitmap = 0;
        } else if (strncmp(line, "BBX", 3) == 0) {
            sscanf(line, "BBX %*d %d %*d %d", &bbx_h, &bbx_yoff);
        } else if (strncmp(line, "BITMAP", 6) == 0) {
            in_bitmap = 1;
            bitmap_row = 0;
            memset(bitmap, 0, sizeof(bitmap));
        } else if (strncmp(line, "ENDCHAR", 7) == 0) {
            if (current_codepoint <= LAST_CODEPOINT
                    && current_codepoint >= FIRST_CODEPOINT) {
                blit_glyph(current_codepoint, bitmap, bbx_h, bbx_yoff);
            }
            bbx_yoff = 0;
            bbx_h = GLYPH_H;
            in_bitmap = 0;
        } else if (in_bitmap && bitmap_row < GLYPH_H) {
            bitmap[bitmap_row++] = (uint8_t)strtol(line, NULL, 16);
        }
    }

    for (int row = 0; row < ATLAS_H; row++) {
        for (int col = 0; col < ATLAS_W; col++) {
            uint8_t lit = atlas[row][col][3];
            printf("%c", lit ? '#' : ' ');
        }
        printf("\n");
    }

    fprintf(cozette_bitmap, "#pragma once\n");
    fprintf(cozette_bitmap, "#include <stdint.h>\n\n");
    fprintf(cozette_bitmap, "#define COZETTE_ATLAS_W %d\n", ATLAS_W);
    fprintf(cozette_bitmap, "#define COZETTE_ATLAS_H %d\n", ATLAS_H);
    fprintf(cozette_bitmap, "#define COZETTE_GLYPH_W %d\n", GLYPH_W);
    fprintf(cozette_bitmap, "#define COZETTE_GLYPH_H %d\n\n", GLYPH_H);
    fprintf(cozette_bitmap,
            "\n"
            "#define COZETTE_ATLAS_COLS %d\n"
            "#define COZETTE_FIRST_CODEPOINT %d\n"
            "#define COZETTE_LAST_CODEPOINT  %d\n"
            "\n"
            "static inline void cozette_glyph_uv(int codepoint,\n"
            "        float *u0, float *v0, float *u1, float *v1)\n"
            "{\n"
            "    if (codepoint < COZETTE_FIRST_CODEPOINT ||\n"
            "        codepoint > COZETTE_LAST_CODEPOINT)\n"
            "        codepoint = '?';\n"
            "    int idx = codepoint - COZETTE_FIRST_CODEPOINT;\n"
            "    int col = idx %% COZETTE_ATLAS_COLS;\n"
            "    int row = idx / COZETTE_ATLAS_COLS;\n"
            "    *u0 = (float)(col * COZETTE_GLYPH_W) / (float)COZETTE_ATLAS_W;\n"
            "    *v0 = (float)(row * COZETTE_GLYPH_H) / (float)COZETTE_ATLAS_H;\n"
            "    *u1 = (float)(col * COZETTE_GLYPH_W + COZETTE_GLYPH_W)"
            "/ (float)COZETTE_ATLAS_W;\n"
            "    *v1 = (float)(row * COZETTE_GLYPH_H + COZETTE_GLYPH_H)"
            "/ (float)COZETTE_ATLAS_H;\n"
            "}\n",
            ATLAS_COLS, FIRST_CODEPOINT, LAST_CODEPOINT);

    fprintf(cozette_bitmap, "static const uint8_t cozette_atlas[] = {\n");

    for (int row = 0; row < ATLAS_H; row++) {
        for (int col = 0; col < ATLAS_W; col++) {
            fprintf(cozette_bitmap, "0x%02x,0x%02x,0x%02x,0x%02x,",
                    atlas[row][col][0], atlas[row][col][1],
                    atlas[row][col][2], atlas[row][col][3]);
        }
        fprintf(cozette_bitmap, "\n");
    }
    fprintf(cozette_bitmap, "};\n");

    if (cozette_bdf) fclose(cozette_bdf);
    if (cozette_bitmap) fclose(cozette_bitmap);

    return 0;

error:
    if (cozette_bdf) fclose(cozette_bdf);
    if (cozette_bitmap) fclose(cozette_bitmap);
}
