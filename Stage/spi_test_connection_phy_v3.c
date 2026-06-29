#ifdef PREINIT_SUPPORTED
#include "preinit.h"
#endif

#include "MikroSDK.Driver.SPI.Master"
#include "MikroSDK.Driver.GPIO.Out"
#include "MikroSDK.Driver.UART"
#include "MikroSDK.Board"

// ── Pins Mikrobus 2 sur UNI-DS v8 ──────────────────────────
#define MIKROBUS2_SCK   GPIO_PA5
#define MIKROBUS2_MISO  GPIO_PA6
#define MIKROBUS2_MOSI  GPIO_PB5
#define MIKROBUS2_CS    GPIO_PB2
#define MIKROBUS2_RST   GPIO_PE12

// ── Commandes SPI ENC28J60 ──────────────────────────────────
#define ENC28J60_RCR_CMD  0x00
#define ENC28J60_WCR_CMD  0x40
#define ENC28J60_BFS_CMD  0x80
#define ENC28J60_BFC_CMD  0xA0
#define ENC28J60_SRC_CMD  0xFF

// ── Zones mémoire (RAM interne = 8KB : 0x0000–0x1FFF) ───────
// RX : 0x0000 – 0x17FF (6144 octets)
// TX : 0x1800 – 0x1FFF (2048 octets)
#define RXBUF_START  0x0000
#define RXBUF_END    0x17FF
#define TXBUF_START  0x1800

// ── Registres Bank 0 ─────────────────────────────────────────
#define ERDPTL   0x00
#define ERDPTH   0x01
#define EWRPTL   0x02
#define EWRPTH   0x03
#define ETXSTL   0x04
#define ETXSTH   0x05
#define ETXNDL   0x06
#define ETXNDH   0x07
#define ERXSTL   0x08
#define ERXSTH   0x09
#define ERXNDL   0x0A
#define ERXNDH   0x0B
#define ERXRDPTL 0x0C
#define ERXRDPTH 0x0D
#define EIR      0x1C
#define ESTAT    0x1D
#define ECON2    0x1E
#define ECON1    0x1F

#define ECON1_RXEN   0x04
#define ECON1_TXRST  0x80
#define ECON1_TXRTS  0x08
#define ECON2_PKTDEC 0x40
#define EIR_TXERIF   0x02
#define EIR_TXIF     0x08
#define ESTAT_CLKRDY 0x01
#define EPKTCNT      0x19   // Bank 1

// ── Registres Bank 1 ─────────────────────────────────────────
#define ERXFCON  0x18

// ── Registres Bank 2 (bit7=1 = registre MAC/MII) ─────────────
#define MACON1   (0x00 | 0x80)
#define MACON3   (0x02 | 0x80)
#define MACON4   (0x03 | 0x80)
#define MABBIPG  (0x04 | 0x80)
#define MAIPGL   (0x06 | 0x80)
#define MAIPGH   (0x07 | 0x80)
#define MAMXFLL  (0x0A | 0x80)
#define MAMXFLH  (0x0B | 0x80)
#define MICMD    (0x12 | 0x80)
#define MIREGADR (0x14 | 0x80)
#define MIWRL    (0x16 | 0x80)
#define MIWRH    (0x17 | 0x80)
#define MIRDL    (0x18 | 0x80)
#define MIRDH    (0x19 | 0x80)
#define MISTAT   (0x0A | 0x80)

#define MICMD_MIIRD  0x01
#define MISTAT_BUSY  0x01

// ── Registres Bank 3 ─────────────────────────────────────────
#define MAADR5   (0x00 | 0x80)
#define MAADR6   (0x01 | 0x80)
#define MAADR3   (0x02 | 0x80)
#define MAADR4   (0x03 | 0x80)
#define MAADR1   (0x04 | 0x80)
#define MAADR2   (0x05 | 0x80)
#define EREVID   0x12

// ── Registres PHY ─────────────────────────────────────────────
#define PHCON1   0x00
#define PHCON2   0x10
#define PHHID1   0x02
#define PHSTAT2  0x11

// ── Objets globaux ────────────────────────────────────────────
static spi_master_t   spi;
static digital_out_t  rst;
static uart_t         uart;
static uart_config_t  uart_cfg;
static uint8_t uart_rx_buffer[128];
static uint8_t uart_tx_buffer[128];

static const uint8_t local_mac[6] = { 0x02, 0xDE, 0xAD, 0xBE, 0xEF, 0x01 };

