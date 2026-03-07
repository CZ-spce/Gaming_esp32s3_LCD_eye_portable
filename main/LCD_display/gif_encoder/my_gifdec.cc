#include "my_gifdec.h" // 确保引用正确
#include "esp_heap_caps.h" 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

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

// --- 【新增】内存读取辅助函数 ---
static void mem_read(gd_GIF *gif, void *buf, size_t len) {
    if (gif->f_pos + len > gif->size) {
        len = gif->size - gif->f_pos; // 防止越界
    }
    memcpy(buf, gif->data + gif->f_pos, len);
    gif->f_pos += len;
}

static uint16_t read_num(gd_GIF *gif) {
    uint8_t bytes[2];
    mem_read(gif, bytes, 2);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}

// --- 【修改】打开 GIF (从内存) ---
gd_GIF *my_gd_open_gif_buffer(const uint8_t *buffer, size_t size) {
    uint8_t sigver[3];
    uint16_t width, height, depth;
    uint8_t fdsz, bgidx, aspect;
    int i;
    uint8_t *bgcolor;
    int gct_sz;
    gd_GIF *gif;

    // 分配结构体
    gif = (gd_GIF*)calloc(1, sizeof(*gif));
    if (!gif) return NULL;
    
    // 初始化内存流
    gif->data = buffer;
    gif->size = size;
    gif->f_pos = 0;

    /* Header */
    mem_read(gif, sigver, 3);
    if (memcmp(sigver, "GIF", 3) != 0) goto fail;
    /* Version */
    mem_read(gif, sigver, 3);
    if (memcmp(sigver, "89a", 3) != 0) goto fail;
    /* Width x Height */
    width  = read_num(gif);
    height = read_num(gif);
    /* FDSZ */
    mem_read(gif, &fdsz, 1);
    /* Presence of GCT */
    if (!(fdsz & 0x80)) goto fail;
    /* Color Space's Depth */
    depth = ((fdsz >> 4) & 7) + 1;
    /* GCT Size */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    /* Background Color Index */
    mem_read(gif, &bgidx, 1);
    /* Aspect Ratio */
    mem_read(gif, &aspect, 1);
    
    gif->width  = width;
    gif->height = height;
    gif->depth  = depth;
    /* Read GCT */
    gif->gct.size = gct_sz;
    mem_read(gif, gif->gct.colors, 3 * gif->gct.size);
    gif->palette = &gif->gct;
    gif->bgindex = bgidx;
    
    // PSRAM 分配 Frame
    gif->frame = (uint8_t*)heap_caps_calloc(4, width * height, MALLOC_CAP_SPIRAM);
    if (!gif->frame) {
        free(gif);
        return NULL;
    }
    gif->canvas = &gif->frame[width * height];
    if (gif->bgindex)
        memset(gif->frame, gif->bgindex, gif->width * gif->height);
    bgcolor = &gif->palette->colors[gif->bgindex*3];
    if (bgcolor[0] || bgcolor[1] || bgcolor [2])
        for (i = 0; i < gif->width * gif->height; i++)
            memcpy(&gif->canvas[i*3], bgcolor, 3);
            
    gif->anim_start = gif->f_pos; // 记录当前位置
    return gif;
fail:
    free(gif);
    return NULL;
}

static void discard_sub_blocks(gd_GIF *gif) {
    uint8_t size;
    do {
        mem_read(gif, &size, 1);
        gif->f_pos += size; // 相当于 lseek seek_cur
    } while (size);
}

static void read_plain_text_ext(gd_GIF *gif) {
    if (gif->plain_text) {
        uint16_t tx, ty, tw, th;
        uint8_t cw, ch, fg, bg;
        size_t sub_block_pos; 
        
        gif->f_pos += 1; 
        tx = read_num(gif);
        ty = read_num(gif);
        tw = read_num(gif);
        th = read_num(gif);
        mem_read(gif, &cw, 1);
        mem_read(gif, &ch, 1);
        mem_read(gif, &fg, 1);
        mem_read(gif, &bg, 1);
        
        sub_block_pos = gif->f_pos;
        gif->plain_text(gif, tx, ty, tw, th, cw, ch, fg, bg);
        gif->f_pos = sub_block_pos; 
    } else {
        gif->f_pos += 13;
    }
    discard_sub_blocks(gif);
}

