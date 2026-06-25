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
#define ENC28J60_WCR_CMD  0x40
#define ENC28J60_BFS_CMD  0x80
#define ENC28J60_BFC_CMD  0xA0
#define ENC28J60_SRC_CMD  0xFF

// Bank 0
#define ERDPTL   0x00
#define ERDPTH   0x01
#define ERXSTL   0x08
#define ERXSTH   0x09
#define ERXNDL   0x0A
#define ERXNDH   0x0B
#define ERXRDPTL 0x0C
#define ERXRDPTH 0x0D
// Bank-independent
#define ECON1    0x1F
#define ECON1_RXEN 0x04
#define ESTAT    0x1D
#define ESTAT_CLKRDY 0x08
// Bank 1
#define ERXFCON  0x18
// Bank 2
#define MACON1   (0x00|0x80)
#define MACON3   (0x02|0x80)
#define MACON4   (0x03|0x80)
#define MABBIPG  (0x04|0x80)
#define MAIPGL   (0x06|0x80)
#define MAIPGH   (0x07|0x80)
#define MAMXFLL  (0x0A|0x80)
#define MAMXFLH  (0x0B|0x80)
#define MICMD    (0x12|0x80)
#define MIREGADR (0x14|0x80)
#define MIWRL    (0x16|0x80)
#define MIWRH    (0x17|0x80)
#define MIRDL    (0x18|0x80)
#define MIRDH    (0x19|0x80)
// Bank 3
#define MAADR5   (0x00|0x80)
#define MAADR6   (0x01|0x80)
#define MAADR3   (0x02|0x80)
#define MAADR4   (0x03|0x80)
#define MAADR1   (0x04|0x80)
#define MAADR2   (0x05|0x80)
#define EREVID   0x12
// PHY registers (accédés via MII, pas via SPI direct)
#define PHCON1   0x00
#define PHHID1   0x02
#define PHSTAT2  0x11

// ── Objets globaux ──────────────────────────────────────────
static spi_master_t   spi;
static digital_out_t  cs;
static digital_out_t  rst;
static uart_t         uart;
static uart_config_t  uart_cfg;
static uint8_t uart_rx_buffer[128];
static uint8_t uart_tx_buffer[128];

// MAC locale arbitraire (pas une MAC officielle - suffisant pour un POC)
static const uint8_t local_mac[6] = { 0x02, 0xDE, 0xAD, 0xBE, 0xEF, 0x01 };

// ── SPI bas niveau ───────────────────────────────────────────
void cs_low()  { digital_out_low(&cs);  }
void cs_high() { digital_out_high(&cs); }

void enc_select_bank(uint8_t bank) {
    uint8_t clr[2] = { ENC28J60_BFC_CMD | (ECON1 & 0x1F), 0x03 };
    cs_low(); spi_master_write(&spi, clr, 2); cs_high();
    uint8_t set[2] = { ENC28J60_BFS_CMD | (ECON1 & 0x1F), bank & 0x03 };
    cs_low(); spi_master_write(&spi, set, 2); cs_high();
}

uint8_t enc_read_reg(uint8_t reg) {
    uint8_t cmd = ENC28J60_RCR_CMD | (reg & 0x1F);
    uint8_t dummy = 0, data = 0;
    cs_low();
    spi_master_write(&spi, &cmd, 1);
    if (reg & 0x80) spi_master_read(&spi, &dummy, 1); // dummy byte registres MAC/MII
    spi_master_read(&spi, &data, 1);
    cs_high();
    return data;
}

void enc_write_reg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { (uint8_t)(ENC28J60_WCR_CMD | (reg & 0x1F)), value };
    cs_low(); spi_master_write(&spi, buf, 2); cs_high();
}

void enc_bfs(uint8_t reg, uint8_t mask) {
    uint8_t buf[2] = { (uint8_t)(ENC28J60_BFS_CMD | (reg & 0x1F)), mask };
    cs_low(); spi_master_write(&spi, buf, 2); cs_high();
}

void enc_soft_reset(void) {
    uint8_t cmd = ENC28J60_SRC_CMD;
    cs_low(); spi_master_write(&spi, &cmd, 1); cs_high();
    Delay_ms(2);
}

// Attend l'oscillateur stable (erratum ENC28J60 : accès PHY avant
// CLKRDY=1 -> lectures MII incohérentes/à 0).
void enc_wait_clkready(void) {
    uint16_t tries = 0;
    while (!(enc_read_reg(ESTAT) & ESTAT_CLKRDY)) {
        Delay_ms(1);
        if (++tries > 200) break; // timeout sécurité ~200ms
    }
}

// ── Accès MII (registres PHY) ───────────────────────────────
uint8_t enc_phy_read(uint8_t phy_reg, uint8_t *low, uint8_t *high)
{
    enc_select_bank(2);

    enc_write_reg(MIREGADR, phy_reg);

    // start read
    enc_write_reg(MICMD, 0x01);

    // WAIT MIIRD completion (IMPORTANT)
    while (enc_read_reg(MICMD) & 0x01);

    // required small delay after completion
    Delay_ms(1);

    *low  = enc_read_reg(MIRDL);
    *high = enc_read_reg(MIRDH);

    return 1;
}

// Ecriture d'un registre PHY (16 bits) : pas besoin de MICMD, l'écriture
// de MIWRH déclenche automatiquement le transfert vers le PHY.
void enc_phy_write(uint8_t phy_reg, uint16_t value) {
    enc_select_bank(2);
    enc_write_reg(MIREGADR, phy_reg);
    enc_write_reg(MIWRL, (uint8_t)(value & 0xFF));
    enc_write_reg(MIWRH, (uint8_t)(value >> 8));
    Delay_ms(1); // datasheet : attendre ~10.24us avant tout autre accès MII
}

