#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/sync.h>
#include <hardware/resets.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <string.h>

#include "vidin.pio.h"

#define IIC_NO_EXTERN
#include "iic.h"

volatile uint8_t v_mode[MAX_LINES];
volatile uint32_t v_buffer32[BUFFER_LEN_32];

volatile uint16_t v_curline = 0;
volatile uint16_t v_curcolumn = 0;
volatile uint32_t sync_min = 0xffffffff;
volatile uint32_t sync_max = 0x00000000;

uint datain_dma_channel;

#define CAPTURE_PIO pio1

enum {
 DATAIN_SM = 0,
 SYNCIN_SM = 1,
};

bool led_status = false;

void __isr __time_critical_func(gpio_callback)(uint gpio, uint32_t events) {
    uint32_t synclen;

    // so that we sample the pixels at the same point every scanline
    pio_sm_clkdiv_restart(CAPTURE_PIO, DATAIN_SM);

    if (dma_channel_is_busy(datain_dma_channel)) {
        // we need to be ready to copy the new data
        dma_channel_abort(datain_dma_channel);
        // clear fifos because if the channel is interrupted there will be old pixel data left
        pio_sm_clear_fifos(CAPTURE_PIO, DATAIN_SM);
    }

    // Wait for pulse length from PIO
    synclen = pio_sm_get_blocking(CAPTURE_PIO, SYNCIN_SM);
    if(synclen > 100) {
        // V-Sync
        v_curline=0;
    } else if(v_curline < MAX_LINES-2) {
        // H-Sync
        v_curline++;
    }
    v_curcolumn = 0;

    dma_channel_set_write_addr(datain_dma_channel, &v_buffer32[v_curline*LINEBUFFER_LEN_32], true);

    v_mode[v_curline] = (gpio_get(GR_PIN) ? 0 : 1);
    if(synclen < sync_min) sync_min = synclen;
    if(synclen > sync_max) sync_max = synclen;
}

void iic_init() {
    pio_sm_config c;
    int pio_offset;

    // initialize the GPIOs
    pio_gpio_init(CAPTURE_PIO, GPIO_PIXELCLOCK);
    gpio_set_dir(GPIO_PIXELCLOCK, GPIO_IN);
    gpio_set_pulls(GPIO_PIXELCLOCK, false, false);

    pio_gpio_init(CAPTURE_PIO, GPIO_SYNC);
    gpio_set_dir(GPIO_SYNC, GPIO_IN);
    gpio_set_pulls(GPIO_SYNC, false, false);

    pio_gpio_init(CAPTURE_PIO, GPIO_WNDW);
    gpio_set_dir(GPIO_WNDW, GPIO_IN);
    gpio_set_pulls(GPIO_WNDW, false, false);

    pio_gpio_init(CAPTURE_PIO, GPIO_DATA);
    gpio_set_dir(GPIO_DATA, GPIO_IN);
    gpio_set_pulls(GPIO_DATA, false, false);


    // Load video data input program
    pio_offset = pio_add_program(CAPTURE_PIO, &datain_program);
    c = datain_program_get_default_config(pio_offset);

    // true - shift right, auto pull, # of bits
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    sm_config_set_in_pins(&c, GPIO_DATA);
    sm_config_set_jmp_pin(&c, GPIO_SYNC);

    sm_config_set_clkdiv(&c, IIC_CLOCK_DIV);

    pio_sm_init(CAPTURE_PIO, DATAIN_SM, pio_offset, &c);
    pio_sm_clear_fifos(CAPTURE_PIO, DATAIN_SM);


    // Load video sync input program
    pio_offset = pio_add_program(CAPTURE_PIO, &syncin_program);
    c = syncin_program_get_default_config(pio_offset);

    // true - shift right, auto pull, # of bits
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    sm_config_set_jmp_pin(&c, GPIO_SYNC);

    sm_config_set_clkdiv(&c, IIC_CLOCK_DIV);

    pio_sm_init(CAPTURE_PIO, SYNCIN_SM, pio_offset, &c);
    pio_sm_clear_fifos(CAPTURE_PIO, SYNCIN_SM);

    datain_dma_channel = 6; // dma_claim_unused_channel(true);
    dma_channel_claim(datain_dma_channel);
    dma_channel_config channel_config = dma_channel_get_default_config(datain_dma_channel);
    channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_32);
    channel_config_set_read_increment(&channel_config, false);
    channel_config_set_write_increment(&channel_config, true);
    channel_config_set_dreq(&channel_config, pio_get_dreq(CAPTURE_PIO, DATAIN_SM, false));
    channel_config_set_ring(&channel_config, true, 0);

    dma_channel_configure(datain_dma_channel, &channel_config, &v_buffer32[0], &CAPTURE_PIO->rxf[DATAIN_SM], LINEBUFFER_LEN_32, false);

    // Start the PIO state machines
    pio_enable_sm_mask_in_sync(CAPTURE_PIO, (1 << DATAIN_SM) | (1 << SYNCIN_SM));

    gpio_set_irq_enabled_with_callback(GPIO_SYNC, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
}
