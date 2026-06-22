# SPI Ethernet — ENC28J60 Driver

Middleware MikroSDK pour contrôler un chip Ethernet **ENC28J60** via SPI.

---

## Architecture

```
Application
    │
    ▼
spi_ethernet.h / spi_ethernet.c       ← API publique (couche abstraction)
    │  vtable (pointeurs de fonctions)
    ▼
spi_ethernet_enc28j60.h / .c          ← Driver spécifique ENC28J60
    │  commandes SPI (RCR, WCR, RBM, WBM, BFS, BFC)
    ▼
Puce ENC28J60 ──► Réseau Ethernet
```

---

## Fichiers

### `spi_ethernet_enc28j60.h`
Dictionnaire hardware du chip. Contient uniquement des constantes :
- Adresses des registres organisés en **4 banques** (Bank 0–3 + PHY)
- Les **6 instructions SPI** du protocole ENC28J60 (`RCR`, `WCR`, `RBM`, `WBM`, `BFS`, `BFC`)
- Layout de la **SRAM interne** (8 ko) : buffer RX circulaire (`0x0000–0x17FF`), buffer TX (`0x1800–0x1FFF`)
- Structures de données (`enc28j60_arp_cache_t`, `enc28j60_cfg_t`)

### `spi_ethernet.h`
Contrat public de la bibliothèque. Définit :
- `spi_ethernet_t` — objet représentant une interface réseau (SPI handle, CS pin, MAC, IP, duplex...)
- `spi_ethernet_driver_t` — **vtable** (table de fonctions virtuelles) permettant de brancher n'importe quel driver
- `ethernet_frame_t` — structure d'une trame Ethernet complète (dest, src, type, payload)
- L'API publique : `spi_ethernet_init()`, `spi_ethernet_send()`, `spi_ethernet_receive()`, `spi_ethernet_available()`, `spi_ethernet_get_link_status()`...

### `spi_ethernet_enc28j60.c`
Implémentation bas niveau. Dialogue directement avec le chip via SPI :
- **Init** : séquence en 9 étapes (reset HW, config buffers RX/TX, config MAC, écriture adresse MAC, activation réception)
- **Sélection de banque** : `enc28j60_select_bank()` gère les 4 banques de registres du chip
- **Primitives SPI** (`static`) : `read_reg`, `write_reg`, `read_mem`, `write_mem`, `set_bit_reg`, `clear_bit_reg`, `soft_reset`
- **Réception** : `enc28j60_read_packet()` lit le header 6 octets (next pointer + byte count + RSV), extrait les données et libère le buffer circulaire
- **Émission** : `enc28j60_send_packet()` écrit dans le buffer TX et arme la transmission
- Remplit la **vtable** `enc28j60_driver` exportée vers la couche supérieure

### `spi_ethernet.c`
Couche d'abstraction. Chaque fonction :
1. Valide les pointeurs (`eth`, `eth->drv`, fonction cible)
2. Délègue à la vtable du driver (`eth->drv->send_packet(...)`)

Fournit aussi `ethernet_send_frame()` et `ethernet_receive_frame()` qui assemblent/désassemblent une trame Ethernet complète (header 14 octets + payload).

---

## Utilisation typique

```c
spi_ethernet_t eth = {
    .mac        = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01},
    .ip         = 0xC0A80001, // 192.168.0.1
    .fullDuplex = 1,
};

spi_ethernet_init(&eth, &enc28j60_driver);

// Envoyer des données brutes
spi_ethernet_send(&eth, data, len);

// Recevoir
if (spi_ethernet_available(&eth)) {
    spi_ethernet_receive(&eth, buf, sizeof(buf));
}
```

---

## État du code

| Fonctionnalité | État |
|---|---|
| Init SPI + ENC28J60 | ✅ Implémenté |
| Envoi / Réception de paquets | ✅ Implémenté |
| Assemblage trames Ethernet | ✅ Implémenté |
| get/set MAC, IP | ⚠️ Stubs (`TODO`) |
| DHCP, DNS, ARP, ICMP | ⚠️ Déclaré, non implémenté |
| Gateway, PHY mode, ping | 🚧 Commenté |
| Gestion nextPtr en RX | ⚠️ Bug connu (octets mal ordonnés) |

---

## Fichier de test `main.c`

Le fichier contient **3 exemples** empilés sous des blocs `#if 0 / #else / #endif`. Seul le dernier bloc (`#else`) est actif à la compilation.

### Bloc 1 — Test minimal (désactivé)
Initialise l'interface, envoie 5 octets, puis boucle en lisant les paquets reçus. Sert à vérifier que la couche SPI fonctionne.

### Bloc 2 — Observer le trafic (désactivé)
Lit des trames Ethernet, parse le header (`eth_hdr_t`) et inspecte l'EtherType. Utile pour valider la réception brute (`0x0806` = ARP, `0x0800` = IPv4).

### Bloc 3 — Stack réseau minimaliste (actif)

C'est le test le plus complet. Le MCU répond à deux protocoles de façon autonome :

#### ARP (EtherType `0x0806`)
```
PC : "Qui a l'IP 192.168.1.50 ?"  →  ARP Request (broadcast)
MCU : "C'est moi, MAC = 02:00:00:00:00:01"  →  ARP Reply
```
Le code vérifie l'opcode (`0x0001` = request), compare les 4 octets d'IP cible, construit la réponse en inversant src/dst et la transmet.

#### ICMP Echo / Ping (EtherType `0x0800`, protocole `0x01`)
```
PC : ping 192.168.1.50  →  ICMP Echo Request (type 8)
MCU : répond              →  ICMP Echo Reply (type 0)
```
Le code extrait la longueur du header IP (`ip[0] & 0x0F) * 4`), recalcule les checksums IP et ICMP manuellement avant de renvoyer la trame avec IPs inversées.

#### Fonction `ip_checksum()`
Implémente l'algorithme standard RFC 1071 — somme des mots 16 bits avec retenue repliée, puis complément à 1.

```
Trame reçue (rx)
    │
    ├─ type == 0x0806 → répondre à l'ARP
    └─ type == 0x0800 → si proto ICMP et IP cible = moi → répondre au ping
```

> **Note** : l'IP du MCU est codée en dur via les macros `MY_IP_0..3` (`192.168.1.50`). Il n'y a pas de DHCP dans ce test.

---

## Dépendances

- `drv_spi_master.h` — driver SPI MikroSDK
- `delays.h` — fonctions de délai (`Delay_ms`, `Delay_500us`)