static void read_graphic_control_ext(gd_GIF *gif) {
    uint8_t rdit;
    gif->f_pos += 1; 
    mem_read(gif, &rdit, 1);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif);
    mem_read(gif, &gif->gce.tindex, 1);
    gif->f_pos += 1; 
}

static void read_comment_ext(gd_GIF *gif) {
    if (gif->comment) {
        size_t sub_block_pos = gif->f_pos;
        gif->comment(gif);
        gif->f_pos = sub_block_pos;
    }
    discard_sub_blocks(gif);
}

static void read_application_ext(gd_GIF *gif) {
    char app_id[8];
    char app_auth_code[3];
    gif->f_pos += 1;
    mem_read(gif, app_id, 8);
    mem_read(gif, app_auth_code, 3);
    if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
        gif->f_pos += 2;
        gif->loop_count = read_num(gif);
        gif->f_pos += 1;
    } else if (gif->application) {
        size_t sub_block_pos = gif->f_pos;
        gif->application(gif, app_id, app_auth_code);
        gif->f_pos = sub_block_pos;
        discard_sub_blocks(gif);
    } else {
        discard_sub_blocks(gif);
    }
}

static void read_ext(gd_GIF *gif) {
    uint8_t label;
    mem_read(gif, &label, 1);
    switch (label) {
    case 0x01: read_plain_text_ext(gif); break;
    case 0xF9: read_graphic_control_ext(gif); break;
    case 0xFE: read_comment_ext(gif); break;
    case 0xFF: read_application_ext(gif); break;
    default:   break;
    }
}

static Table *new_table(int key_size) {
    int key;
    int init_bulk = MAX(1 << (key_size + 1), 0x100);
    Table *table = (Table*)malloc(sizeof(*table) + sizeof(Entry) * init_bulk);
    if (table) {
        table->bulk = init_bulk;
        table->nentries = (1 << key_size) + 2;
        table->entries = (Entry *) &table[1];
        for (key = 0; key < (1 << key_size); key++)
            table->entries[key] = (Entry) {1, 0xFFF, (uint8_t)key};
    }
    return table;
}

static int add_entry(Table **tablep, uint16_t length, uint16_t prefix, uint8_t suffix) {
    Table *table = *tablep;
    if (table->nentries == table->bulk) {
        table->bulk *= 2;
        table = (Table*)realloc(table, sizeof(*table) + sizeof(Entry) * table->bulk);
        if (!table) return -1;
        table->entries = (Entry *) &table[1];
        *tablep = table;
    }
    table->entries[table->nentries] = (Entry) {length, prefix, suffix};
    table->nentries++;
    if ((table->nentries & (table->nentries - 1)) == 0) return 1;
    return 0;
}

static uint16_t get_key(gd_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte) {
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key;
    key = 0;
    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            if (*sub_len == 0) {
                mem_read(gif, sub_len, 1);
                if (*sub_len == 0) return 0x1000;
            }
            mem_read(gif, byte, 1);
            (*sub_len)--;
        }
        frag_size = MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
    }
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}

static int interlaced_line_index(int h, int y) {
    int p = (h - 1) / 8 + 1;
    if (y < p) return y * 8;
    y -= p;
    p = (h - 5) / 8 + 1;
    if (y < p) return y * 8 + 4;
    y -= p;
    p = (h - 3) / 4 + 1;
    if (y < p) return y * 4 + 2;
    y -= p;
    return y * 2 + 1;
}

