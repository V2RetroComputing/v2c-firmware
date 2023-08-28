#include <pico/stdlib.h>

#define IIC_CLOCK_DIV 1

// pin assignments
#define CLOCK_PIN  4   // pin 6
#define GR_PIN     5   // pin 7
#define SYNC_PIN   6   // pin 8
#define DATA_PIN   7   // pin 9
#define SEGB_PIN   8   // pin 11
#define LDPS_PIN   9   // pin 12
#define D7_PIN     10  // pin 13
#define WNDW_PIN   11  // pin 14

#define MAX_LINES 262
#define LINE_OFFSET 11
#define COL_OFFSET 9

// buffers need to be multiple of 4 for 32 bit copies
#define LINEBUFFER_LEN_32 32  // X*4*8 = # of bits
#define LINEBUFFER_LEN_8 LINEBUFFER_LEN_32*4
#define BUFFER_LEN_32 MAX_LINES*LINEBUFFER_LEN_32
#define BUFFER_LEN_8 BUFFER_LEN_32*4

#ifndef IIC_NO_EXTERN
extern volatile bool v_curbuf;
extern volatile uint32_t v_buffer32[BUFFER_LEN_32];
extern volatile uint8_t v_mode[MAX_LINES];
extern volatile uint16_t v_curline;
extern volatile uint16_t v_curcolumn;

extern void iic_init();
extern void __noinline __time_critical_func(iic_main)();
#endif

