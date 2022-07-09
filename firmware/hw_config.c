#include "hw_config.h"
#include "ff.h" // Obtains integer types
#include "diskio.h" // Declarations of disk functions

void spi1_dma_isr();

// Hardware Configuration of SPI "objects"
// Multiple SD cards can be driven by one SPI if they use different slave selects.

static spi_t spis[] = {  // One for each SPI.
    {
        .hw_inst = spi1, // SPI component
        .miso_gpio = 12, // GPIO number (not pin number)
        .mosi_gpio = 15,
        .sck_gpio = 14,
        .baud_rate = 20e6,

        .dma_isr = spi1_dma_isr
    }};

// Hardware Configuration of the SD Card "objects"
static sd_card_t sd_cards[] = {  // One for each SD card
    {
        .pcName = "0:",           // Name used to mount device
        .spi = &spis[0],          // Pointer to the SPI driving this card
        .ss_gpio = 13,            // The SPI slave select GPIO for this SD card

        .use_card_detect = false,
        .card_detect_gpio = -1,   // Card detect
        .card_detected_true = 1,  // What the GPIO read returns when a card is
                                  // present. Use -1 if there is no card detect.
        .m_Status = STA_NOINIT,
    }
};

void spi1_dma_isr()
{
    spi_irq_handler(&spis[0]);
}

/* ********************************************************************** */
size_t sd_get_num() {
    return count_of(sd_cards);
}

sd_card_t *sd_get_by_num(size_t num)
{
    if (num <= sd_get_num()) {
        return &sd_cards[num];
    } else {
        return NULL;
    }
}

size_t spi_get_num()
{
    return count_of(spis);
}

spi_t *spi_get_by_num(size_t num)
{
    if (num <= sd_get_num()) {
        return &spis[num];
    } else {
        return NULL;
    }
}