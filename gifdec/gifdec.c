/* gifdec - public domain GIF decoder
 * Based on https://github.com/lecram/gifdec
 * Simplified for sf2ww background usage */

#include "gifdec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GD_MIN(A, B) ((A) < (B) ? (A) : (B))
#define GD_MAX(A, B) ((A) > (B) ? (A) : (B))

typedef struct Entry {
    uint16_t length;
    uint16_t prefix;
    uint8_t  suffix;
} Entry;

typedef struct Table {
    int bulk;
    int nentries;
    Entry *entries;
} Table;

static int interlace_row(int h, int y);

static uint16_t
read_num(FILE *fd)
{
    uint8_t bytes[2];
    fread(bytes, 2, 1, fd);
    return bytes[0] + (((uint16_t)bytes[1]) << 8);
}

gd_GIF *
gd_open_gif(const char *fname)
{
    FILE *fd;
    uint8_t sigver[3];
    uint16_t width, height;
    int depth;
    uint8_t fdsz, bgidx, aspect;
    int i;
    uint8_t *bgcolor;
    int gct_sz;
    gd_GIF *gif;

    fd = fopen(fname, "rb");
    if (!fd)
        return NULL;

    fread(sigver, 3, 1, fd);
    if (memcmp(sigver, "GIF", 3) != 0) {
        fprintf(stderr, "gifdec: invalid signature\n");
        goto fail;
    }
    fread(sigver, 3, 1, fd);
    if (memcmp(sigver, "89a", 3) != 0 && memcmp(sigver, "87a", 3) != 0) {
        fprintf(stderr, "gifdec: invalid version\n");
        goto fail;
    }

    width  = read_num(fd);
    height = read_num(fd);
    fread(&fdsz, 1, 1, fd);

    if (!(fdsz & 0x80)) {
        fprintf(stderr, "gifdec: no global color table\n");
        goto fail;
    }

    depth = ((fdsz >> 4) & 7) + 1;
    gct_sz = 1 << ((fdsz & 0x07) + 1);

    fread(&bgidx, 1, 1, fd);
    fread(&aspect, 1, 1, fd);

    gif = calloc(1, sizeof(*gif));
    if (!gif) goto fail;

    gif->fd = fd;
    gif->width  = width;
    gif->height = height;
    gif->depth  = depth;
    gif->bgindex = bgidx;
    gif->loop_count = 0;

    gif->gct.size = gct_sz;
    fread(gif->gct.colors, 3, gct_sz, fd);
    gif->palette = &gif->gct;

    gif->canvas = calloc(1, (size_t)width * height);
    gif->frame  = calloc(1, (size_t)width * height);
    if (!gif->canvas || !gif->frame) {
        free(gif->canvas);
        free(gif->frame);
        free(gif);
        goto fail;
    }

    memset(gif->canvas, bgidx, (size_t)width * height);

    gif->gce.delay = 0;
    gif->gce.tindex = 0;
    gif->gce.disposal = 0;
    gif->gce.input = 0;
    gif->gce.transparency = 0;

    return gif;
fail:
    fclose(fd);
    return NULL;
}

static void
discard_sub_blocks(gd_GIF *gif)
{
    uint8_t size;
    do {
        fread(&size, 1, 1, gif->fd);
        fseek(gif->fd, size, SEEK_CUR);
    } while (size);
}

static void
read_graphic_control_ext(gd_GIF *gif)
{
    uint8_t rdit;
    uint8_t block_size;
    uint8_t term;

    fread(&block_size, 1, 1, gif->fd);
    fread(&rdit, 1, 1, gif->fd);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif->fd);
    fread(&gif->gce.tindex, 1, 1, gif->fd);
    fread(&term, 1, 1, gif->fd);
}

static void
read_application_ext(gd_GIF *gif)
{
    char app_id[8];
    char app_auth[3];
    uint8_t sub_block;

    fread(&sub_block, 1, 1, gif->fd);
    if (sub_block != 11) {
        fseek(gif->fd, sub_block, SEEK_CUR);
        discard_sub_blocks(gif);
        return;
    }
    fread(app_id, 8, 1, gif->fd);
    fread(app_auth, 3, 1, gif->fd);
    if (!strncmp(app_id, "NETSCAPE", 8)) {
        fread(&sub_block, 1, 1, gif->fd);
        uint8_t loop_sub;
        fread(&loop_sub, 1, 1, gif->fd);
        gif->loop_count = read_num(gif->fd);
        uint8_t term;
        fread(&term, 1, 1, gif->fd);
    } else {
        discard_sub_blocks(gif);
    }
}

