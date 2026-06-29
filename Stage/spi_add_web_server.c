#ifdef PREINIT_SUPPORTED
#include "preinit.h"
#endif

#include "MikroSDK.Driver.SPI.Master"
#include "MikroSDK.Driver.GPIO.Out"
#include "MikroSDK.Driver.UART"
#include "MikroSDK.Board"
#include <string.h>

// ── Pins Mikrobus 2 sur UNI-DS v8 ──────────────────────────
#define MIKROBUS2_SCK   GPIO_PA5
#define MIKROBUS2_MISO  GPIO_PA6
#define MIKROBUS2_MOSI  GPIO_PB5
#define MIKROBUS2_CS    GPIO_PB2
#define MIKROBUS2_RST   GPIO_PE12

// ── Commandes SPI ENC28J60 ──────────────────────────────────────────────────
#define ENC28J60_RCR_CMD  0x00
#define ENC28J60_WCR_CMD  0x40
#define ENC28J60_BFS_CMD  0x80
#define ENC28J60_BFC_CMD  0xA0
#define ENC28J60_SRC_CMD  0xFF
#define ENC28J60_RBM_CMD  0x3A
#define ENC28J60_WBM_CMD  0x7A

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

// ── TCP flags ────────────────────────────────────────────────
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_ACK 0x10

// ── Objets globaux ────────────────────────────────────────────
static spi_master_t   spi;
static digital_out_t  rst;
static uart_t         uart;
static uart_config_t  uart_cfg;
static uint8_t uart_rx_buffer[128];
static uint8_t uart_tx_buffer[128];

// Adresse MAC locale
static const uint8_t local_mac[6] = { 0x02, 0xDE, 0xAD, 0xBE, 0xEF, 0x01 };

// IP statique sur le même réseau que le PC (172.20.22.x/24)
static const uint8_t local_ip[4]  = { 172, 20, 22, 200 };

// Pointeur paquet RX (workaround errata ENC28J60)
static uint16_t next_pkt_ptr = RXBUF_START;

// ── SPI bas niveau ────────────────────────────────────────────
void enc_select_bank(uint8_t bank) {
    uint8_t clr[2] = { (uint8_t)(ENC28J60_BFC_CMD | (ECON1 & 0x1F)), 0x03 };
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, clr, 2);
    spi_master_deselect_device(MIKROBUS2_CS);

    uint8_t set[2] = { (uint8_t)(ENC28J60_BFS_CMD | (ECON1 & 0x1F)), (uint8_t)(bank & 0x03) };
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, set, 2);
    spi_master_deselect_device(MIKROBUS2_CS);
}

uint8_t enc_read_reg(uint8_t reg) {
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
    uint8_t buf[2] = { (uint8_t)(ENC28J60_BFS_CMD | (reg & 0x1F)), mask };
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, buf, 2);
    spi_master_deselect_device(MIKROBUS2_CS);
}

