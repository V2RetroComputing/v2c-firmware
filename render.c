#include <pico/stdlib.h>
#include "render.h"
#include "vgaout.h"
#include "iic.h"
#include "hires_color_patterns.h"
#include "v2logo.h"

extern bool logo_screen;
extern uint16_t logo_frame;

extern uint8_t menu_screen;
extern uint8_t render_mode;

uint8_t __attribute__((section(".uninitialized_data.terminal_rom"))) terminal_rom[4096];
uint8_t __attribute__((section(".uninitialized_data.terminal_ram"))) terminal_ram[4096];
extern uint8_t palette_select;

uint16_t stripe_colors[18] = {
    0x06d2,
    0x06d2,
    0x06d2,
    0x0fd4,
    0x0fd4,
    0x0fd4,
    0x0f14,
    0x0f14,
    0x0f14,
    0x07cc,
    0x07cc,
    0x07cc,
    0x09c9,
    0x09c9,
    0x09c9,
    0x009b,
    0x009b,
    0x009b,
};

uint16_t color_fore = RGB_WHITE;
uint16_t color_back = RGB_BLACK;

static void __noinline __time_critical_func(render_logo)(uint16_t line, bool mono);
static void __noinline __time_critical_func(render_menu_line)(uint16_t line);
static void __noinline __time_critical_func(render_line)(uint16_t line, bool mono);
static void __noinline __time_critical_func(render_border)(uint16_t count);
static void __noinline __time_critical_func(render_black)(uint16_t count);

uint16_t __attribute__((section(".time_critical.color_palette"))) color_palette[16] = {
    RGB_BLACK,    RGB_MAGENTA,  RGB_DBLUE,     RGB_HVIOLET,
    RGB_DGREEN,   RGB_HGRAY,    RGB_HBLUE,     RGB_LBLUE,
    RGB_BROWN,    RGB_HORANGE,  RGB_HGRAY,     RGB_PINK,
    RGB_HGREEN,   RGB_YELLOW,   RGB_AQUA,      RGB_WHITE
};

uint16_t __attribute__((section(".time_critical.half_palette"))) half_palette[16] = {
    (RGB_BLACK>>1) & 0x777,     (RGB_MAGENTA>>1) & 0x777,
    (RGB_DBLUE>>1) & 0x777,     (RGB_HVIOLET>>1) & 0x777,
    (RGB_DGREEN>>1) & 0x777,    (RGB_HGRAY>>1) & 0x777,
    (RGB_HBLUE>>1) & 0x777,     (RGB_LBLUE>>1) & 0x777,
    (RGB_BROWN>>1) & 0x777,     (RGB_HORANGE>>1) & 0x777,
    (RGB_HGRAY>>1) & 0x777,     (RGB_PINK>>1) & 0x777,
    (RGB_HGREEN>>1) & 0x777,    (RGB_YELLOW>>1) & 0x777,
    (RGB_AQUA>>1) & 0x777,      (RGB_WHITE>>1) & 0x777
};

void __noinline __time_critical_func(vga_main)() {
    bool mono = false;
    uint8_t line;

    for(;;) {
        vga_prepare_frame();
        if(logo_screen) {
            if(menu_screen == 2) {
                for(line=0; line < 7; line++) {
                    render_menu_line(line);
                }
                for(line=0; line < 180; line++) {
                    render_logo(line, (palette_select != 0));
                }
                for(line=16; line < 24; line++) {
                    render_menu_line(line);
                }
                if((logo_frame < 660) || (logo_frame >= 751)) {
                    logo_frame+=6;
                }
            } else {
                render_black(140);
                for(line=0; line < 180; line++) {
                    render_logo(line, false);
                }
                render_black(160);
                logo_frame+=2;
                if(logo_frame >= 950) {
                    logo_frame = 0;
                    logo_screen = false;
                }
            }
        } else if(menu_screen) {
            for(line=0; line < 24; line++) {
                render_menu_line(line);
            }
        } else {
            for(line=0; line < 240; line++) {
                render_line(line+LINE_OFFSET, (palette_select != 0) || v_mode[line+LINE_OFFSET]);
            }
        }
    }
}