static void
read_ext(gd_GIF *gif)
{
    uint8_t label;
    fread(&label, 1, 1, gif->fd);
    switch (label) {
    case 0x01: /* Plain Text */
    {
        uint8_t sub_block;
        fread(&sub_block, 1, 1, gif->fd);
        fseek(gif->fd, sub_block, SEEK_CUR);
        discard_sub_blocks(gif);
        break;
    }
    case 0xF9:
        read_graphic_control_ext(gif);
        break;
    case 0xFE: /* Comment */
        discard_sub_blocks(gif);
        break;
    case 0xFF:
        read_application_ext(gif);
        break;
    default:
        fprintf(stderr, "gifdec: unknown extension: %02X\n", label);
    }
}

static Table *
new_table(int key_size)
{
    int key;
    int init_bulk = GD_MAX(1 << (key_size + 1), 0x100);
    Table *table = malloc(sizeof(*table) + sizeof(Entry) * init_bulk);
    if (table) {
        table->bulk = init_bulk;
        table->nentries = (1 << key_size) + 2;
        table->entries = (Entry *) &table[1];
        for (key = 0; key < (1 << key_size); key++) {
            table->entries[key] = (Entry) {1, 0xFFF, (uint8_t)key};
        }
    }
    return table;
}

static uint16_t
get_key(gd_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key;

    key = 0;
    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            if (*sub_len == 0) {
                fread(sub_len, 1, 1, gif->fd);
                if (*sub_len == 0)
                    return 0x1000;
            }
            fread(byte, 1, 1, gif->fd);
            (*sub_len)--;
        }
        frag_size = GD_MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
    }
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}

static int
read_image_data(gd_GIF *gif, int interlace)
{
    uint8_t sub_len, shift, byte;
    int init_key_size;
    int key_size, table_is_full;
    int frm_off, frm_size, str_len, i, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    Table *table;
    Entry entry;

    fread(&byte, 1, 1, gif->fd);
    init_key_size = (int) byte;
    if (init_key_size < 2 || init_key_size > 8)
        return -1;

    clear = 1 << init_key_size;
    stop  = clear + 1;
    table = new_table(init_key_size);
    if (!table) return -1;

    key_size = init_key_size + 1;
    table_is_full = 0;
    sub_len = shift = 0;
    str_len = 0;
    memset(&entry, 0, sizeof(entry));

    key = get_key(gif, key_size, &sub_len, &shift, &byte);
    frm_off = 0;
    ret = 0;
    frm_size = gif->fw * gif->fh;

    while (frm_off < frm_size) {
        if (key == clear) {
            key_size = init_key_size + 1;
            table_is_full = 0;
            table->nentries = (1 << init_key_size) + 2;
        } else if (!table_is_full) {
            ret = 0;
            if (table->nentries == table->bulk) {
                table->bulk *= 2;
                Table *newtable = realloc(table, sizeof(*table) + sizeof(Entry) * table->bulk);
                if (!newtable) {
                    free(table);
                    return -1;
                }
                table = newtable;
                table->entries = (Entry *) &table[1];
            }
            table->entries[table->nentries] = (Entry) {(uint16_t)(str_len + 1), key, entry.suffix};
            table->nentries++;
            if ((table->nentries & (table->nentries - 1)) == 0)
                ret = 1;
            if (table->nentries == 0x1000) {
                ret = 0;
                table_is_full = 1;
            }
        }
        key = get_key(gif, key_size, &sub_len, &shift, &byte);
        if (key == clear) continue;
        if (key == stop || key == 0x1000) break;
        if (ret == 1) key_size++;
        entry = table->entries[key];
        str_len = entry.length;
        for (i = 0; i < str_len; i++) {
            p = frm_off + entry.length - 1;
            x = p % gif->fw;
            y = p / gif->fw;
            if (interlace)
                y = interlace_row(gif->fh, y);
            if (x < gif->fw && y < gif->fh) {
                int dst = (gif->fy + y) * gif->width + gif->fx + x;
                if (dst >= 0 && dst < gif->width * gif->height)
                    gif->frame[dst] = entry.suffix;
            }
            if (entry.prefix == 0xFFF)
                break;
            else
                entry = table->entries[entry.prefix];
        }
        frm_off += str_len;
        if (key < table->nentries - 1 && !table_is_full)
            table->entries[table->nentries - 1].suffix = entry.suffix;
    }
    free(table);

    if (sub_len)
        fseek(gif->fd, sub_len, SEEK_CUR);

    /* consume remaining sub-blocks */
    fread(&sub_len, 1, 1, gif->fd);
    while (sub_len) {
        fseek(gif->fd, sub_len, SEEK_CUR);
        fread(&sub_len, 1, 1, gif->fd);
    }
    return 0;
}

