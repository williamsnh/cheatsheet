# 🚗 Guide d'examen — Systèmes et Autonomie des Transports (CAN Bus)

> **Usage** : Guide de référence rapide pour l'examen machine de 2h.  
> Environnement : Ubuntu, `can-utils` installé, simulateur `avsim2D` prêt.

---

## 📦 TABLE DES MATIÈRES

1. [Setup initial — CAN virtuel](#1-setup-initial--can-virtuel)
2. [Outils can-utils essentiels](#2-outils-can-utils-essentiels)
3. [Trame CAN en C — bases](#3-trame-can-en-c--bases)
4. [Spécifications CAN du simulateur](#4-spécifications-can-du-simulateur)
5. [vehicle_checker_student.c](#5-vehicle_checker_studentc)
6. [dashboard.c](#6-dashboardc)
7. [road_follower.c](#7-road_followerc)
8. [OBD2 (TP3)](#8-obd2-tp3)
9. [CAN-FD (TP3)](#9-can-fd-tp3)
10. [MISRA — règles clés](#10-misra--règles-clés)
11. [Compilation & Makefile](#11-compilation--makefile)
12. [Checklist examen](#12-checklist-examen)

---

## 1. Setup initial — CAN virtuel

### Terminaux à ouvrir (dans l'ordre)

**Terminal 1 — Setup réseau CAN (UNE SEULE FOIS par session)**
```bash
sudo modprobe vcan                          # Charge le module kernel vCAN
sudo ip link add dev vcan0 type vcan        # Crée l'interface vcan0
sudo ip link set up vcan0                   # Active vcan0

# Si besoin de vcan1 (OBD2) :
sudo ip link add dev vcan1 type vcan
sudo ip link set up vcan1

# Si besoin de vcan2 (CAN-FD) :
sudo ip link add dev vcan2 type vcan
sudo ip link set up vcan2 mtu 72           # MTU 72 obligatoire pour CAN-FD !
```

**Terminal 2 — Lancer le simulateur**
```bash
python3 -m venv venv_SAdT 


source venv_SAdT/bin/activate              # Activer l'environnement Python
python -m avsim2D                          # Lance le simulateur (avec CAN)
# Raccourcis utiles :
#   S = afficher/cacher le capteur caméra
#   R = reset simulateur
```

**Terminal 3 — Surveiller le bus**
```bash
candump vcan0                              # Affiche toutes les trames
candump vcan0 -l                           # Log dans un fichier
candump vcan0 100~1FF                      # Filtre IDs 0x100 à 0x1FF
```

**Terminal 4 — Votre programme**
```bash
gcc -o mon_programme mon_programme.c       # Compiler
./mon_programme                            # Exécuter
```

---

## 2. Outils can-utils essentiels

```bash
# Envoyer une trame manuellement :
cansend vcan0 321#006400                   # ID=0x321, data=0x00 0x64 0x00

# Générer des trames aléatoires (test de filtres) :
cangen vcan0 -I 100 -L 8 -D i             # ID fixe 0x100, 8 bytes, incrémental

# Afficher avec filtre ID :
candump vcan0 321:FFF                      # Uniquement ID 0x321
candump vcan0 C00:FF0                      # IDs 0xC00 à 0xC0F (masque)

# Rejouer un log :
canplayer -I fichier.log vcan0=vcan0

# Voir les stats :
canstat vcan0
```

### Filtre candump — syntaxe
```
candump vcan0 <id>:<mask>
# Exemple : IDs 0x100 à 0x1FF
candump vcan0 100:700
# Explication : reçu & 0x700 == 0x100 & 0x700  →  accepte 0x100..0x1FF
```

---

## 3. Trame CAN en C — bases

### Template complet : init socket + filtre + send/receive

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int main(void) {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    /* --- Ouvrir le socket CAN --- */
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) { perror("socket"); return 1; }

    /* --- Lier à vcan0 --- */
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_index;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }

    /* --- Filtre : accepter uniquement IDs 0x100 à 0x1FF --- */
    struct can_filter rfilter[1];
    rfilter[0].can_id   = 0x100;
    rfilter[0].can_mask = 0x700;   /* reçu & 0x700 == 0x100 */
    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    /* --- Envoyer une trame --- */
    frame.can_id  = 0x321;
    frame.can_dlc = 3;
    frame.data[0] = 0x00;   /* throttle */
    frame.data[1] = 0x00;   /* brake    */
    frame.data[2] = 0x00;   /* steering */
    write(s, &frame, sizeof(frame));

    /* --- Recevoir une trame --- */
    int nbytes = read(s, &frame, sizeof(frame));
    if (nbytes > 0) {
        printf("ID: 0x%X  DLC: %d  Data:", frame.can_id, frame.can_dlc);
        for (int i = 0; i < frame.can_dlc; i++)
            printf(" %02X", frame.data[i]);
        printf("\n");
    }

    close(s);
    return 0;
}
```

### ID étendu 29 bits (CAN 2.0B)
```c
frame.can_id = 0x8123 | CAN_EFF_FLAG;   /* Active le flag 29 bits */
```

### Mettre une valeur numérique dans les données
```c
frame.data[1] = 42;                    /* Valeur décimale 42 directement */
frame.data[0] = (valeur >> 8) & 0xFF;  /* Octet haut d'un uint16 */
frame.data[1] =  valeur       & 0xFF;  /* Octet bas  d'un uint16 */
/* Exemple : motor speed 0x6E17 = 5998 rpm */
uint16_t rpm = (frame.data[0] << 8) | frame.data[1];
```

### Valeur signée (steering)
```c
int8_t steering = (int8_t)frame.data[2];   /* Cast signé ! */
/* steering ∈ [-100, 100] */
```

---

## 4. Spécifications CAN du simulateur

### 🎮 Trame de contrôle véhicule — **ENVOYER**

| ID | DLC | Byte 0 | Byte 1 | Byte 2 |
|----|-----|--------|--------|--------|
| `0x321` | 3 | throttle 0–100 | brake 0–100 | steering **signé** -100–+100 |

```c
/* Exemple : avancer à 50%, pas de frein, tout droit */
frame.can_id  = 0x321;
frame.can_dlc = 3;
frame.data[0] = 50;            /* throttle 50% */
frame.data[1] = 0;             /* brake 0%     */
frame.data[2] = (uint8_t)0;   /* steering 0   */
```

### 📡 Trames capteurs — **RECEVOIR**

| ID | DLC | Contenu |
|----|-----|---------|
| `0xC06` | 2 | bytes 0-1 : motor speed RPM (uint16 big-endian) |
| `0xC07` | 2 | byte 0 : vehicle speed km/h, byte 1 : gear (0-5) |

```c
/* Décoder 0xC06 — Motor Speed */
uint16_t motor_rpm = (frame.data[0] << 8) | frame.data[1];

/* Décoder 0xC07 — Speed & Gear */
uint8_t speed_kmh = frame.data[0];
uint8_t gear      = frame.data[1];
```

### 📷 Trames caméra sémantique — **RECEVOIR** (0xC00 à 0xC05)

| ID | Zone | Couleur |
|----|------|---------|
| `0xC00` | full_left | rouge |
| `0xC01` | left | orange |
| `0xC02` | middle_left | jaune |
| `0xC03` | middle_right | vert |
| `0xC04` | right | bleu |
| `0xC05` | full_right | violet |

**Pour chaque ID, 8 bytes :**
```
byte 0 : road     (0–100)
byte 1 : stop     (0–100)
byte 2 : yield    (0–100)
byte 3 : crossing (0–100)
byte 4 : car_park (0–100)
byte 5-7 : réservés
```

```c
/* Exemple : lire la quantité de route vue à gauche */
uint8_t road_left   = cam[0].data[0];   /* 0xC00, byte 0 */
uint8_t road_middle = cam[2].data[0];   /* 0xC02, byte 0 */
```

### 💡 Trames lumières — (reverse engineering TP1)

```bash
# Observer avec candump pendant que vehicle_checker tourne :
candump vcan0
# Les IDs des blinkers/feux sont à retrouver par observation
# Typiquement envoyé avec write() dans vehicle_checker
```

---

## 5. vehicle_checker_student.c

> **But** : open-loop, envoie des commandes prédéfinies, attend d'avoir reçu 0xC00→0xC07 avant d'envoyer.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <pthread.h>

/* Flags indiquant quelles trames capteur ont été reçues au moins une fois */
static volatile int received[8] = {0}; /* index 0 = 0xC00, ..., 7 = 0xC07 */

/* Vérifie si toutes les trames 0xC00→0xC07 ont été reçues */
static int all_received(void) {
    for (int i = 0; i < 8; i++) {
        if (!received[i]) return 0;
    }
    return 1;
}

int main(void) {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    /* Init socket */
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_index;
    bind(s, (struct sockaddr *)&addr, sizeof(addr));

    /* Attendre au moins une fois chaque ID 0xC00 à 0xC07 */
    printf("Waiting for sensor frames 0xC00..0xC07...\n");
    while (!all_received()) {
        int n = read(s, &frame, sizeof(frame));
        if (n > 0) {
            if (frame.can_id >= 0xC00 && frame.can_id <= 0xC07) {
                received[frame.can_id - 0xC00] = 1;
                printf("Got 0x%X\n", frame.can_id);
            }
        }
    }
    printf("All sensor frames received. Starting vehicle control...\n");

    /* Séquence open-loop */
    /* Exemple : avancer 3s, freiner 1s */
    for (int i = 0; i < 30; i++) {
        frame.can_id  = 0x321;
        frame.can_dlc = 3;
        frame.data[0] = 30;    /* throttle */
        frame.data[1] = 0;     /* brake    */
        frame.data[2] = (uint8_t)0; /* tout droit */
        write(s, &frame, sizeof(frame));
        usleep(100000);        /* 100ms */
    }
    /* Freiner */
    frame.data[0] = 0;
    frame.data[1] = 50;
    for (int i = 0; i < 10; i++) {
        write(s, &frame, sizeof(frame));
        usleep(100000);
    }

    close(s);
    return 0;
}
```

```bash
gcc -o vehicule_checker_student vehicule_checker_student.c
./vehicule_checker_student
```

---

## 6. dashboard.c

> **But** : affiche en continu speed, gear, RPM et la direction à suivre (computed depuis caméra).

### Algorithme de direction (formule du TP)

```
Steering = Σ(i=0→2) Ki * C_C0i  -  Σ(i=3→5) Ki * C_C0i  - Offset
```
- Zones gauches (0xC00, 0xC01, 0xC02) → contribuent positivement → tourner à gauche
- Zones droites (0xC03, 0xC04, 0xC05) → contribuent négativement → tourner à droite
- Si Steering > seuil → `<-` (gauche)
- Si Steering < -seuil → `->` (droite)
- Sinon → `^` (tout droit)

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <pthread.h>

/* Données partagées (accès depuis thread de lecture) */
static volatile uint8_t  g_speed  = 0;
static volatile uint8_t  g_gear   = 0;
static volatile uint16_t g_rpm    = 0;
static volatile uint8_t  g_cam[6][8]; /* 6 zones, 8 bytes chacune */

/* Thread de lecture CAN */
void *can_reader(void *arg) {
    int s = *(int *)arg;
    struct can_frame frame;
    while (1) {
        int n = read(s, &frame, sizeof(frame));
        if (n <= 0) continue;
        switch (frame.can_id) {
            case 0xC06:
                g_rpm = (uint16_t)((frame.data[0] << 8) | frame.data[1]);
                break;
            case 0xC07:
                g_speed = frame.data[0];
                g_gear  = frame.data[1];
                break;
            case 0xC00: case 0xC01: case 0xC02:
            case 0xC03: case 0xC04: case 0xC05: {
                int idx = frame.can_id - 0xC00;
                for (int i = 0; i < 8; i++)
                    g_cam[idx][i] = frame.data[i];
                break;
            }
            default: break;
        }
    }
    return NULL;
}

/* Calcul direction (poids Ki = 1,2,3 par exemple) */
const char *compute_direction(void) {
    static const int K[6] = {1, 2, 3, 3, 2, 1};
    int steering = 0;
    /* byte 0 = road weight de chaque zone */
    for (int i = 0; i < 3; i++)
        steering += K[i] * g_cam[i][0];   /* zones gauches */
    for (int i = 3; i < 6; i++)
        steering -= K[i] * g_cam[i][0];   /* zones droites */
    /* Offset optionnel pour compenser asymétrie */
    steering -= 0; /* ajuster si nécessaire */

    if      (steering >  20) return "<-";
    else if (steering < -20) return "->";
    else                      return "^";
}

int main(void) {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;

    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_index;
    bind(s, (struct sockaddr *)&addr, sizeof(addr));

    pthread_t tid;
    pthread_create(&tid, NULL, can_reader, &s);

    /* Boucle d'affichage */
    while (1) {
        printf("\033[H\033[J");   /* Effacer l'écran (ANSI) */
        printf("Speed: %d km/h\n",   g_speed);
        printf("Gear: %d\n",         g_gear);
        printf("Motor speed: %d rpm\n", g_rpm);
        printf("Action to follow the road: %s\n", compute_direction());
        fflush(stdout);
        usleep(200000);   /* rafraîchir 5x/s */
    }

    close(s);
    return 0;
}
```

```bash
gcc -o dashboard dashboard.c -lpthread
./dashboard
```

---

## 7. road_follower.c

> **But** : boucle fermée — utilise la caméra pour corriger la direction, limite à ~50 km/h.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <pthread.h>
#include <stdint.h>

/* ===== État global ===== */
static volatile uint8_t  g_speed = 0;
static volatile uint8_t  g_cam[6][8];

static volatile int g_cam_ready = 0;   /* reçu au moins une fois ? */

/* ===== Thread lecture ===== */
void *reader_thread(void *arg) {
    int s = *(int *)arg;
    struct can_frame frame;
    while (1) {
        read(s, &frame, sizeof(frame));
        if (frame.can_id == 0xC07) {
            g_speed = frame.data[0];
        }
        if (frame.can_id >= 0xC00 && frame.can_id <= 0xC05) {
            int idx = (int)(frame.can_id - 0xC00);
            for (int i = 0; i < 8; i++)
                g_cam[idx][i] = frame.data[i];
            g_cam_ready = 1;
        }
    }
    return NULL;
}

/* ===== Calcul du steering (même formule que dashboard) ===== */
int8_t compute_steering(void) {
    const int K[6] = {1, 2, 3, 3, 2, 1};
    int raw = 0;
    for (int i = 0; i < 3; i++)
        raw += K[i] * (int)g_cam[i][0];
    for (int i = 3; i < 6; i++)
        raw -= K[i] * (int)g_cam[i][0];

    /* Normaliser à [-100, 100] */
    if (raw >  100) raw =  100;
    if (raw < -100) raw = -100;
    return (int8_t)raw;
}

/* ===== Thread contrôle ===== */
void *control_thread(void *arg) {
    int s = *(int *)arg;
    struct can_frame frame;

    while (!g_cam_ready) usleep(10000);  /* Attendre les capteurs */

    while (1) {
        int8_t steering = compute_steering();

        /* Throttle : réduire si trop vite */
        uint8_t throttle = (g_speed < 45) ? 30U : 5U;
        uint8_t brake    = (g_speed > 55) ? 30U : 0U;

        frame.can_id  = 0x321;
        frame.can_dlc = 3;
        frame.data[0] = throttle;
        frame.data[1] = brake;
        frame.data[2] = (uint8_t)steering;  /* signé dans un uint8 */

        write(s, &frame, sizeof(frame));
        usleep(50000);   /* 20 Hz */
    }
    return NULL;
}

int main(void) {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;

    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_index;
    bind(s, (struct sockaddr *)&addr, sizeof(addr));

    pthread_t r_tid, c_tid;
    pthread_create(&r_tid, NULL, reader_thread,  &s);
    pthread_create(&c_tid, NULL, control_thread, &s);

    pthread_join(r_tid, NULL);
    pthread_join(c_tid, NULL);

    close(s);
    return 0;
}
```

```bash
gcc -o road_follower road_follower.c -lpthread
./road_follower
```

---

## 8. OBD2 (TP3)

### Rappel protocole OBD2 sur CAN

| Direction | CAN ID | Byte 0 | Byte 1 | Byte 2 | Byte 3 |
|-----------|--------|--------|--------|--------|--------|
| **Requête** (UserOBD2Terminal → studentOBD2) | `0x7DF` | `0x02` | `0x01` | PID | - |
| **Réponse** (studentOBD2 → UserOBD2Terminal) | `0x7E8` | `0x03` | `0x41` | PID | valeur |

### PIDs utiles

| PID | Paramètre | Formule décodage |
|-----|-----------|-----------------|
| `0x0C` | Engine RPM | `((A*256)+B) / 4` → RPM |
| `0x0D` | Vehicle Speed | `A` → km/h |
| `0x11` | Throttle Position | `A * 100 / 255` → % |

### studentOBD2.c — logique principale

```c
/* Lit sur vcan0 (simulateur), répond sur vcan1 */
/* Deux sockets : s0 = vcan0, s1 = vcan1 */

/* Boucle principale */
while (1) {
    /* Lire capteurs sur vcan0 */
    read(s0, &frame0, sizeof(frame0));
    if (frame0.can_id == 0xC06)
        motor_rpm = (frame0.data[0] << 8) | frame0.data[1];
    if (frame0.can_id == 0xC07) {
        speed_kmh = frame0.data[0];
    }

    /* Lire requête OBD2 sur vcan1 */
    /* Utiliser select() ou poll() pour non-bloquant si besoin */
    /* Si reçoit 0x7DF avec byte1=0x01 (mode courant) */
    if (req.can_id == 0x7DF && req.data[1] == 0x01) {
        uint8_t pid = req.data[2];
        struct can_frame resp;
        resp.can_id  = 0x7E8;
        resp.can_dlc = 8;
        memset(resp.data, 0x55, 8);

        if (pid == 0x0C) {               /* RPM */
            uint16_t raw = (uint16_t)(motor_rpm * 4);
            resp.data[0] = 0x04;
            resp.data[1] = 0x41;
            resp.data[2] = 0x0C;
            resp.data[3] = (raw >> 8) & 0xFF;
            resp.data[4] = raw & 0xFF;
        } else if (pid == 0x0D) {        /* Speed */
            resp.data[0] = 0x03;
            resp.data[1] = 0x41;
            resp.data[2] = 0x0D;
            resp.data[3] = speed_kmh;
        } else if (pid == 0x11) {        /* Throttle */
            resp.data[0] = 0x03;
            resp.data[1] = 0x41;
            resp.data[2] = 0x11;
            resp.data[3] = (uint8_t)(throttle_pct * 255 / 100);
        }
        write(s1, &resp, sizeof(resp));
    }
}
```

```bash
# Terminal 1 : simulateur sur vcan0
python -m avsim2D

# Terminal 2 : studentOBD2 (bridge vcan0 ↔ vcan1)
./studentOBD2

# Terminal 3 : UserOBD2Terminal (envoie requêtes sur vcan1)
./UserOBD2Terminal

# Terminal 4 : vérifier
candump vcan1
```

---

## 9. CAN-FD (TP3)

### Setup vcan2 pour CAN-FD
```bash
sudo ip link add dev vcan2 type vcan
sudo ip link set up vcan2 mtu 72    # MTU 72 = canfd_frame size
```

### sensorsCAN.c — socket CAN-FD

```c
#include <linux/can.h>
#include <linux/can/raw.h>

/* Activer CAN-FD sur le socket */
int enable_canfd = 1;
setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_canfd, sizeof(enable_canfd));

/* Utiliser canfd_frame (pas can_frame) */
struct canfd_frame fd_frame;
fd_frame.can_id = 0x100;
fd_frame.len    = 48;   /* 6 zones × 8 bytes */
fd_frame.flags  = 0;

/* Concaténer les 6 zones C00..C05 */
/* cam[0] = données de 0xC00, etc. */
for (int zone = 0; zone < 6; zone++) {
    for (int b = 0; b < 8; b++) {
        fd_frame.data[zone * 8 + b] = cam[zone][b];
    }
}
write(s_fd, &fd_frame, sizeof(fd_frame));
```

```bash
# Vérifier sur vcan2 :
candump vcan2
# Vous devez voir un frame ID 0x100 avec 48 bytes
```

---

## 10. MISRA — règles clés

### Règles les plus fréquentes dans ce projet

| Règle | Catégorie | Obligatoire? | Description |
|-------|-----------|-------------|-------------|
| **2.7** | Required | ✅ | Pas de paramètres de fonction inutilisés |
| **3.1** | Required | ✅ | Pas de `/*` ou `//` dans un commentaire |
| **13.4** | Advisory | ⚠️ | Ne pas utiliser le résultat d'une affectation comme condition |
| **14.4** | Required | ✅ | La condition d'un `if` doit être de type booléen |
| **17.7** | Required | ✅ | Vérifier la valeur de retour des fonctions |
| **11.3** | Required | ✅ | Pas de cast entre pointeur et type entier |
| **21.6** | Required | ✅ | Ne pas utiliser `printf` en production (mais ok pour ce TP) |

### Corrections typiques

```c
/* ❌ R13.4 — affectation dans condition */
if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) { ... }

/* ✅ Correction */
s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
if (s < 0) { ... }

/* ❌ R2.7 — paramètre inutilisé */
int main(int argc, char **argv) { ... }  /* argc et argv non utilisés */

/* ✅ Correction */
int main(void) { ... }

/* ❌ R3.1 — commentaire dans commentaire */
/* ceci est /* interdit */ */

/* ✅ Correction */
/* ceci est - interdit - */

/* ❌ R17.7 — retour de fonction ignoré */
write(s, &frame, sizeof(frame));

/* ✅ Correction */
ssize_t ret = write(s, &frame, sizeof(frame));
if (ret < 0) { perror("write"); }

/* ❌ Casting implicite (arithmetic) */
frame.data[2] = steering;   /* int8_t → uint8_t implicite */

/* ✅ Correction explicite */
frame.data[2] = (uint8_t)steering;
```

### Lancer cppcheck
```bash
# Vérification MISRA
cppcheck --enable=all --addon=misra road_follower.c

# Alias si sur machine CPE
alias cppcheck='/softwares/INFO/Robotique/CppCheck/cppcheck'
cppcheck --enable=all --addon=misra road_follower_misra.c
```

---

## 11. Compilation & Makefile

### Compilation manuelle
```bash
gcc -Wall -Wextra -o vehicule_checker_student vehicule_checker_student.c
gcc -Wall -Wextra -o dashboard dashboard.c -lpthread
gcc -Wall -Wextra -o road_follower road_follower.c -lpthread
gcc -Wall -Wextra -o road_follower_misra road_follower_misra.c -lpthread
gcc -Wall -Wextra -o studentOBD2 studentOBD2.c -lpthread
gcc -Wall -Wextra -o UserOBD2Terminal UserOBD2Terminal.c
gcc -Wall -Wextra -o sensorsCAN sensorsCAN.c -lpthread
```

### Makefile simple
```makefile
CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lpthread

TARGETS = vehicule_checker_student dashboard road_follower road_follower_misra \
          studentOBD2 UserOBD2Terminal sensorsCAN

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)
```

```bash
make all       # Compiler tout
make clean     # Nettoyer
```

---

## 12. Checklist examen

### Au démarrage (2 minutes)
- [ ] Ouvrir 4+ terminaux
- [ ] `sudo modprobe vcan && sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0`
- [ ] `source venv_SAdT/bin/activate`
- [ ] `python -m avsim2D` dans un terminal dédié
- [ ] `candump vcan0` dans un terminal de monitoring

### Vérification que tout fonctionne
```bash
candump vcan0   # Doit afficher des trames 0xC00..0xC07 en continu
```

### En cas de problème

| Problème | Solution |
|----------|----------|
| `bind: No such device` | Interface vcan0 pas créée/activée → relancer les commandes `ip link` |
| `socket: Protocol not supported` | `sudo modprobe vcan` manquant |
| Programme ne reçoit rien | Vérifier les filtres CAN, ou enlever le `setsockopt` de filtre |
| Steering ne fonctionne pas | Vérifier le cast `(uint8_t)(int8_t)steering` |
| CAN-FD échoue | `mtu 72` sur vcan2, et `setsockopt CAN_RAW_FD_FRAMES` |
| cppcheck non trouvé | `alias cppcheck='/softwares/INFO/Robotique/CppCheck/cppcheck'` |

### Rappels rapides

```bash
# Voir ce que le simulateur envoie :
candump vcan0 | grep "C0[0-7]"

# Envoyer manuellement throttle=30, brake=0, steering=0 :
cansend vcan0 321#1E0000

# Envoyer manuellement blinker gauche (à adapter selon reverse engineering) :
cansend vcan0 <ID>#<DATA>

# Tester le road_follower avec le dashboard en parallèle :
./road_follower &
./dashboard
```

---

*Guide préparé pour l'examen machine — Module SAT / CAN Bus*


Bien sûr ! Commençons par le plus simple possible. Je vais te montrer **un programme qui envoie une commande au simulateur** (avancer tout droit) et t'expliquer chaque ligne comme si tu n'avais jamais fait de C.

---

## Le programme le plus simple possible

```c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
```

Ces lignes tout en haut s'appellent des **imports**. C'est comme dire à ton programme : *"hé, j'aurai besoin de ces outils, garde-les à portée de main"*.
- `stdio.h` → pour afficher des trucs avec `printf`
- `socket.h`, `can.h` → les outils pour parler le langage CAN

---

```c
int main(void) {
```

C'est **le point de départ** de tout programme C. Quand tu lances `./monprogramme`, le code ici s'exécute. `void` signifie juste qu'on ne lui passe rien en entrée.

---

```c
    int s;
```

On crée une variable `s` de type entier. Elle va stocker le **numéro de notre connexion** au bus CAN (comme un numéro de téléphone pour savoir à qui parler).

---

```c
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
```

On crée trois **"boîtes"** :
- `addr` → l'adresse du bus CAN (comme une adresse postale)
- `ifr` → les infos sur l'interface réseau (ici `vcan0`)
- `frame` → **la trame CAN qu'on va envoyer** (c'est le message)

---

```c
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("Erreur socket");
        return 1;
    }
```

On **ouvre une connexion** vers le bus CAN. C'est comme décrocher un téléphone.  
Si ça échoue (s < 0), on affiche l'erreur et on arrête le programme (`return 1` = j'ai eu un problème).

---

```c
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
```

On dit au programme : *"je veux me connecter à l'interface qui s'appelle `vcan0`"*.  
`ioctl` récupère ensuite le numéro interne de cette interface (le système Linux travaille avec des numéros, pas des noms).

---

```c
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_index;
    bind(s, (struct sockaddr *)&addr, sizeof(addr));
```

On **branche** notre connexion sur `vcan0`. Après cette ligne, notre programme est connecté au bus CAN, exactement comme brancher un câble.

---

```c
    frame.can_id  = 0x321;
    frame.can_dlc = 3;
    frame.data[0] = 30;
    frame.data[1] = 0;
    frame.data[2] = 0;
```

On **prépare le message** à envoyer. D'après les specs du simulateur :
- `can_id = 0x321` → c'est l'identifiant de la trame "contrôle véhicule"
- `can_dlc = 3` → on envoie 3 octets de données
- `data[0] = 30` → **accélérateur à 30%**
- `data[1] = 0` → **frein à 0%** (pas de frein)
- `data[2] = 0` → **direction : tout droit** (0 = neutre, négatif = gauche, positif = droite)

---

```c
    write(s, &frame, sizeof(frame));
    printf("Trame envoyée !\n");
```

On **envoie** la trame sur le bus. `write` c'est comme appuyer sur "Envoyer". Puis on affiche un message pour confirmer.

---

```c
    close(s);
    return 0;
}
```

On **ferme la connexion** (comme raccrocher le téléphone). `return 0` = tout s'est bien passé.

---

## Programme complet à copier-coller

```c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int main(void) {

    /* Étape 1 : déclarer nos variables */
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    /* Étape 2 : ouvrir la connexion CAN */
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("Erreur socket");
        return 1;
    }

    /* Étape 3 : se connecter à vcan0 */
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_index;
    bind(s, (struct sockaddr *)&addr, sizeof(addr));

    /* Étape 4 : préparer le message */
    frame.can_id  = 0x321;   /* ID de la trame véhicule */
    frame.can_dlc = 3;        /* 3 octets de données */
    frame.data[0] = 30;       /* accélérateur 30% */
    frame.data[1] = 0;        /* frein 0% */
    frame.data[2] = 0;        /* direction : tout droit */

    /* Étape 5 : envoyer */
    write(s, &frame, sizeof(frame));
    printf("Trame envoyée ! Le véhicule devrait avancer.\n");

    /* Étape 6 : fermer */
    close(s);
    return 0;
}
```

---

## Comment le lancer

Dans ton terminal, dans l'ordre :

```bash
# 1. Compiler (transformer le texte en programme exécutable)
gcc -o mon_premier_programme mon_premier_programme.c

# 2. Lancer le simulateur dans un autre terminal d'abord !
python -m avsim2D

# 3. Lancer ton programme
./mon_premier_programme
```

Dans un troisième terminal tu peux vérifier que la trame est bien partie :
```bash
candump vcan0
# Tu devrais voir : vcan0  00000321  [3]  1E 00 00
#                                              ^30  ^0  ^0
```

---

## Le schéma mental à retenir

```
Ton programme          Bus vcan0          Simulateur
     |                    |                    |
     |--- socket() ------>|                    |
     |--- bind() -------->|                    |
     |                    |                    |
     |--- write(0x321) -->|--- trame CAN ----->|
     |                    |                 🚗 bouge !
     |--- close() ------->|                    |
```

C'est tout ! Chaque programme C du TP suit exactement ce même squelette. On change juste les valeurs dans `frame.data[]` selon ce qu'on veut faire.

# Exemple Masque
```bash
0x480 → 0x4BF

1001 0000 000
1001 0111 111
-------------
1111 1??? ??? --> 0111 1100 0000 = 0x7C0

================= Test =================

0100 0111 1001 0x479
0111 1100 0000
--------------
0100 0100 0000 = 0x440

0100 1000 0001 = 0x481
0111 1100 0000
-----------------
0100 1000 0000 = 0x480


0100 1011 1111 = 0x4BF
0111 1100 0000
---------------
0100 1000 0000 = 0x480


0100 1000 0000 = 0x480
0111 1100 0000
-----------------
0100 1000 0000  0x480


0100 1100 0000 = 0x4C0
0111 1100 0000
------------------
0100 1100 0000  = 0x4C0
```