// ── SPI bas niveau ────────────────────────────────────────────
void enc_select_bank(uint8_t bank) {
    // Clear bits 0-1 de ECON1
    uint8_t clr[2] = { (uint8_t)(ENC28J60_BFC_CMD | (ECON1 & 0x1F)), 0x03 };
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, clr, 2);
    spi_master_deselect_device(MIKROBUS2_CS);
    // Set la banque voulue
    uint8_t set[2] = { (uint8_t)(ENC28J60_BFS_CMD | (ECON1 & 0x1F)), (uint8_t)(bank & 0x03) };
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, set, 2);
    spi_master_deselect_device(MIKROBUS2_CS);
}

uint8_t enc_read_reg(uint8_t reg) {
    // Si bit7 = 1 (registre MAC/MII), il faut lire 2 octets : le 1er est un dummy
    uint8_t cmd = ENC28J60_RCR_CMD | (reg & 0x1F);
    uint8_t data[2] = {0, 0};
    uint8_t read_len = (reg & 0x80) ? 2 : 1;
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write_then_read(&spi, &cmd, 1, data, read_len);
    spi_master_deselect_device(MIKROBUS2_CS);
    return data[read_len - 1];
}

void enc_write_reg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { (uint8_t)(ENC28J60_WCR_CMD | (reg & 0x1F)), value };
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, buf, 2);
    spi_master_deselect_device(MIKROBUS2_CS);
}

void enc_bfs(uint8_t reg, uint8_t mask) {
    // BFS : Bit Field Set — ne fonctionne que sur les registres ETH (pas MAC/MII)
    uint8_t buf[2] = { (uint8_t)(ENC28J60_BFS_CMD | (reg & 0x1F)), mask };
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, buf, 2);
    spi_master_deselect_device(MIKROBUS2_CS);
}

void enc_bfc(uint8_t reg, uint8_t mask) {
    // BFC : Bit Field Clear — ne fonctionne que sur les registres ETH (pas MAC/MII)
    uint8_t buf[2] = { (uint8_t)(ENC28J60_BFC_CMD | (reg & 0x1F)), mask };
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, buf, 2);
    spi_master_deselect_device(MIKROBUS2_CS);
}

void enc_soft_reset(void) {
    uint8_t cmd = ENC28J60_SRC_CMD;
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, &cmd, 1);
    spi_master_deselect_device(MIKROBUS2_CS);
    Delay_ms(2);
}

void enc_wait_clkready(void) {
    enc_select_bank(0);
    uint16_t tries = 0;
    while (!(enc_read_reg(ESTAT) & ESTAT_CLKRDY)) {
        Delay_ms(1);
        if (++tries > 200) break;
    }
}

// ── Accès MII (registres PHY) ─────────────────────────────────
// Les registres PHY passent par les registres MII de la banque 2.
// On attend que MISTAT.BUSY retombe avant/après l'opération.
void enc_phy_write(uint8_t phy_reg, uint16_t value) {
    enc_select_bank(2);
    enc_write_reg(MIREGADR & 0x1F, phy_reg);
    enc_write_reg(MIWRL    & 0x1F, (uint8_t)(value & 0xFF));
    enc_write_reg(MIWRH    & 0x1F, (uint8_t)(value >> 8));
    Delay_ms(15);
    enc_select_bank(2);   // ← ajout
    uint16_t tries = 0;
    while (enc_read_reg(MISTAT & 0x1F) & MISTAT_BUSY) {
        Delay_ms(1);
        if (++tries > 50) break;
    }
}

void enc_phy_read(uint8_t phy_reg, uint8_t *low, uint8_t *high) {
    enc_select_bank(2);
    enc_write_reg(MIREGADR & 0x1F, phy_reg);
    enc_write_reg(MICMD    & 0x1F, MICMD_MIIRD);
    Delay_ms(15);
    enc_select_bank(2);   // ← ajout
    uint16_t tries = 0;
    while (enc_read_reg(MISTAT & 0x1F) & MISTAT_BUSY) {
        Delay_ms(1);
        if (++tries > 50) break;
    }
    enc_write_reg(MICMD & 0x1F, 0x00);
    *low  = enc_read_reg(MIRDL & 0x1F);
    *high = enc_read_reg(MIRDH & 0x1F);
}

// ── UART helpers ──────────────────────────────────────────────
void mb1_print(const char *str) {
    while (*str) { uart_write(&uart, (uint8_t *)str, 1); str++; }
}

