#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "render.h"
#include "iic.h"

#define GPIO_BTN2 12
#define GPIO_BTN3 13

// Videx Character Generator ROM
extern const uint8_t rom_videx[];
extern const uint32_t rom_videx_size;

// We load these into RAM for speed at runtime
extern uint8_t __attribute__((section(".uninitialized_data.terminal_rom"))) terminal_rom[4096];

// Menu screen text buffer
extern uint8_t __attribute__((section(".uninitialized_data.terminal_ram"))) terminal_ram[4096];

extern uint16_t color_border;
extern uint16_t color_fore;
extern uint16_t color_back;

void core1_entry();

uint8_t menu_screen = 0;
bool menu_dirty = 0;

bool btn_mode = 0;
bool btn_select = 0;

uint8_t menu_line = 0;
uint8_t menu_max = 0;

#define RENDER_MAX 5
uint8_t render_mode = 4;

#define PALETE_MAX 6
uint8_t palette_select = 0;

extern volatile uint32_t sync_min;
extern volatile uint32_t sync_max;

// These aren't correct yet.
uint16_t palette_map[6*3] = {
    0x000, 0x000, 0xfff,
    0xfff, 0xfff, 0x000,

    0x000, 0x000, 0x07c,
    0x07c, 0x07c, 0x000,

    0x000, 0x000, 0x360,
    0x360, 0x360, 0x000,
};


void __noinline __time_critical_func(menu_paint_text)(uint16_t y, uint16_t x, const char *text) {
    memcpy(&terminal_ram[y * 128 + x], text, strlen(text));
}

uint8_t hexdigit[] = "0123456789ABCDEF";

void __noinline __time_critical_func(menu_paint_hex12)(uint16_t y, uint16_t x, uint16_t value) {
    terminal_ram[y * 128 + x + 0] = '#';
    terminal_ram[y * 128 + x + 1] = hexdigit[(value >> 8) & 0xf];
    terminal_ram[y * 128 + x + 2] = hexdigit[(value >> 4) & 0xf];
    terminal_ram[y * 128 + x + 3] = hexdigit[(value >> 0) & 0xf];
}

void __noinline __time_critical_func(menu_paint_u32)(uint16_t y, uint16_t x, uint32_t value) {
    terminal_ram[y * 128 + x + 0] = '0';
    terminal_ram[y * 128 + x + 1] = 'x';
    terminal_ram[y * 128 + x + 2] = hexdigit[(value >> 28) & 0xf];
    terminal_ram[y * 128 + x + 3] = hexdigit[(value >> 24) & 0xf];
    terminal_ram[y * 128 + x + 4] = hexdigit[(value >> 20) & 0xf];
    terminal_ram[y * 128 + x + 5] = hexdigit[(value >> 16) & 0xf];
    terminal_ram[y * 128 + x + 6] = hexdigit[(value >> 12) & 0xf];
    terminal_ram[y * 128 + x + 7] = hexdigit[(value >> 8) & 0xf];
    terminal_ram[y * 128 + x + 8] = hexdigit[(value >> 4) & 0xf];
    terminal_ram[y * 128 + x + 9] = hexdigit[(value >> 0) & 0xf];
}

void __noinline __time_critical_func(menu_paint_main)(uint8_t line) {
    // Number of selectable menu lines
    menu_max = 4;
    if(!line) memset(terminal_ram, ' ', sizeof(terminal_ram));
    if(!line) menu_paint_text(2, 4, "V2c Main Menu");
    if(!line || (line==1)) {
        menu_paint_text(4, 6, "Render Mode:");
        switch(render_mode) {
            default:
                render_mode = 0;
            case 0:
                menu_paint_text(4, 20, "Monochrome  ");
                break;
            case 1:
                menu_paint_text(4, 20, "Naive       ");
                break;
            case 2:
                menu_paint_text(4, 20, "Idealized   ");
                break;
            case 3:
                menu_paint_text(4, 20, "Composite   ");
                break;
            case 4:
                menu_paint_text(4, 20, "Lookup Table");
                break;
        }
    }

    if(!line || (line==2)) {
        menu_paint_text(5, 6, "Palette:");
        switch(palette_select) {
            case 0:
                menu_paint_text(5, 20, "Normal        ");
                break;
            case 1:
                menu_paint_text(5, 20, "Inverse       ");
                break;
            case 2:
                menu_paint_text(5, 20, "Green         ");
                break;
            case 3:
                menu_paint_text(5, 20, "Green Inverse ");
                break;
            case 4:
                menu_paint_text(5, 20, "Amber         ");
                break;
            case 5:
                menu_paint_text(5, 20, "Amber Inverse ");
                break;
        }

        menu_paint_text(6, 8, "Border:");
        menu_paint_hex12(6, 20, color_border);

        menu_paint_text(7, 8, "Background:");
        menu_paint_hex12(7, 20, color_back);

        menu_paint_text(8, 8, "Foreground:");
        menu_paint_hex12(8, 20, color_fore);
    }

    if(!line || (line==3)) {
        menu_paint_text(9, 6, "About");
    }

    if(!line || (line==4)) {
        menu_paint_text(10, 6, "Exit Menu");
    }

    if(!line) switch(menu_line) {
        case 0:
            menu_paint_text(4, 4, ">");
            break;
        case 1:
            menu_paint_text(5, 4, ">");
            break;
        case 2:
            menu_paint_text(9, 4, ">");
            break;
        case 3:
            menu_paint_text(10, 4, ">");
            break;
    }
    
    if(!line) menu_paint_text(20, 4, "Sync Min:");
    if(!line) menu_paint_text(21, 4, "Sync Max:");

}