static void __noinline __time_critical_func(render_logo)(uint16_t line, bool mono) {
    uint16_t stripe = stripe_colors[line / 10];
    struct vga_scanline *sl = vga_prepare_scanline();
    uint16_t sl_pos, i;
    int32_t x,y,z;
    uint32_t pixeldata;
    uint8_t bits;
    uint16_t local_color[4];

    local_color[0] = (mono) ? color_back : 0x000;
    local_color[1] = (mono) ? color_fore : 0xfff;
    local_color[2] = (mono) ? color_fore : stripe;

    sl_pos = 0;
    z=(line/30)*10 + 1200 - logo_frame;

    pixeldata = (local_color[0]) | ((local_color[0]) << 16);
    sl->data[sl_pos++] = pixeldata;
    pixeldata = (local_color[0]|THEN_EXTEND_7) | ((local_color[0]|THEN_EXTEND_7) << 16);
    for(i = 0; i < 13; i++) {
        sl->data[sl_pos++] = pixeldata;
    }

    for(i = 0; i < 220; i+=2, z++) {
        if((z < 450) || (z > 750)) {
            pixeldata = ((local_color[0]) | ((local_color[0]) << 16));
        } else {
            bits = v2logo[line*220+i];
            pixeldata = (z > 749) ? local_color[bits ? 1 : 0] : local_color[bits & 3];
            bits = v2logo[line*220+i+1];
            pixeldata |= ((z > 749) ? local_color[bits ? 1 : 0] : local_color[bits & 3]) << 16;
        }
        sl->data[sl_pos++] = pixeldata;
    }
    
    pixeldata = (local_color[0]|THEN_EXTEND_7) | ((local_color[0]|THEN_EXTEND_7) << 16);
    for(i = 0; i < 13; i++) {
        sl->data[sl_pos++] = pixeldata;
    }
    pixeldata = (local_color[0]) | ((local_color[0]) << 16);
    sl->data[sl_pos++] = pixeldata;

    sl->length = sl_pos;
    sl->repeat_count = 0;
    vga_submit_scanline(sl);
}

// Skip lines to center vertically or blank the screen
static void __noinline __time_critical_func(render_border)(uint16_t count) {
    struct vga_scanline *sl = vga_prepare_scanline();
    uint sl_pos = 0;

    while(sl_pos < VGA_WIDTH/16) {
        sl->data[sl_pos] = (color_back|THEN_EXTEND_7) | ((color_back|THEN_EXTEND_7) << 16);
        sl_pos++;
    }

    sl->length = sl_pos;
    sl->repeat_count = count - 1;
    vga_submit_scanline(sl);
}

// Skip lines to center vertically or blank the screen
static void __noinline __time_critical_func(render_black)(uint16_t count) {
    struct vga_scanline *sl = vga_prepare_scanline();
    uint sl_pos = 0;

    while(sl_pos < VGA_WIDTH/16) {
        sl->data[sl_pos] = (0x000|THEN_EXTEND_7) | ((0x000|THEN_EXTEND_7) << 16);
        sl_pos++;
    }

    sl->length = sl_pos;
    sl->repeat_count = count - 1;
    vga_submit_scanline(sl);
}