void mb1_print_hex(uint8_t val) {
    const char hex[] = "0123456789ABCDEF";
    char buf[5] = "0x";
    buf[2] = hex[val >> 4]; buf[3] = hex[val & 0x0F]; buf[4] = '\0';
    mb1_print(buf);
}

void mb1_print_hex16(uint16_t val) {
    mb1_print_hex((uint8_t)(val >> 8));
    mb1_print_hex((uint8_t)(val & 0xFF));
}

// ── Configuration ENC28J60 (MAC + buffers) ────────────────────
void enc_init(void) {
    enc_select_bank(0);

    // Buffer de réception : 0x0000 – 0x17FF
    enc_write_reg(ERXSTL, (uint8_t)(RXBUF_START & 0xFF));
    enc_write_reg(ERXSTH, (uint8_t)(RXBUF_START >> 8));
    enc_write_reg(ERXNDL, (uint8_t)(RXBUF_END & 0xFF));
    enc_write_reg(ERXNDH, (uint8_t)(RXBUF_END >> 8));

    // Workaround errata silicium : ERXRDPT doit valoir RXBUF_END et non RXBUF_START
    enc_write_reg(ERXRDPTL, (uint8_t)(RXBUF_END & 0xFF));
    enc_write_reg(ERXRDPTH, (uint8_t)(RXBUF_END >> 8));

    // Buffer de transmission : commence à 0x1800
    enc_write_reg(ETXSTL, (uint8_t)(TXBUF_START & 0xFF));
    enc_write_reg(ETXSTH, (uint8_t)(TXBUF_START >> 8));

    // Filtre réception : CRC + unicast + broadcast (0xA1)
    enc_select_bank(1);
    enc_write_reg(ERXFCON, 0xA1);

    // Configuration MAC (bank 2)
    enc_select_bank(2);
    // MACON1 : 0x05 = MARXEN (activer la réception) + TXPAUS/RXPAUS pour half duplex
    enc_write_reg(MACON1  & 0x1F, 0x05);
    // MACON3 : 0x30 = padding auto à 60 octets + CRC auto, half duplex
    enc_write_reg(MACON3  & 0x1F, 0x30);
    // MACON4 : 0x40 = DEFER (attendre si le medium est occupé, conforme IEEE 802.3)
    enc_write_reg(MACON4  & 0x1F, 0x40);
    // Inter-packet gap (valeurs datasheet pour half duplex)
    enc_write_reg(MABBIPG & 0x1F, 0x12);
    enc_write_reg(MAIPGL  & 0x1F, 0x12);
    enc_write_reg(MAIPGH  & 0x1F, 0x0C);
    // Taille max de trame : 1518 (0x05EE)
    enc_write_reg(MAMXFLL & 0x1F, 0xEE);
    enc_write_reg(MAMXFLH & 0x1F, 0x05);

    // Adresse MAC (bank 3)
    enc_select_bank(3);
    enc_write_reg(MAADR1 & 0x1F, local_mac[0]);
    enc_write_reg(MAADR2 & 0x1F, local_mac[1]);
    enc_write_reg(MAADR3 & 0x1F, local_mac[2]);
    enc_write_reg(MAADR4 & 0x1F, local_mac[3]);
    enc_write_reg(MAADR5 & 0x1F, local_mac[4]);
    enc_write_reg(MAADR6 & 0x1F, local_mac[5]);

    // Activer la réception
    enc_select_bank(0);
    enc_bfs(ECON1, ECON1_RXEN);
}

// ── Configuration PHY ─────────────────────────────────────────
// PAS d'auto-négociation : errata connu ENC28J60.
// On force le half duplex, on désactive le loopback interne.
void enc_phy_init(void) {
    uint8_t low, high;

    mb1_print("PHY RESET (hard via PHCON1.PRST)...");
    // PHCON1 bit15 = PRST (reset PHY)
    enc_phy_write(PHCON1, 0x8000);
    Delay_ms(100);
    mb1_print(" OK\r\n");

    // PHCON1 = 0x0000 : half duplex, pas d'auto-neg, pas de loopback
    mb1_print("PHCON1 = half duplex, no auto-neg...");
    enc_phy_write(PHCON1, 0x0000);
    Delay_ms(50);
    mb1_print(" OK\r\n");

    // PHCON2 bit8 = HDLDCON (désactiver le loopback half duplex interne)
    mb1_print("PHCON2 = disable HD loopback...");
    enc_phy_write(PHCON2, 0x0100);
    Delay_ms(50);
    mb1_print(" OK\r\n");

    mb1_print("READ PHCON1 = ");
    enc_phy_read(PHCON1, &low, &high);
    mb1_print_hex(high); mb1_print_hex(low); mb1_print("\r\n");
}