void enc_bfc(uint8_t reg, uint8_t mask) {
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
void enc_phy_write(uint8_t phy_reg, uint16_t value) {
    enc_select_bank(2);
    enc_write_reg(MIREGADR & 0x1F, phy_reg);
    enc_write_reg(MIWRL    & 0x1F, (uint8_t)(value & 0xFF));
    enc_write_reg(MIWRH    & 0x1F, (uint8_t)(value >> 8));
    Delay_ms(15);
    enc_select_bank(2);
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
    enc_select_bank(2);
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

    enc_write_reg(ERXSTL,   (uint8_t)(RXBUF_START & 0xFF));
    enc_write_reg(ERXSTH,   (uint8_t)(RXBUF_START >> 8));
    enc_write_reg(ERXNDL,   (uint8_t)(RXBUF_END & 0xFF));
    enc_write_reg(ERXNDH,   (uint8_t)(RXBUF_END >> 8));

    // Workaround errata : ERXRDPT doit valoir RXBUF_END
    enc_write_reg(ERXRDPTL, (uint8_t)(RXBUF_END & 0xFF));
    enc_write_reg(ERXRDPTH, (uint8_t)(RXBUF_END >> 8));

    enc_write_reg(ETXSTL,   (uint8_t)(TXBUF_START & 0xFF));
    enc_write_reg(ETXSTH,   (uint8_t)(TXBUF_START >> 8));

    // Filtre réception : CRC + unicast + broadcast
    enc_select_bank(1);
    enc_write_reg(ERXFCON, 0xA1);

    // Configuration MAC (bank 2)
    enc_select_bank(2);
    enc_write_reg(MACON1  & 0x1F, 0x05); // MARXEN + RXPAUS/TXPAUS
    enc_write_reg(MACON3  & 0x1F, 0x30); // padding 60B + CRC auto, half duplex
    enc_write_reg(MACON4  & 0x1F, 0x40); // DEFER (IEEE 802.3)
    enc_write_reg(MABBIPG & 0x1F, 0x12); // IPG half duplex
    enc_write_reg(MAIPGL  & 0x1F, 0x12);
    enc_write_reg(MAIPGH  & 0x1F, 0x0C);
    enc_write_reg(MAMXFLL & 0x1F, 0xEE); // max frame 1518
    enc_write_reg(MAMXFLH & 0x1F, 0x05);

    // Adresse MAC (bank 3)
    enc_select_bank(3);
    enc_write_reg(MAADR1 & 0x1F, local_mac[0]);
    enc_write_reg(MAADR2 & 0x1F, local_mac[1]);
    enc_write_reg(MAADR3 & 0x1F, local_mac[2]);
    enc_write_reg(MAADR4 & 0x1F, local_mac[3]);
    enc_write_reg(MAADR5 & 0x1F, local_mac[4]);
    enc_write_reg(MAADR6 & 0x1F, local_mac[5]);

    enc_select_bank(0);
    enc_bfs(ECON1, ECON1_RXEN);
}

// ── Configuration PHY ─────────────────────────────────────────
void enc_phy_init(void) {
    uint8_t low, high;

    mb1_print("PHY RESET...");
    enc_phy_write(PHCON1, 0x8000);
    Delay_ms(100);
    mb1_print(" OK\r\n");

    mb1_print("PHCON1 = half duplex, no auto-neg...");
    enc_phy_write(PHCON1, 0x0000);
    Delay_ms(50);
    mb1_print(" OK\r\n");

    mb1_print("PHCON2 = disable HD loopback...");
    enc_phy_write(PHCON2, 0x0100);
    Delay_ms(50);
    mb1_print(" OK\r\n");

    mb1_print("READ PHCON1 = ");
    enc_phy_read(PHCON1, &low, &high);
    mb1_print_hex(high); mb1_print_hex(low); mb1_print("\r\n");
}

// ── Vérification du lien ──────────────────────────────────────
uint8_t enc_check_link(void) {
    uint8_t low, high;
    enc_phy_read(PHSTAT2, &low, &high);
    return (high & 0x04) ? 1 : 0;
}

// ── Lecture/écriture buffer mémoire ENC28J60 ─────────────────

void enc_write_buf(uint16_t addr, const uint8_t *data, uint16_t len) {
    enc_select_bank(0);
    enc_write_reg(EWRPTL, addr & 0xFF);
    enc_write_reg(EWRPTH, addr >> 8);
    uint8_t cmd = ENC28J60_WBM_CMD;
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write(&spi, &cmd, 1);
    spi_master_write(&spi, (uint8_t*)data, len);
    spi_master_deselect_device(MIKROBUS2_CS);
}

void enc_read_buf(uint16_t addr, uint8_t *data, uint16_t len) {
    enc_select_bank(0);
    enc_write_reg(ERDPTL, addr & 0xFF);
    enc_write_reg(ERDPTH, addr >> 8);
    uint8_t cmd = ENC28J60_RBM_CMD;
    spi_master_select_device(MIKROBUS2_CS);
    spi_master_write_then_read(&spi, &cmd, 1, data, len);
    spi_master_deselect_device(MIKROBUS2_CS);
}

// ── Envoi d'un paquet Ethernet ────────────────────────────────
void enc_send_packet(const uint8_t *buf, uint16_t len) {
    // Workaround errata B5/B7 : reset TX avant envoi
    enc_bfs(ECON1, ECON1_TXRST);
    enc_bfc(ECON1, ECON1_TXRST);
    enc_bfc(EIR, EIR_TXERIF | EIR_TXIF);

    enc_select_bank(0);
    enc_write_reg(ETXSTL, TXBUF_START & 0xFF);
    enc_write_reg(ETXSTH, TXBUF_START >> 8);

    // +1 pour le control byte, donc end = start + 1 + len - 1
    uint16_t txnd = TXBUF_START + len;
    enc_write_reg(ETXNDL, txnd & 0xFF);
    enc_write_reg(ETXNDH, txnd >> 8);

    // Control byte 0x00 à TXBUF_START, puis les données
    uint8_t ctrl = 0x00;
    enc_write_buf(TXBUF_START,     &ctrl, 1);
    enc_write_buf(TXBUF_START + 1, buf,   len);

    enc_bfs(ECON1, ECON1_TXRTS);

    uint16_t tries = 0;
    while (enc_read_reg(ECON1) & ECON1_TXRTS) {
        Delay_ms(1);
        if (++tries > 200) break;
    }
}

// ── Réception d'un paquet ─────────────────────────────────────
uint16_t enc_recv_packet(uint8_t *buf, uint16_t maxlen) {
    enc_select_bank(1);
    uint8_t pktcnt = enc_read_reg(EPKTCNT);
    if (pktcnt == 0) return 0;

    // Header RSV : 2 octets next ptr + 2 octets longueur + 2 octets status
    uint8_t header[6];
    enc_read_buf(next_pkt_ptr, header, 6);

    uint16_t next   = header[0] | ((uint16_t)header[1] << 8);
    uint16_t rxlen  = header[2] | ((uint16_t)header[3] << 8);

    rxlen -= 4; // Enlever les 4 octets CRC
    if (rxlen > maxlen) rxlen = maxlen;

    enc_read_buf(next_pkt_ptr + 6, buf, rxlen);

    // Avancer le pointeur (workaround errata : valeur impaire)
    next_pkt_ptr = next;
    uint16_t erxrdpt = (next == RXBUF_START) ? RXBUF_END : next - 1;
    enc_select_bank(0);
    enc_write_reg(ERXRDPTL, erxrdpt & 0xFF);
    enc_write_reg(ERXRDPTH, erxrdpt >> 8);

    enc_bfs(ECON2, ECON2_PKTDEC);

    return rxlen;
}

// ── Checksum IP ───────────────────────────────────────────────
uint16_t ip_checksum(const uint8_t *data, uint16_t len) {
    uint32_t sum = 0;
    for (uint16_t i = 0; i + 1 < len; i += 2)
        sum += ((uint32_t)data[i] << 8) | data[i+1];
    if (len & 1) sum += (uint32_t)data[len-1] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

// ── Checksum TCP (avec pseudo-header) ────────────────────────
uint16_t tcp_checksum(const uint8_t *src_ip, const uint8_t *dst_ip,
                      const uint8_t *tcp_seg, uint16_t tcp_len) {
    uint8_t pseudo[12];
    memcpy(&pseudo[0], src_ip, 4);
    memcpy(&pseudo[4], dst_ip, 4);
    pseudo[8]  = 0;
    pseudo[9]  = 6; // protocole TCP
    pseudo[10] = tcp_len >> 8;
    pseudo[11] = tcp_len & 0xFF;

    uint32_t sum = 0;
    for (int i = 0; i + 1 < 12; i += 2)
        sum += ((uint32_t)pseudo[i] << 8) | pseudo[i+1];
    for (uint16_t i = 0; i + 1 < tcp_len; i += 2)
        sum += ((uint32_t)tcp_seg[i] << 8) | tcp_seg[i+1];
    if (tcp_len & 1) sum += (uint32_t)tcp_seg[tcp_len-1] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

// ── Envoi d'un segment TCP ────────────────────────────────────
void send_tcp(const uint8_t *dst_mac, const uint8_t *dst_ip,
              uint16_t src_port, uint16_t dst_port,
              uint32_t seq, uint32_t ack_num,
              uint8_t flags,
              const uint8_t *payload, uint16_t payload_len) {

    uint16_t tcp_len   = 20 + payload_len;
    uint16_t ip_len    = 20 + tcp_len;
    uint16_t total_len = 14 + ip_len;

    // Buffer statique (évite la stack overflow sur STM32)
    static uint8_t pkt[700];
    memset(pkt, 0, total_len);

    // ── Ethernet header ──
    memcpy(&pkt[0], dst_mac,   6);
    memcpy(&pkt[6], local_mac, 6);
    pkt[12] = 0x08; pkt[13] = 0x00; // IPv4

    // ── IP header ──
    pkt[14] = 0x45;                   // version=4, IHL=5
    pkt[15] = 0x00;                   // DSCP/ECN
    pkt[16] = ip_len >> 8;
    pkt[17] = ip_len & 0xFF;
    pkt[18] = 0x00; pkt[19] = 0x01;  // ID
    pkt[20] = 0x40; pkt[21] = 0x00;  // DF, no fragment
    pkt[22] = 64;                     // TTL
    pkt[23] = 6;                      // protocol TCP
    pkt[24] = 0; pkt[25] = 0;        // checksum (calculé après)
    memcpy(&pkt[26], local_ip, 4);
    memcpy(&pkt[30], dst_ip,   4);

    uint16_t ip_cksum = ip_checksum(&pkt[14], 20);
    pkt[24] = ip_cksum >> 8;
    pkt[25] = ip_cksum & 0xFF;

    // ── TCP header ──
    pkt[34] = src_port >> 8;  pkt[35] = src_port & 0xFF;
    pkt[36] = dst_port >> 8;  pkt[37] = dst_port & 0xFF;
    pkt[38] = seq >> 24;      pkt[39] = seq >> 16;
    pkt[40] = seq >> 8;       pkt[41] = seq & 0xFF;
    pkt[42] = ack_num >> 24;  pkt[43] = ack_num >> 16;
    pkt[44] = ack_num >> 8;   pkt[45] = ack_num & 0xFF;
    pkt[46] = 0x50;            // data offset = 5 words = 20 octets
    pkt[47] = flags;
    pkt[48] = 0x20; pkt[49] = 0x00; // window = 8192
    // pkt[50]/[51] = checksum (calculé après)
    // pkt[52]/[53] = urgent pointer = 0

    if (payload && payload_len)
        memcpy(&pkt[54], payload, payload_len);

    uint16_t tcp_cksum = tcp_checksum(&pkt[26], &pkt[30], &pkt[34], tcp_len);
    pkt[50] = tcp_cksum >> 8;
    pkt[51] = tcp_cksum & 0xFF;

    enc_send_packet(pkt, total_len);
}

// ── Réponse ARP ───────────────────────────────────────────────
void handle_arp(uint8_t *pkt, uint16_t len) {
    if (len < 42) return;
    // op = pkt[20]/[21] : 0x0001 = request
    if (pkt[20] != 0x00 || pkt[21] != 0x01) return;
    // Target IP = pkt[38..41]
    if (memcmp(&pkt[38], local_ip, 4) != 0) return;

    uint8_t reply[42];
    // Ethernet
    memcpy(&reply[0], &pkt[6], 6);   // dst = adresse MAC de l'expéditeur
    memcpy(&reply[6], local_mac, 6); // src = notre MAC
    reply[12] = 0x08; reply[13] = 0x06; // ARP

    // ARP payload
    reply[14] = 0x00; reply[15] = 0x01; // HW type = Ethernet
    reply[16] = 0x08; reply[17] = 0x00; // protocole = IPv4
    reply[18] = 6;    reply[19] = 4;    // tailles adresses
    reply[20] = 0x00; reply[21] = 0x02; // op = reply

    memcpy(&reply[22], local_mac, 6);  // sender MAC = nous
    memcpy(&reply[28], local_ip, 4);   // sender IP  = nous
    memcpy(&reply[32], &pkt[22], 6);   // target MAC = expéditeur
    memcpy(&reply[36], &pkt[28], 4);   // target IP  = expéditeur

    enc_send_packet(reply, 42);
    mb1_print("ARP reply sent\r\n");
}

// ── Réponse HTTP ──────────────────────────────────────────────
static const char http_response[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html><html><body>"
    "<h1>Hello from ENC28J60!</h1>"
    "<p>STM32F429ZIT6 - UNI-DS v8</p>"
    "<p>IP: 172.20.22.200</p>"
    "</body></html>\r\n";

// ── ICMP Echo Reply (ping) ────────────────────────────────────
void handle_icmp(uint8_t *pkt, uint16_t len) {
    uint8_t ihl     = (pkt[14] & 0x0F) * 4;
    uint16_t ip_base = 14;
    uint16_t icmp_base = ip_base + ihl;

    // Type 8 = Echo Request
    if (pkt[icmp_base] != 8) return;

    uint16_t ip_total = ((uint16_t)pkt[16] << 8) | pkt[17];
    uint16_t icmp_len = ip_total - ihl;

    static uint8_t reply[600];
    uint16_t total = 14 + 20 + icmp_len;
    memset(reply, 0, total);

    // Ethernet
    memcpy(&reply[0], &pkt[6],  6);
    memcpy(&reply[6], local_mac, 6);
    reply[12] = 0x08; reply[13] = 0x00;

    // IP (copier header source, swap src/dst)
    memcpy(&reply[14], &pkt[14], 20);
    reply[22] = 64; // TTL
    reply[23] = 1;  // ICMP
    memcpy(&reply[26], local_ip, 4);   // src = nous
    memcpy(&reply[30], &pkt[26], 4);   // dst = expéditeur
    reply[24] = 0; reply[25] = 0;
    uint16_t ip_ck = ip_checksum(&reply[14], 20);
    reply[24] = ip_ck >> 8; reply[25] = ip_ck & 0xFF;

    // ICMP : copier le payload, changer type en 0 (Echo Reply)
    memcpy(&reply[icmp_base], &pkt[icmp_base], icmp_len);
    reply[icmp_base] = 0; // type = Echo Reply
    reply[icmp_base+1] = 0; // code = 0
    reply[icmp_base+2] = 0; reply[icmp_base+3] = 0; // checksum reset
    uint16_t icmp_ck = ip_checksum(&reply[icmp_base], icmp_len);
    reply[icmp_base+2] = icmp_ck >> 8;
    reply[icmp_base+3] = icmp_ck & 0xFF;

    enc_send_packet(reply, total);
    mb1_print("ICMP Echo Reply sent\r\n");
}

// ── Traitement TCP/HTTP ───────────────────────────────────────
void handle_ip(uint8_t *pkt, uint16_t len) {
    if (len < 34) return;
    if (memcmp(&pkt[30], local_ip, 4) != 0) return;

    if (pkt[23] == 1) { handle_icmp(pkt, len); return; }
    if (pkt[23] != 6) return;                         // TCP seulement
    if (memcmp(&pkt[30], local_ip, 4) != 0) return;  // pas pour nous

    uint8_t  ihl      = (pkt[14] & 0x0F) * 4;
    uint16_t tcp_base = 14 + ihl;

    if (len < tcp_base + 20) return;

    uint16_t src_port = ((uint16_t)pkt[tcp_base]   << 8) | pkt[tcp_base+1];
    uint16_t dst_port = ((uint16_t)pkt[tcp_base+2] << 8) | pkt[tcp_base+3];

    if (dst_port != 80) return; // on n'écoute que le port 80

    uint32_t seq = ((uint32_t)pkt[tcp_base+4] << 24) |
                   ((uint32_t)pkt[tcp_base+5] << 16) |
                   ((uint32_t)pkt[tcp_base+6] <<  8) |
                              pkt[tcp_base+7];

    uint8_t  tcp_hlen       = (pkt[tcp_base+12] >> 4) * 4;
    uint8_t  flags          = pkt[tcp_base+13];
    uint16_t total_ip_len   = ((uint16_t)pkt[16] << 8) | pkt[17];
    uint16_t tcp_payload_len = total_ip_len - ihl - tcp_hlen;

    uint8_t *src_ip  = &pkt[26];
    uint8_t *src_mac = &pkt[6];

    // SEQ initial fixe (suffisant pour un serveur simple)
    static uint32_t our_seq = 0x12345678;

    if (flags & TCP_FLAG_SYN) {
        mb1_print("TCP SYN recu -> SYN-ACK\r\n");
        send_tcp(src_mac, src_ip, 80, src_port,
                 our_seq, seq + 1,
                 TCP_FLAG_SYN | TCP_FLAG_ACK,
                 NULL, 0);
        our_seq++;
        return;
    }

    if ((flags & TCP_FLAG_ACK) && tcp_payload_len > 0) {
        mb1_print("TCP DATA recu -> HTTP 200\r\n");
        uint32_t new_ack = seq + tcp_payload_len;

        // ACK les données reçues
        send_tcp(src_mac, src_ip, 80, src_port,
                 our_seq, new_ack,
                 TCP_FLAG_ACK,
                 NULL, 0);

        // Envoyer la réponse HTTP + FIN en un seul segment
        uint16_t resp_len = (uint16_t)(sizeof(http_response) - 1);
        send_tcp(src_mac, src_ip, 80, src_port,
                 our_seq, new_ack,
                 TCP_FLAG_ACK | TCP_FLAG_FIN,
                 (const uint8_t*)http_response, resp_len);
        our_seq += resp_len + 1; // +1 pour le FIN
        return;
    }

    if (flags & TCP_FLAG_FIN) {
        uint32_t ack_num = ((uint32_t)pkt[tcp_base+8]  << 24) |
                           ((uint32_t)pkt[tcp_base+9]  << 16) |
                           ((uint32_t)pkt[tcp_base+10] <<  8) |
                                      pkt[tcp_base+11];
        send_tcp(src_mac, src_ip, 80, src_port,
                 our_seq, seq + 1,
                 TCP_FLAG_ACK,
                 NULL, 0);
        mb1_print("TCP FIN -> ACK\r\n");
    }
}

// ── Main ──────────────────────────────────────────────────────
int main(void) {
    #ifdef PREINIT_SUPPORTED
    preinit();
    #endif

    // ── UART ──────────────────────────────────────────────────
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
    mb1_print("=== ENC28J60 WEB SERVER ===\r\n");
    mb1_print("IP: 172.20.22.200 port 80\r\n");

    // ── Reset matériel via RST ────────────────────────────────
    digital_out_init(&rst, MIKROBUS2_RST);
    mb1_print("HW RESET...");
    digital_out_high(&rst); Delay_ms(10);
    digital_out_low(&rst);  Delay_ms(10);
    digital_out_high(&rst); Delay_ms(100);
    mb1_print(" OK\r\n");

    // ── SPI 1 MHz mode 0 ─────────────────────────────────────
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

    // ── Reset logiciel ────────────────────────────────────────
    mb1_print("SW RESET...");
    enc_soft_reset();
    Delay_ms(100);
    mb1_print(" OK\r\n");

    // ── Attendre oscillateur interne ──────────────────────────
    mb1_print("WAIT CLKRDY...");
    enc_wait_clkready();
    Delay_ms(500);
    mb1_print(" OK\r\n");

    // ── Révision chip ─────────────────────────────────────────
    enc_select_bank(3);
    uint8_t chip_rev = enc_read_reg(EREVID);
    mb1_print("EREVID = ");
    mb1_print_hex(chip_rev);
    if (chip_rev == 0x05 || chip_rev == 0x07)
        mb1_print(" (rev B5/B7 workarounds TX actifs)\r\n");
    else
        mb1_print("\r\n");

    // ── Init MAC + buffers ────────────────────────────────────
    mb1_print("MAC INIT...");
    enc_init();
    mb1_print(" OK\r\n");

    // ── Init PHY ──────────────────────────────────────────────
    enc_phy_init();

    // ── Lire PHY ID pour valider MII ─────────────────────────
    uint8_t low, high;
    mb1_print("PHHID1 = ");
    enc_phy_read(PHHID1, &low, &high);
    mb1_print_hex(high); mb1_print_hex(low);
    mb1_print(" (attendu 0x0083)\r\n");

    enc_phy_read(PHSTAT2, &low, &high);
    mb1_print("PHSTAT2 = "); mb1_print_hex(high); mb1_print_hex(low); mb1_print("\r\n");

    // ── Attente lien (10s max) ────────────────────────────────
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

    // ── Boucle principale ─────────────────────────────────────
    mb1_print("\r\n=== SERVEUR ACTIF — ouvrir http://172.20.22.200 ===\r\n");

    static uint8_t rx_buf[700];

    while (1) {
        uint16_t rx_len = enc_recv_packet(rx_buf, sizeof(rx_buf));
        if (rx_len > 13) {
            uint16_t etype = ((uint16_t)rx_buf[12] << 8) | rx_buf[13];
            if (etype == 0x0806)
                handle_arp(rx_buf, rx_len);
            else if (etype == 0x0800)
                handle_ip(rx_buf, rx_len);
        }
    }

    return 0;
}