void __noinline __time_critical_func(menu_action_main)() {
    switch(menu_line) {
        case 0:
            render_mode = (render_mode + 1) % RENDER_MAX;
            menu_paint_main(1);
            break;
        case 1:
            palette_select = (palette_select + 1) % PALETE_MAX;
            color_border = palette_map[palette_select * 3 + 0];
            color_back = palette_map[palette_select * 3 + 1];
            color_fore = palette_map[palette_select * 3 + 2];
            menu_paint_main(2);
            break;
        case 2:
            menu_screen = 2;
            menu_dirty = 1;
            menu_line = 0;
            break;
        case 3:
            menu_screen = 0;
            break;
    }
}

void __noinline __time_critical_func(menu_select_main)() {
    switch(menu_line) {
        case 0:
            menu_paint_text(4, 4, " ");
            break;
        case 1:
            menu_paint_text(5, 4, " ");
            break;
        case 2:
            menu_paint_text(9, 4, " ");
            break;
        case 3:
            menu_paint_text(10, 4, " ");
            break;
    }
    menu_line = (menu_line + 1) % menu_max;
    switch(menu_line) {
        case 0:
            menu_paint_text(4, 4, ">");
            break;
        case 1:
            menu_paint_text(5, 4, ">");
            break;
        case 2:
            menu_paint_text(9, 4, ">");
            break;
        case 3:
            menu_paint_text(10, 4, ">");
            break;
    }
}

void __noinline __time_critical_func(menu_paint_about)(uint8_t line) {
    // Number of selectable menu lines
    menu_max = 3;
    if(!line) memset(terminal_ram, ' ', sizeof(terminal_ram));
    if(!line) menu_paint_text(10, 23, "V2c Hardware and Firmware Designed");
    if(!line) menu_paint_text(11, 29, "in 2023 by David Kuder");
    if(!line) menu_paint_text(20, 28, "> Press Select to exit <");
}

void __noinline __time_critical_func(menu_action_about)() {
    menu_screen = 1;
    menu_dirty = 1;
    menu_line = 0;
}

void __noinline __time_critical_func(menu_select_about)() {
}

void __noinline __time_critical_func(menu_main)() {
    for(;;) {
        if (!gpio_get(GPIO_BTN2)) {
            while(!gpio_get(GPIO_BTN2)) sleep_ms(5);
            if(!menu_screen) {
                menu_screen = 1;
                menu_dirty = 1;
                menu_line = 1;
            } else {
                btn_mode = 1;
            }
        } else if (!gpio_get(GPIO_BTN3)) {
            while(!gpio_get(GPIO_BTN3)) sleep_ms(5);
            btn_select = 1;
        }
        if(menu_screen) {
            menu_paint_u32(20, 16, sync_min);
            menu_paint_u32(21, 16, sync_max);
            if(menu_dirty) {
                menu_dirty = 0;
                switch(menu_screen) {
                    case 1:
                        menu_paint_main(0);
                        break;
                    case 2:
                        menu_paint_about(0);
                        break;
                }
            }
            if(btn_mode) {
                btn_mode = 0;

                // Cycle through menu lines
                switch(menu_screen) {
                    case 1:
                        menu_select_main();
                        break;
                    case 2:
                        menu_select_about();
                        break;
                }
            }
            if(btn_select) {
                btn_select = 0;

                switch(menu_screen) {
                    case 1:
                        menu_action_main();
                        break;
                    case 2:
                        menu_action_about();
                        break;
                }
            }
        } else if(btn_select) {
            btn_select = 0;

            render_mode = (render_mode + 1) % RENDER_MAX;
        }
        sleep_ms(50);
    }
}

int main() {
    // set clock speed
    set_sys_clock_khz(CONFIG_SYSCLOCK * 1000, true);

    // initialise onboard LEDs for debug flashing
    for(int i = 0; i < 4; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
    }

    for(int i = 0; i < 4; i++) {
        gpio_put(i, true);
        sleep_ms(50);
        gpio_put(i, false);
    }

    gpio_init(GPIO_BTN2);
    gpio_set_dir(GPIO_BTN2, GPIO_IN);
    gpio_pull_up(GPIO_BTN2);

    gpio_init(GPIO_BTN3);
    gpio_set_dir(GPIO_BTN3, GPIO_IN);
    gpio_pull_up(GPIO_BTN3);

    // launch VGA on core1
    multicore_launch_core1(core1_entry);

    sleep_ms(250);

    // setup everything for video capture
    gpio_put(0, true);
    iic_init();
    gpio_put(0, false);

    menu_main();

    // should never reach here
    for(;;) {
        gpio_put(0, true);
        sleep_ms(125);
        gpio_put(0, false);
        sleep_ms(125);
    }
}

// all things VGA
void __noinline __time_critical_func(core1_entry)() {
    // Blank menu screen
    memset(terminal_ram, ' ', sizeof(terminal_ram));

    // Load font
    memcpy(terminal_rom, rom_videx, sizeof(terminal_rom));

    gpio_put(1, true);
    vga_init();
    gpio_put(1, false);
    vga_main();

    // should never reach here
    for(;;) {
        gpio_put(1, true);
        sleep_ms(125);
        gpio_put(1, false);
        sleep_ms(125);
    }
}