// ── UART helpers ─────────────────────────────────────────────
void mb1_print(const char *str) {
    while (*str) { uart_write(&uart, (uint8_t *)str, 1); str++; }
}
void mb1_print_hex(uint8_t val) {
    const char hex[] = "0123456789ABCDEF";
    char buf[5] = "0x";
    buf[2] = hex[val >> 4]; buf[3] = hex[val & 0x0F]; buf[4] = '\0';
    mb1_print(buf);
}

// ── Initialisation "ce que le chip attend pour monter le module Ethernet" ──
void enc_setup_ethernet(void) {
    // Bank 0 : buffer de réception (on réserve toute la RAM dispo à la RX,
    // pas de TX pour ce POC -> on ne fait que vérifier le link, pas envoyer)
    enc_select_bank(0);
    enc_write_reg(ERXSTL, 0x00);
    enc_write_reg(ERXSTH, 0x00);
    enc_write_reg(ERXNDL, 0xFF);
    enc_write_reg(ERXNDH, 0x1F);
    enc_write_reg(ERXRDPTL, 0x00);
    enc_write_reg(ERXRDPTH, 0x00);

    // Bank 1 : filtre de réception (unicast + broadcast + CRC valide)
    enc_select_bank(1);
    enc_write_reg(ERXFCON, 0xA1);

    // Bank 2 : config MAC
    enc_select_bank(2);
    enc_write_reg(MACON1, 0x01);      // MARXEN : active la réception MAC
    enc_write_reg(MACON3, 0x32);      // PAD à 60 octets + CRC auto, half-duplex
    enc_write_reg(MACON4, 0x40);      // DEFER (requis half-duplex)
    enc_write_reg(MABBIPG, 0x12);     // back-to-back inter-packet gap (half-duplex)
    enc_write_reg(MAIPGL, 0x12);
    enc_write_reg(MAIPGH, 0x0C);
    enc_write_reg(MAMXFLL, 0xEE);     // longueur max trame = 1518 (0x05EE)
    enc_write_reg(MAMXFLH, 0x05);

    // Bank 3 : adresse MAC (locale arbitraire pour ce POC)
    enc_select_bank(3);
    enc_write_reg(MAADR1, local_mac[0]);
    enc_write_reg(MAADR2, local_mac[1]);
    enc_write_reg(MAADR3, local_mac[2]);
    enc_write_reg(MAADR4, local_mac[3]);
    enc_write_reg(MAADR5, local_mac[4]);
    enc_write_reg(MAADR6, local_mac[5]);

    // PHY : forcer half-duplex côté PHY pour matcher MACON3 (PDPXMD = 0)
    enc_phy_write(PHCON1, 0x0000);

    // Activer la réception au niveau MAC/ECON1
    enc_bfs(ECON1, ECON1_RXEN);
}

// ── Main ─────────────────────────────────────────────────────
int main(void) {
    #ifdef PREINIT_SUPPORTED
    preinit();
    #endif

    // UART
    uart_configure_default(&uart_cfg);
    uart.tx_ring_buffer = uart_tx_buffer;
    uart.rx_ring_buffer = uart_rx_buffer;
    uart_cfg.tx_pin = USB_UART_TX;
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

    // CS / RESET
    digital_out_init(&cs,  MIKROBUS1_CS);
    digital_out_init(&rst, MIKROBUS1_RST);
    cs_high();
    digital_out_high(&rst); Delay_ms(10);
    digital_out_low(&rst);  Delay_ms(10);
    digital_out_high(&rst); Delay_ms(10);
    mb1_print("Reset OK\r\n");

    // SPI
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

    enc_soft_reset();
    Delay_ms(10);
    enc_wait_clkready();
    mb1_print("CLKRDY OK\r\n");

    // Sanity check (comme avant)
    enc_select_bank(3);
    uint8_t rev = enc_read_reg(EREVID);
    mb1_print("EREVID = "); mb1_print_hex(rev); mb1_print("\r\n");

    // Vraie config du module Ethernet
    enc_setup_ethernet();
    mb1_print("Module Ethernet configure (buffers RX + MAC)\r\n");

    // ── Test diagnostique : PHID1 doit valoir 0x0083 (valeur fixe documentée
    //    ENC28J60), indépendante du câble. Confirme que le mécanisme MII
    //    fonctionne, avant de se fier à PHSTAT2.
    uint8_t low, high;
    enc_phy_read(PHHID1, &low, &high);
    mb1_print("PHID1 = "); mb1_print_hex(high); mb1_print_hex(low); mb1_print("\r\n");

    // Lecture du link status via PHSTAT2 (bit LSTAT = bit 10 -> bit2 de MIRDH)
    enc_phy_read(PHSTAT2, &low, &high);
    mb1_print("PHSTAT2 = "); mb1_print_hex(high); mb1_print_hex(low); mb1_print("\r\n");

    if (high & 0x04) {
        mb1_print(">>> LINK UP - cable Ethernet detecte !\r\n");
    } else {
        mb1_print(">>> LINK DOWN - verifier le cable RJ45\r\n");
    }

    while (1) {
        // Repolling du link toutes les secondes
        enc_phy_read(PHSTAT2, &low, &high);
        mb1_print("Link = ");
        mb1_print((high & 0x04) ? "UP\r\n" : "DOWN\r\n");
        Delay_ms(1000);
    }

    return 0;
}