// ── Vérification du lien ──────────────────────────────────────
// PHSTAT2 bit10 (bit2 de l'octet haut) = LSTAT : 1 si lien actif
uint8_t enc_check_link(void) {
    uint8_t low, high;
    enc_phy_read(PHSTAT2, &low, &high);
    return (high & 0x04) ? 1 : 0;
}

// ── Main ──────────────────────────────────────────────────────
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
    mb1_print("=== ENC28J60 INIT ===\r\n");

    // Reset matériel via broche RST
    digital_out_init(&rst, MIKROBUS2_RST);
    mb1_print("HW RESET...");
    digital_out_high(&rst); Delay_ms(10);
    digital_out_low(&rst);  Delay_ms(10);
    digital_out_high(&rst); Delay_ms(100);
    mb1_print(" OK\r\n");

    // SPI à 1 MHz, mode 0
    mb1_print("SPI INIT...");
    spi_master_config_t spi_cfg;
    spi_master_configure_default(&spi_cfg);
    spi_cfg.sck   = MIKROBUS2_SCK;
    spi_cfg.miso  = MIKROBUS2_MISO;
    spi_cfg.mosi  = MIKROBUS2_MOSI;
    spi_cfg.speed = 1000000;
    spi_cfg.mode  = SPI_MASTER_MODE_0;
    spi_master_open(&spi, &spi_cfg);
    spi_master_set_chip_select_polarity(SPI_MASTER_CHIP_SELECT_DEFAULT_POLARITY);
    spi_master_deselect_device(MIKROBUS2_CS);
    mb1_print(" OK\r\n");

    // Reset logiciel
    mb1_print("SW RESET...");
    enc_soft_reset();
    Delay_ms(100);
    mb1_print(" OK\r\n");

    // Attendre que l'oscillateur interne soit prêt
    mb1_print("WAIT CLKRDY...");
    enc_wait_clkready();
    Delay_ms(500);
    mb1_print(" OK\r\n");

    // Lire la révision du chip (utile pour identifier les workarounds nécessaires)
    enc_select_bank(3);
    uint8_t chip_rev = enc_read_reg(EREVID);
    mb1_print("EREVID = ");
    mb1_print_hex(chip_rev);
    // B5 = 0x05, B7 = 0x07 → nécessitent des workarounds TX
    if (chip_rev == 0x05 || chip_rev == 0x07) {
        mb1_print(" (rev B5/B7 — workarounds TX actifs)\r\n");
    } else {
        mb1_print("\r\n");
    }

    // Configuration MAC + buffers RX/TX
    mb1_print("MAC INIT...");
    enc_init();
    mb1_print(" OK\r\n");

    // Configuration PHY (half duplex, pas d'auto-neg)
    enc_phy_init();

    // Lecture PHY ID pour valider la communication MII
    uint8_t low, high;
    mb1_print("PHHID1 = ");
    enc_phy_read(PHHID1, &low, &high);
    mb1_print_hex(high); mb1_print_hex(low);
    mb1_print(" (attendu 0x0083)\r\n");  // ID fixe du ENC28J60

    enc_phy_read(PHSTAT2, &low, &high);
    mb1_print("PHSTAT2 = "); mb1_print_hex(high); mb1_print_hex(low); mb1_print("\r\n");
    // Bit 10 (0x0400) = LSTAT

    // Attente de lien (10 secondes max)
    mb1_print("\r\nWAIT LINK (10s max)...\r\n");
    uint8_t link_ok = 0;
    for (int i = 0; i < 100; i++) {
        if (enc_check_link()) {
            link_ok = 1;
            mb1_print(">>> LINK UP\r\n");
            break;
        }
        mb1_print(".");
        if ((i + 1) % 10 == 0) mb1_print("\r\n");
        Delay_ms(100);
    }
    if (!link_ok) mb1_print("\r\n>>> LINK DOWN (pas de cable ?)\r\n");

    // Boucle de monitoring
    mb1_print("\r\n=== MONITORING ===\r\n");
    while (1) {
        uint8_t link = enc_check_link();
        mb1_print(link ? "Link: UP\r\n" : "Link: DOWN\r\n");
        Delay_ms(1000);
    }

    return 0;
}