static int read_image_data(gd_GIF *gif, int interlace) {
    uint8_t sub_len, shift, byte;
    int init_key_size, key_size, table_is_full = 0;
    int frm_off, frm_size, str_len = 0, i, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    Table *table;
    Entry entry = {0, 0, 0};
    size_t start, end;

    mem_read(gif, &byte, 1);
    key_size = (int) byte;
    if (key_size < 2 || key_size > 8) return -1;
    
    start = gif->f_pos;
    discard_sub_blocks(gif);
    end = gif->f_pos;
    gif->f_pos = start; 
    
    clear = 1 << key_size;
    stop = clear + 1;
    table = new_table(key_size);
    key_size++;
    init_key_size = key_size;
    sub_len = shift = 0;
    key = get_key(gif, key_size, &sub_len, &shift, &byte); 
    frm_off = 0;
    ret = 0;
    frm_size = gif->fw*gif->fh;
    while (frm_off < frm_size) {
        if (key == clear) {
            key_size = init_key_size;
            table->nentries = (1 << (key_size - 1)) + 2;
            table_is_full = 0;
        } else if (!table_is_full) {
            ret = add_entry(&table, str_len + 1, key, entry.suffix);
            if (ret == -1) {
                free(table);
                return -1;
            }
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
                y = interlaced_line_index((int) gif->fh, y);
            gif->frame[(gif->fy + y) * gif->width + gif->fx + x] = entry.suffix;
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
    if (key == stop)
        mem_read(gif, &sub_len, 1); 
    gif->f_pos = end; 
    return 0;
}

static int read_image(gd_GIF *gif) {
    uint8_t fisrz;
    int interlace;
    gif->fx = read_num(gif);
    gif->fy = read_num(gif);
    if (gif->fx >= gif->width || gif->fy >= gif->height) return -1;
    gif->fw = read_num(gif);
    gif->fh = read_num(gif);
    gif->fw = MIN(gif->fw, gif->width - gif->fx);
    gif->fh = MIN(gif->fh, gif->height - gif->fy);
    mem_read(gif, &fisrz, 1);
    interlace = fisrz & 0x40;
    if (fisrz & 0x80) {
        gif->lct.size = 1 << ((fisrz & 0x07) + 1);
        mem_read(gif, gif->lct.colors, 3 * gif->lct.size);
        gif->palette = &gif->lct;
    } else
        gif->palette = &gif->gct;
    return read_image_data(gif, interlace);
}

static void render_frame_rect(gd_GIF *gif, uint8_t *buffer) {
    int i, j, k;
    uint8_t index, *color;
    i = gif->fy * gif->width + gif->fx;
    for (j = 0; j < gif->fh; j++) {
        for (k = 0; k < gif->fw; k++) {
            index = gif->frame[(gif->fy + j) * gif->width + gif->fx + k];
            color = &gif->palette->colors[index*3];
            if (!gif->gce.transparency || index != gif->gce.tindex)
                memcpy(&buffer[(i+k)*3], color, 3);
        }
        i += gif->width;
    }
}

static void dispose(gd_GIF *gif) {
    int i, j, k;
    uint8_t *bgcolor;
    switch (gif->gce.disposal) {
    case 2: 
        bgcolor = &gif->palette->colors[gif->bgindex*3];
        i = gif->fy * gif->width + gif->fx;
        for (j = 0; j < gif->fh; j++) {
            for (k = 0; k < gif->fw; k++)
                memcpy(&gif->canvas[(i+k)*3], bgcolor, 3);
            i += gif->width;
        }
        break;
    case 3: break;
    default:
        render_frame_rect(gif, gif->canvas);
    }
}

int my_gd_get_frame(gd_GIF *gif) {
    char sep;
    dispose(gif);
    mem_read(gif, &sep, 1);
    while (sep != ',') {
        if (sep == ';') return 0;
        if (sep == '!') read_ext(gif);
        else return -1;
        mem_read(gif, &sep, 1);
    }
    if (read_image(gif) == -1) return -1;
    return 1;
}

void my_gd_render_frame(gd_GIF *gif, uint8_t *buffer) {
    memcpy(buffer, gif->canvas, gif->width * gif->height * 3);
    render_frame_rect(gif, buffer);
}

int my_gd_is_bgcolor(gd_GIF *gif, uint8_t color[3]) {
    return !memcmp(&gif->palette->colors[gif->bgindex*3], color, 3);
}

void my_gd_rewind(gd_GIF *gif) {
    gif->f_pos = gif->anim_start;
}

void my_gd_close_gif(gd_GIF *gif) {
    heap_caps_free(gif->frame);    
    free(gif);
}
