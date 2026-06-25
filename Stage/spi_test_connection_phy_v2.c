#ifdef PREINIT_SUPPORTED
#include "preinit.h"
#endif

#include "MikroSDK.Driver.SPI.Master"
#include "MikroSDK.Driver.GPIO.Out"
#include "MikroSDK.Driver.UART"
#include "MikroSDK.Board"

// ── Pins Mikrobus 1 sur UNI-DS v8 ──────────────────────────
#define MIKROBUS2_SCK   GPIO_PA5
#define MIKROBUS2_MISO  GPIO_PA6
#define MIKROBUS2_MOSI  GPIO_PB5
#define MIKROBUS2_CS    GPIO_PA4
#define MIKROBUS2_RST   GPIO_PE11

// ── Commandes SPI ENC28J60 ──────────────────────────────────
#define ENC28J60_RCR_CMD  0x00
#define ENC28J60_WCR_CMD  0x40
#define ENC28J60_BFS_CMD  0x80
#define ENC28J60_BFC_CMD  0xA0
#define ENC28J60_SRC_CMD  0xFF
#define ENC28J60_RBM_CMD 0x3A
#define ENC28J60_WBM_CMD 0x7A

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
#define ESTAT_CLKRDY 0x01
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
#define MISTAT   (0x0A|0x80)
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
    uint8_t data[2] = {0, 0};
    uint8_t read_len = (reg & 0x80) ? 2 : 1; // 2 = dummy + data pour MAC/MII
    
    cs_low();
    spi_master_write_then_read(&spi, &cmd, 1, data, read_len);
    cs_high();
    
    return data[read_len - 1];
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

    enc_write_reg(MIREGADR & 0x1F, phy_reg);  // MIREGADR = 0x14

    // SET le bit MIIRD via BFS (pas une écriture directe !)
    enc_bfs(MICMD & 0x1F, 0x01);              // MICMD = 0x12, MIIRD = 0x01

    Delay_ms(11);  // datasheet : 10.24 µs minimum, on prend 11ms pour être large

    // CLEAR MIIRD via BFC
    uint8_t bfc[2] = { ENC28J60_BFC_CMD | (MICMD & 0x1F), 0x01 };
    cs_low(); spi_master_write(&spi, bfc, 2); cs_high();

    // Lire les résultats (toujours en bank 2)
    *low  = enc_read_reg(MIRDL);
    *high = enc_read_reg(MIRDH);

    return 1;
}

// Ecriture d'un registre PHY (16 bits) : pas besoin de MICMD, l'écriture
// de MIWRH déclenche automatiquement le transfert vers le PHY.
void enc_phy_write(uint8_t phy_reg, uint16_t value) {
    enc_select_bank(2);
    enc_write_reg(MIREGADR & 0x1F, phy_reg);
    Delay_ms(1);
    enc_write_reg(MIWRL & 0x1F, (uint8_t)(value & 0xFF));
    Delay_ms(1);
    enc_write_reg(MIWRH & 0x1F, (uint8_t)(value >> 8));
    Delay_ms(15);  // attendre fin transfert PHY
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
   // reset PHY
    enc_phy_write(PHCON1, 0x8000);
    Delay_ms(50);

    // clear reset
    enc_phy_write(PHCON1, 0x0000);
    Delay_ms(50);

    // Activer la réception au niveau MAC/ECON1
    enc_bfs(ECON1, ECON1_RXEN);
}

void enc_wait_link(void)
{
    uint8_t low, high;

    for (int i = 0; i < 50; i++) {
        enc_phy_read(PHSTAT2, &low, &high);

        uint16_t phstat2 = ((uint16_t)high << 8) | low;

        if (phstat2 & 0x0400) {
            return;
        }

        Delay_ms(200);
    }
}