static void __noinline __time_critical_func(render_line)(uint16_t line, bool mono) {
    struct vga_scanline *sl = vga_prepare_scanline();
    uint sl_pos = 0;
    uint i;

    const uint16_t *line_mem = (const uint16_t *)(&v_buffer32[line * LINEBUFFER_LEN_32]);

    uint32_t dots = 0;
    uint_fast8_t dotc = 0;
    uint32_t pixeldata;
    uint32_t pixelmode = 0;

    i = 0;
    if(mono || (render_mode == 0)) {
        // 560x192 MONO
        while(i < 40) {
            if((dotc <= 16) && (i < 40)) {
                dots = line_mem[i+COL_OFFSET] << dotc;
                dotc += 16;
                i++;
            }

            // Consume pixels
            while((dotc >= 2) || ((dotc > 0) && (i == 40))) {
                pixeldata = ((dots & 1) ? color_fore : color_back);
                dots >>= 1;
                pixeldata |= (((dots & 1) ? color_fore : color_back)) << 16;
                dots >>= 1;
                sl->data[sl_pos++] = pixeldata;
                dotc -= 2;
            }
        }
    } else switch(render_mode) {
        case 1:
            // Naive 160x192 Color
            while(i < 40) {
                if((dotc <= 16) && (i < 40)) {
                    dots = line_mem[i+COL_OFFSET] << dotc;
                    dotc += 16;
                    i++;
                }

                // Consume pixels
                while((dotc >= 8) || ((dotc > 0) && (i == 40))) {
                    pixeldata = (color_palette[dots & 0xF] | THEN_EXTEND_3);
                    dots >>= 4;
                    pixeldata |= (color_palette[dots & 0xF] | THEN_EXTEND_3) << 16;
                    dots >>= 4;
                    sl->data[sl_pos++] = pixeldata;
                    dotc -= 8;
                }
            }
            break;
        case 2:
            // Interpolated 560x192 -  very close to AppleWin's "Idealized" Composite
            while(i < 40) {
                if((dotc <= 16) && (i < 40)) {
                    dots |= line_mem[i+COL_OFFSET] << dotc;
                    dotc += 16;
                    i++;
                }

                while((dotc >= 12) || ((dotc > 0) && (i == 40))) {
                    dots &= 0xfffffffe;
                    dots |= (dots >> 4) & 1;
                    pixeldata = color_palette[dots & 0xf];
                    dots &= 0xfffffffc;
                    dots |= (dots >> 4) & 3;
                    pixeldata |= color_palette[dots & 0xf] << 16;
                    sl->data[sl_pos++] = pixeldata;

                    dots &= 0xfffffff8;
                    dots |= (dots >> 4) & 7;
                    pixeldata = color_palette[dots & 0xf];
                    dots >>= 4;
                    pixeldata |= color_palette[dots & 0xf] << 16;
                    sl->data[sl_pos++] = pixeldata;

                    dotc -= 4;
                }
            }
            break;
        case 3:
            // Stippled Interpolated 560x192 - Similar to AppleWin's "Composite" Monitor
            while(i < 40) {
                if((dotc <= 16) && (i < 40)) {
                    dots |= line_mem[i+COL_OFFSET] << dotc;
                    dotc += 16;
                    i++;
                }

                while((dotc >= 12) || ((dotc > 0) && (i == 40))) {
                    dots &= 0xfffffffc;
                    dots |= (dots >> 4) & 3;
                    pixeldata = (line & 1) ? color_palette[dots & 0xf] : half_palette[dots & 0xf];

                    dots >>= 4;
                    pixeldata |= ((line & 1) ? half_palette[dots & 0xf] : color_palette[dots & 0xf]) << 16;
                    sl->data[sl_pos++] = pixeldata;

                    pixeldata |= (line & 1) ? color_palette[dots & 0xf] : half_palette[dots & 0xf];
                    dots &= 0xfffffffc;
                    dots |= (dots >> 4) & 3;
                    pixeldata |= ((line & 1) ? half_palette[dots & 0xf] : color_palette[dots & 0xf]) << 16;
                    sl->data[sl_pos++] = pixeldata;
                    dotc -= 4;
                }
            }
            break;
        case 4:
            // 560x192 Lookup Table
            while(i < 40) {
                if((dotc <= 16) && (i < 40)) {
                    dots |= line_mem[i+COL_OFFSET] << dotc;
                    dotc += 16;
                    i++;
                }

                while((dotc >= 12) || ((dotc > 0) && (i == 40))) {
                    sl->data[sl_pos++] = hires_color_patterns1[dots & 0xfff];
                    sl->data[sl_pos++] = hires_color_patterns2[dots & 0xfff];
                    dots >>= 4;
                    dotc -= 4;
                }
            }
            break;
    }

    sl->length = sl_pos;
    sl->repeat_count = 1;
    vga_submit_scanline(sl);
}

static inline uint_fast8_t char_terminal_bits(uint_fast8_t ch, uint_fast8_t glyph_line) {
    uint_fast8_t bits = terminal_rom[(((uint_fast16_t)ch & 0x7f) << 4) | glyph_line];

    if(ch & 0x80) {
        return (bits & 0x7f) ^ 0x7f;
    }
    return (bits & 0x7f);
}

static void __noinline __time_critical_func(render_menu_line)(uint16_t line) {
    uint text_line_offset = (line << 7) & 0xF80;
    const uint8_t *line_buf = (const uint8_t *)terminal_ram + text_line_offset;

    for(uint glyph_line = 0; glyph_line < 10; glyph_line++) {
        struct vga_scanline *sl = vga_prepare_scanline();
        uint sl_pos = 0;

        sl->data[sl_pos++] = color_back | ((color_back|THEN_EXTEND_3) << 16);

        for(uint col=0; col < 90; ) {
            // Grab 14 pixels from the next two characters
            uint32_t bits_a = char_terminal_bits(line_buf[col], glyph_line);
            col++;
            uint32_t bits_b = char_terminal_bits(line_buf[col], glyph_line);
            col++;

            uint32_t bits = (bits_a << 7) | bits_b;

            // Translate each pair of bits into a pair of pixels
            for(int i=0; i < 7; i++) {
                uint32_t pixeldata = ((bits & 0x2000) ? color_fore : color_back);
                pixeldata |= ((bits & 0x1000) ? color_fore : color_back) << 16;
                bits <<= 2;

                sl->data[sl_pos] = pixeldata;
                sl_pos++;
            }
        }

        sl->data[sl_pos++] = color_back | ((color_back|THEN_EXTEND_3) << 16);

        sl->length = sl_pos;
        sl->repeat_count = 1;
        vga_submit_scanline(sl);
    }
}

