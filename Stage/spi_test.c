#ifdef PREINIT_SUPPORTED
#include "preinit.h"
#endif

#include "MikroSDK.Driver.SPI.Master"
#include "MikroSDK.Driver.GPIO.Out"
#include "MikroSDK.Driver.UART"
#include "MikroSDK.Board"

// ── Pins Mikrobus 1 sur UNI-DS v8 ──────────────────────────
#define MIKROBUS1_SCK   GPIO_PA5
#define MIKROBUS1_MISO  GPIO_PA6
#define MIKROBUS1_MOSI  GPIO_PB5
#define MIKROBUS1_CS    GPIO_PA4
#define MIKROBUS1_RST   GPIO_PE11

// ── Commandes SPI ENC28J60 ──────────────────────────────────
#define ENC28J60_RCR_CMD  0x00
#define ENC28J60_SRC_CMD  0xFF

#define ECON1   0x1F
#define EREVID  0x12   // Bank 3 — révision hardware

// ── Objets globaux ──────────────────────────────────────────
static spi_master_t   spi;
static digital_out_t  cs;
static digital_out_t  rst;
static uart_t         uart;
static uart_config_t  uart_cfg;

// Buffers requis par le driver UART (mikroSDK 2.0)
static uint8_t uart_rx_buffer[128];
static uint8_t uart_tx_buffer[128];

// ── Fonctions SPI bas niveau ────────────────────────────────

void cs_low()  { digital_out_low(&cs);  }
void cs_high() { digital_out_high(&cs); }

void enc_soft_reset() {
    uint8_t cmd = ENC28J60_SRC_CMD;
    cs_low();
    spi_master_write(&spi, &cmd, 1);
    cs_high();
    Delay_ms(2);
}

void enc_select_bank(uint8_t bank) {
    uint8_t cmd_clr[2] = { 0xA0 | (ECON1 & 0x1F), 0x03 };
    cs_low();
    spi_master_write(&spi, cmd_clr, 2);
    cs_high();

    uint8_t cmd_set[2] = { 0x80 | (ECON1 & 0x1F), bank & 0x03 };
    cs_low();
    spi_master_write(&spi, cmd_set, 2);
    cs_high();
}

uint8_t enc_read_reg(uint8_t reg) {
    uint8_t cmd   = ENC28J60_RCR_CMD | (reg & 0x1F);
    uint8_t data  = 0;
    uint8_t dummy = 0;

    cs_low();
    spi_master_write(&spi, &cmd, 1);
    if (reg & 0x80) {
        spi_master_read(&spi, &dummy, 1);  // dummy read MAC/MII
    }
    spi_master_read(&spi, &data, 1);
    cs_high();

    return data;
}

// ── UART helpers (renommés pour éviter conflit avec le template) ───
// IMPORTANT : ne pas appeler ces fonctions "uart_print" / "uart_print_hex",
// ce nom est déjà utilisé/déclaré ailleurs dans le projet -> conflit de types.

void mb1_print(const char *str) {
    while (*str) {
        uart_write(&uart, (uint8_t *)str, 1);
        str++;
    }
}

void mb1_print_hex(uint8_t val) {
    const char hex[] = "0123456789ABCDEF";
    char buf[5] = "0x";
    buf[2] = hex[val >> 4];
    buf[3] = hex[val & 0x0F];
    buf[4] = '\0';
    mb1_print(buf);
}

// ── Main ────────────────────────────────────────────────────

int main(void) {

    #ifdef PREINIT_SUPPORTED
    preinit();
    #endif

    // ── Init UART (corrigée selon le demo officiel qui fonctionne) ──
    uart_configure_default(&uart_cfg);

    uart.tx_ring_buffer = uart_tx_buffer;
    uart.rx_ring_buffer = uart_rx_buffer;

    uart_cfg.tx_pin = USB_UART_TX;   // pins du convertisseur USB-UART de la carte
    uart_cfg.rx_pin = USB_UART_RX;

    uart_cfg.tx_ring_size = sizeof(uart_tx_buffer);
    uart_cfg.rx_ring_size = sizeof(uart_rx_buffer);

    uart_open(&uart, &uart_cfg);
    uart_set_baud(&uart, 115200);
    uart_set_parity(&uart, UART_PARITY_DEFAULT);
    uart_set_stop_bits(&uart, UART_STOP_BITS_DEFAULT);
    uart_set_data_bits(&uart, UART_DATA_BITS_DEFAULT);
    Delay_ms(100);

    mb1_print("UART OK\r\n");

    // Init CS et RST
    digital_out_init(&cs,  MIKROBUS1_CS);
    digital_out_init(&rst, MIKROBUS1_RST);
    cs_high();

    // Reset hardware ENC28J60
    digital_out_high(&rst);
    Delay_ms(10);
    digital_out_low(&rst);
    Delay_ms(10);
    digital_out_high(&rst);
    Delay_ms(10);
    mb1_print("Reset OK\r\n");

    // Init SPI
    spi_master_config_t spi_cfg;
    spi_master_configure_default(&spi_cfg);
    spi_cfg.sck   = MIKROBUS1_SCK;
    spi_cfg.miso  = MIKROBUS1_MISO;
    spi_cfg.mosi  = MIKROBUS1_MOSI;
    spi_cfg.speed = 1000000;
    spi_cfg.mode  = SPI_MASTER_MODE_0;
    spi_master_open(&spi, &spi_cfg);
    spi_master_set_chip_select_polarity(SPI_MASTER_CHIP_SELECT_DEFAULT_POLARITY);
    mb1_print("SPI OK\r\n");

    // Soft reset ENC28J60
    enc_soft_reset();
    Delay_ms(10);

    // Bank 3 → EREVID
    enc_select_bank(3);
    uint8_t rev = enc_read_reg(EREVID);

    mb1_print("ENC28J60 EREVID = ");
    mb1_print_hex(rev);
    mb1_print("\r\n");

    if (rev == 0x05 || rev == 0x06) {
        mb1_print(">>> SPI OK - ENC28J60 answered correctly !\r\n\n");
    } else {
        mb1_print(">>> ERREUR - unexpected value, check the wiring\r\n");
    }

    while (1) {
        Delay_ms(1000);
    }

    return 0;
}