static int
interlace_row(int h, int y)
{
    int p;
    p = (h - 1) / 8 + 1;
    if (y < p) return y * 8;
    y -= p;
    p = (h - 5) / 8 + 1;
    if (p <= 0) p = 1;
    if (y < p) return y * 8 + 4;
    y -= p;
    p = (h - 3) / 4 + 1;
    if (p <= 0) p = 1;
    if (y < p) return y * 4 + 2;
    y -= p;
    return y * 2 + 1;
}

int
gd_get_frame(gd_GIF *gif)
{
    char sep;
    uint8_t fdsz;
    int interlace;
    int lct_sz;

    if (gif->gce.disposal == 2) {
        int x, y;
        for (y = gif->fy; y < gif->fy + gif->fh && y < gif->height; y++) {
            for (x = gif->fx; x < gif->fx + gif->fw && x < gif->width; x++) {
                gif->canvas[y * gif->width + x] = gif->bgindex;
            }
        }
    }

    for (;;) {
        if (fread(&sep, 1, 1, gif->fd) < 1)
            return 0;
        if (sep == 0x3B)
            return 0;
        if (sep == 0x21)
            read_ext(gif);
        else if (sep == 0x2C)
            break;
    }

    gif->fx = read_num(gif->fd);
    gif->fy = read_num(gif->fd);
    gif->fw = read_num(gif->fd);
    gif->fh = read_num(gif->fd);
    fread(&fdsz, 1, 1, gif->fd);
    interlace = fdsz & 0x40;

    if (fdsz & 0x80) {
        lct_sz = 1 << ((fdsz & 0x07) + 1);
        gif->lct.size = lct_sz;
        fread(gif->lct.colors, 3, lct_sz, gif->fd);
        gif->palette = &gif->lct;
    } else {
        gif->palette = &gif->gct;
    }

    return read_image_data(gif, interlace) == 0 ? 1 : -1;
}

void
gd_render_frame(gd_GIF *gif, uint8_t *buffer)
{
    int x, y;
    uint8_t index;
    uint8_t *color;

    for (y = gif->fy; y < gif->fy + gif->fh && y < gif->height; y++) {
        for (x = gif->fx; x < gif->fx + gif->fw && x < gif->width; x++) {
            int p = y * gif->width + x;
            index = gif->frame[p];
            if (gif->gce.transparency && index == gif->gce.tindex) {
                color = &gif->canvas[p];
                /* canvas stores indices too, so look up color */
                uint8_t ci = *color;
                uint8_t *rgb = &gif->palette->colors[ci * 3];
                buffer[p * 4 + 0] = rgb[0];
                buffer[p * 4 + 1] = rgb[1];
                buffer[p * 4 + 2] = rgb[2];
                buffer[p * 4 + 3] = 0xFF;
            } else {
                color = &gif->palette->colors[index * 3];
                gif->canvas[p] = index;
                buffer[p * 4 + 0] = color[0];
                buffer[p * 4 + 1] = color[1];
                buffer[p * 4 + 2] = color[2];
                buffer[p * 4 + 3] = 0xFF;
            }
        }
    }
}

int
gd_is_bgcolor(gd_GIF *gif, uint8_t color[3])
{
    return color[0] == gif->palette->colors[gif->bgindex*3]
        && color[1] == gif->palette->colors[gif->bgindex*3 + 1]
        && color[2] == gif->palette->colors[gif->bgindex*3 + 2];
}

void
gd_rewind(gd_GIF *gif)
{
    fseek(gif->fd, 13 + gif->gct.size * 3, SEEK_SET);
}

void
gd_close_gif(gd_GIF *gif)
{
    fclose(gif->fd);
    free(gif->frame);
    free(gif->canvas);
    free(gif);
}