// ── Main ─────────────────────────────────────────────────────
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
    mb1_print("UART OK \r\n");

    // CS / RESET
    digital_out_init(&cs,  MIKROBUS2_CS); 
    digital_out_init(&rst, MIKROBUS2_RST);
    cs_high();

    digital_out_high(&rst); Delay_ms(10);
    digital_out_low(&rst);  Delay_ms(10);
    digital_out_high(&rst); Delay_ms(10);

    mb1_print("Reset OK\r\n");

    // SPI
    spi_master_config_t spi_cfg;
    spi_master_configure_default(&spi_cfg);
    spi_cfg.sck   = MIKROBUS2_SCK;
    spi_cfg.miso  = MIKROBUS2_MISO;
    spi_cfg.mosi  = MIKROBUS2_MOSI;
    spi_cfg.speed = 1000000;
    spi_cfg.mode  = SPI_MASTER_MODE_0;

    spi_master_open(&spi, &spi_cfg);
    spi_master_set_chip_select_polarity(SPI_MASTER_CHIP_SELECT_DEFAULT_POLARITY);

    mb1_print("SPI OK\r\n");

    // ENC reset + clock ready
    enc_soft_reset();
    Delay_ms(100);
    enc_wait_clkready();
    Delay_ms(100);

    /* 
    // TEST DIAGNOSTIC : lire MIRDL brut, avec et sans dummy byte
    uint8_t cmd, d1, d2;

    // Test 1 : lire adresse 0x18 SANS dummy (comme un registre ETH normal)
    cmd = 0x18;  // RCR | 0x18
    cs_low();
    spi_master_write_then_read(&spi, &cmd, 1, &d1, 1);
    cs_high();
    mb1_print("MIRDL sans dummy="); mb1_print_hex(d1); mb1_print("\r\n");

    // Test 2 : lire adresse 0x18 AVEC dummy (registre MAC/MII)
    uint8_t rx2[2] = {0,0};
    cmd = 0x18;
    cs_low();
    spi_master_write_then_read(&spi, &cmd, 1, rx2, 2);
    cs_high();
    mb1_print("MIRDL avec dummy="); mb1_print_hex(rx2[1]); mb1_print("\r\n");

    // Test 3 : lire ESTAT (0x1D) sans dummy - doit retourner CLKRDY=1 soit 0x01
    cmd = 0x1D;
    cs_low();
    spi_master_write_then_read(&spi, &cmd, 1, &d1, 1);
    cs_high();
    mb1_print("ESTAT="); mb1_print_hex(d1); mb1_print("\r\n");

    // ── DIAGNOSTIC MII PAS À PAS ──────────────────────────
    uint8_t cmd2, rb;

    // 1. Forcer bank 2 manuellement (BFC puis BFS sur ECON1)
    uint8_t t[2];
    t[0] = 0xA0 | 0x1F; t[1] = 0x03;  // BFC ECON1, clear bits 0+1
    cs_low(); spi_master_write(&spi, t, 2); cs_high();
    t[0] = 0x80 | 0x1F; t[1] = 0x02;  // BFS ECON1, set bit 1 → bank 2
    cs_low(); spi_master_write(&spi, t, 2); cs_high();
    mb1_print("Bank2 selectionne\r\n");

    cmd2 = 0x1F;  // ECON1, bank-independent
    cs_low(); spi_master_write_then_read(&spi, &cmd2, 1, &rb, 1); cs_high();
    mb1_print("ECON1="); mb1_print_hex(rb); mb1_print("\r\n");

    // 2. Ecrire MIREGADR = 0x02 (PHHID1)
    t[0] = 0x40 | 0x14; t[1] = 0x02;  // WCR MIREGADR, valeur = registre PHY PHHID1
    cs_low(); spi_master_write(&spi, t, 2); cs_high();
    mb1_print("MIREGADR ecrit\r\n");

    // 3. Relire MIREGADR avec dummy byte
    uint8_t rx_mir[2] = {0, 0};
    cmd2 = 0x14;
    cs_low(); spi_master_write_then_read(&spi, &cmd2, 1, rx_mir, 2); cs_high();
    mb1_print("MIREGADR relu AVEC dummy="); mb1_print_hex(rx_mir[1]); mb1_print("\r\n");

    // Relire aussi SANS dummy pour comparer
    cs_low(); spi_master_write_then_read(&spi, &cmd2, 1, &rb, 1); cs_high();
    mb1_print("MIREGADR relu SANS dummy="); mb1_print_hex(rb); mb1_print("\r\n");

    // 4. SET MIIRD dans MICMD via BFS
    t[0] = 0x80 | 0x12; t[1] = 0x01;  // BFS MICMD, set bit 0 (MIIRD)
    cs_low(); spi_master_write(&spi, t, 2); cs_high();
    mb1_print("MIIRD set\r\n");

    // 5. Relire MICMD pour vérifier
    cmd2 = 0x12;
    cs_low(); spi_master_write_then_read(&spi, &cmd2, 1, &rb, 1); cs_high();
    mb1_print("MICMD apres BFS="); mb1_print_hex(rb); mb1_print("\r\n");

    // 6. Attendre
    Delay_ms(50);

    // 7. CLEAR MIIRD
    t[0] = 0xA0 | 0x12; t[1] = 0x01;  // BFC MICMD
    cs_low(); spi_master_write(&spi, t, 2); cs_high();

    // 8. Lire MIRDL et MIRDH avec dummy byte
    uint8_t rx[2];
    cmd2 = 0x18;  // MIRDL
    cs_low(); spi_master_write_then_read(&spi, &cmd2, 1, rx, 2); cs_high();
    mb1_print("MIRDL(dummy)="); mb1_print_hex(rx[1]); mb1_print("\r\n");

    cmd2 = 0x19;  // MIRDH
    cs_low(); spi_master_write_then_read(&spi, &cmd2, 1, rx, 2); cs_high();
    mb1_print("MIRDH(dummy)="); mb1_print_hex(rx[1]); mb1_print("\r\n");
    */

    uint8_t v;

    v = enc_read_reg(MICMD);
    mb1_print("MICMD=");
    mb1_print_hex(v);
    mb1_print("\r\n");

    v = enc_read_reg(MIREGADR);
    mb1_print("MIREGADR=");
    mb1_print_hex(v);
    mb1_print("\r\n");

    mb1_print("CLKRDY OK\r\n");

    // REV check
    enc_select_bank(3);
    uint8_t rev = enc_read_reg(EREVID);
    mb1_print("EREVID = ");
    mb1_print_hex(rev);
    mb1_print("\r\n");

    // Ethernet config MAC
    enc_setup_ethernet();

    // ── PHY FIX (CORRIGÉ) ────────────────────────────────
    uint8_t low, high;

    // 1. RESET PHY (vrai reset hardware PHY)
    enc_phy_write(PHCON1, 0x8000);
    Delay_ms(50);

    // 2. CLEAR reset
    enc_phy_write(PHCON1, 0x0000);
    Delay_ms(50);

    // 3. Restart auto-negociation
    enc_phy_write(PHCON1, 0x1200);
    Delay_ms(200);

    // 4. debug PHY ID (IMPORTANT pour vérifier que MII marche)
    enc_phy_read(PHHID1, &low, &high);
    mb1_print("PHHID1=");
    mb1_print_hex(high);
    mb1_print_hex(low);
    mb1_print("\r\n");

    // 5. wait link
    int timeout = 50;

    while (timeout--) {
        enc_phy_read(PHSTAT2, &low, &high);

        uint16_t phstat2 = ((uint16_t)high << 8) | low;

        if (phstat2 & 0x0400) {
            break;
        }

        Delay_ms(100);
}

    // debug PHY registers
    enc_phy_read(PHCON1, &low, &high);
    mb1_print("PHCON1=");
    mb1_print_hex(high);
    mb1_print_hex(low);
    mb1_print("\r\n");

    enc_phy_read(PHSTAT2, &low, &high);
    mb1_print("PHSTAT2=");
    mb1_print_hex(high);
    mb1_print_hex(low);
    mb1_print("\r\n");

    // LINK status final
    if (high & 0x04) {
        mb1_print(">>> LINK UP - cable Ethernet detecte !\r\n");
    } else {
        mb1_print(">>> LINK DOWN - verifier le cable RJ45\r\n");
    }

    // loop
    while (1) {
        enc_phy_read(PHSTAT2, &low, &high);

        mb1_print("Link = ");
        uint16_t phstat2 = ((uint16_t)high << 8) | low;

        mb1_print((phstat2 & 0x0400) ? "UP\r\n" : "DOWN\r\n");

        Delay_ms(1000);
    }

    return 0;
}