# 🚗 Révision — Systèmes et Autonomie des Transports (SAT)
> Examen machine 2h — CAN Bus, Simulateur, C embarqué

---

## 🔧 1. Setup rapide (à faire au début de chaque TP)

```bash
# Activer le venv
source venv_SAdT/bin/activate

# Lancer le simulateur sans CAN
python -m avsim2D --no-CAN True

# Lancer le simulateur AVEC CAN (usage normal)
python -m avsim2D
```

---

## 📡 2. CAN Bus — Mise en place du réseau virtuel

```bash
# Charger le module kernel vCAN
sudo modprobe vcan

# Créer l'interface vcan0
sudo ip link add dev vcan0 type vcan

# Activer vcan0
sudo ip link set up vcan0

# Créer vcan1 (OBD2) et vcan2 (CAN-FD) de la même façon
sudo ip link add dev vcan1 type vcan
sudo ip link set up vcan1

# vcan2 pour CAN-FD (MTU 72 obligatoire pour CAN-FD)
sudo ip link add dev vcan2 type vcan
sudo ip link set up vcan2 mtu 72

# Si vcan2 est déjà up, le mettre down d'abord :
sudo ip link set down vcan2
```

---

## 🛠️ 3. can-utils — Commandes essentielles

```bash
# Écouter tout ce qui passe sur vcan0
candump vcan0

# Envoyer une trame : ID#DATA (hex)
cansend vcan0 123#F0A1DD03

# Générer des trames aléatoires (utile pour tester les filtres)
cangen vcan0

# Filtrer avec candump (ex: IDs 0x100 à 0x1FF)
candump vcan0 100~1FF     # syntaxe filtre candump
# ou avec masque :
candump vcan0 100:7FF00   # <can_id>:<mask>

# Logger dans un fichier
candump -l vcan0

# Rejouer un log
canplayer -I fichier.log vcan0
```

> 💡 **Règle du filtre** : `<received_id> & mask == can_id & mask`

---

## 💻 4. Code C — Structure de base d'un nœud CAN

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
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    /* Créer le socket CAN */
    sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) { perror("socket"); return 1; }

    /* Lier à vcan0 */
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(sock, SIOCGIFINDEX, &ifr);

    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_index;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }

    /* --- ENVOYER une trame --- */
    frame.can_id  = 0x321;          /* ID standard 11 bits */
    frame.can_dlc = 3;              /* Longueur données */
    frame.data[0] = 50;             /* throttle 50% */
    frame.data[1] = 0;              /* brake 0% */
    frame.data[2] = (signed char)0; /* steering 0 */

    write(sock, &frame, sizeof(frame));

    /* --- RECEVOIR une trame --- */
    int nbytes = read(sock, &frame, sizeof(frame));
    if (nbytes > 0) {
        printf("ID: 0x%X  DLC: %d  Data: ", frame.can_id, frame.can_dlc);
        for (int i = 0; i < frame.can_dlc; i++)
            printf("%02X ", frame.data[i]);
        printf("\n");
    }

    close(sock);
    return 0;
}
```

### ⚠️ ID 29 bits (CAN 2.0B / Extended)
```c
/* Pour un ID > 0x7FF (ex: 0x8123), utiliser CAN_EFF_FLAG */
frame.can_id = 0x8123 | CAN_EFF_FLAG;
```

### Mettre la valeur décimale 42 dans le 2ème octet
```c
frame.data[1] = 42;  /* direct, pas de sprintf ! */
```

---

## 🔍 5. Filtres CAN en C

```c
struct can_filter rfilter[1];

/* Accepter uniquement IDs 0x100 à 0x1FF */
rfilter[0].can_id   = 0x100;
rfilter[0].can_mask = 0x700;  /* 0x100 & 0x700 == 0x100, 0x1FF & 0x700 == 0x100 ✓ */

setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
```

---

## 🚘 6. Spécifications des trames simulateur

### Commande de mouvement (ID 0x321, DLC 3)
| Byte | Signification | Min | Max |
|------|--------------|-----|-----|
| 0 | Throttle (accélération) | 0x00 (0%) | 0x64 (100%) |
| 1 | Brake (frein) | 0x00 (0%) | 0x64 (100%) |
| 2 | Steering (direction) **signed char** | 0x9C (-100%) | 0x64 (100%) |

```c
frame.data[2] = (signed char)(-50); /* Tourner à gauche */
```

### Vitesse moteur (ID 0xC06, DLC 2)
| Bytes | Signification | Min | Max |
|-------|--------------|-----|-----|
| 0-1 | Régime moteur | 0 rpm | 5998 rpm (0x6E17) |

```c
uint16_t rpm = (frame.data[0] << 8) | frame.data[1];
```

### Vitesse véhicule + rapport (ID 0xC07, DLC 2)
| Byte | Signification | Min | Max |
|------|--------------|-----|-----|
| 0 | Vitesse km/h | 0x00 | 0xD6 (214 km/h) |
| 1 | Rapport engagé | 0x00 | 0x05 |

### Caméra sémantique (IDs 0xC00 à 0xC05, DLC 8)
| Byte | Classe | Valeur |
|------|--------|--------|
| 0 | road | 0-100 |
| 1 | stop | 0-100 |
| 2 | yield | 0-100 |
| 3 | crossing | 0-100 |
| 4 | car_park | 0-100 |
| 5-7 | réservé | — |

| CAN ID | Zone caméra |
|--------|------------|
| 0xC00 | full_left |
| 0xC01 | left |
| 0xC02 | middle_left |
| 0xC03 | middle_right |
| 0xC04 | right |
| 0xC05 | full_right |

### Éclairages (reverse engineering TP1)
> À retrouver avec `candump` + `vehicle_checker` — noter ici tes résultats !
```
Blinker droit  : ID=____  Data=____
Blinker gauche : ID=____  Data=____
Feux de croisement : ID=____  Data=____
Feux de route  : ID=____  Data=____
```

---

## 📊 7. Algorithme suivi de route (Dashboard)

```
Steering = Σ(Ki * C_C0i pour i=0..2) - Σ(Ki * C_C0i pour i=3..5) - Offset
```
- Si Steering > seuil → `->` (aller à droite)
- Si Steering < -seuil → `<-` (aller à gauche)
- Sinon → `^` (tout droit)

```c
/* Exemple simple */
int left  = frame_C00.data[0] + frame_C01.data[0] + frame_C02.data[0];
int right = frame_C03.data[0] + frame_C04.data[0] + frame_C05.data[0];
int steering = left - right;

if (steering > 10)       printf("Action: <-\n");
else if (steering < -10) printf("Action: ->\n");
else                     printf("Action: ^\n");
```

---

## 🏥 8. OBD2 (TP3)

### Architecture
```
AVSim2D -- vcan0 --> studentOBD2 -- vcan1 --> UserOBD2Terminal
```

### PIDs OBD2 à implémenter
| PID | Signification | Formule de décodage |
|-----|--------------|---------------------|
| 0x0C | Engine RPM | ((A*256)+B) / 4 |
| 0x0D | Vehicle Speed | A (km/h direct) |
| 0x11 | Throttle position | A * 100 / 255 (%) |

### Format trame requête OBD2
```
ID requête  : 0x7DF
Data[0] = 0x02   (nombre d'octets suivants)
Data[1] = 0x01   (mode 1 = données temps réel)
Data[2] = PID    (ex: 0x0C pour RPM)
```

### Format trame réponse OBD2
```
ID réponse  : 0x7E8
Data[0] = 0x04
Data[1] = 0x41   (mode 1 + 0x40)
Data[2] = PID
Data[3] = A (valeur byte haute)
Data[4] = B (valeur byte basse)
```

---

## ⚡ 9. CAN-FD (TP3)

```c
#include <linux/can/raw.h>

/* Activer CAN-FD sur le socket */
int enable_canfd = 1;
setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_canfd, sizeof(enable_canfd));

/* Utiliser struct canfd_frame au lieu de can_frame */
struct canfd_frame fdframe;
fdframe.can_id  = 0x100;
fdframe.len     = 48;  /* 6 zones x 8 bytes */

/* Copier les données des 6 zones caméra (0xC00..0xC05) */
memcpy(&fdframe.data[0],  camera_data[0], 8);
memcpy(&fdframe.data[8],  camera_data[1], 8);
memcpy(&fdframe.data[16], camera_data[2], 8);
memcpy(&fdframe.data[24], camera_data[3], 8);
memcpy(&fdframe.data[32], camera_data[4], 8);
memcpy(&fdframe.data[40], camera_data[5], 8);

write(sock, &fdframe, sizeof(fdframe));
```

```bash
# Vérifier avec candump sur vcan2
candump vcan2
```

---

## ✅ 10. MISRA C (TP2b) — Règles clés à connaître

| Règle | Description | Obligatoire ? |
|-------|-------------|--------------|
| R 2.7 | Pas de paramètres non utilisés dans les fonctions | Advisory |
| R 3.1 | `/*` et `//` interdits dans un commentaire | Required |
| R 13.4 | Ne pas utiliser le résultat d'une assignation | Advisory |
| R 14.4 | Condition d'un `if` doit être de type booléen | Required |
| R 15.5 | Une seule instruction `return` par fonction | Advisory |
| R 17.7 | Valeur de retour d'une fonction non-void doit être utilisée | Required |

```bash
# Lancer l'analyse MISRA
cppcheck --enable=all --addon=misra road_follower.c

# Sur CPE (alias)
alias cppcheck='/softwares/INFO/Robotique/CppCheck/cppcheck'
```

**MISRA en 50 mots** : MISRA C est un ensemble de règles de codage en C destinées aux systèmes embarqués critiques (automobile, avionique). Il vise à éliminer les comportements indéfinis du langage C pouvant causer des bugs graves. Il comporte des règles obligatoires (*required*), recommandées (*advisory*) et des directives (*directive*).

---

## 🔁 11. Compilation

```bash
# Compiler un fichier C avec les libs socket
gcc -o vehicule_checker_student vehicule_checker_student.c

# Si besoin de threads
gcc -o dashboard dashboard.c -lpthread
```

---

## 📝 12. Checklist GitLab (à ne pas oublier !)

```bash
# Commit minimum 1x par demi-journée
git add .
git commit -m "TP1: code test.c avec filtres CAN"
git push

# Tags obligatoires à la fin de chaque TP
git tag TP1
git push origin TP1

git tag TP2a
git push origin TP2a

git tag TP2b
git push origin TP2b

git tag TP3
git push origin TP3
```

**Structure dossiers dans le repo :**
```
/
├── README.md          ← rapport principal
├── tp1/
│   └── test.c
├── tp2a/
│   ├── vehicule_checker_student.c
│   ├── dashboard.c
│   └── road_follower.c
├── tp2b/
│   └── road_follower_misra.c
└── tp3/
    ├── studentOBD2.c
    ├── UserOBD2Terminal.c
    └── sensorsCAN.c
```

---

## ⚡ 13. Aide-mémoire rapide pour l'examen

| Tâche | Commande / Code |
|-------|----------------|
| Créer vcan0 | `sudo modprobe vcan && sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0` |
| Écouter le bus | `candump vcan0` |
| Envoyer une trame | `cansend vcan0 321#320000` |
| Socket CAN | `socket(PF_CAN, SOCK_RAW, CAN_RAW)` |
| Bind interface | `strcpy(ifr.ifr_name, "vcan0"); ioctl(sock, SIOCGIFINDEX, &ifr)` |
| ID 29 bits | `frame.can_id = 0x8123 \| CAN_EFF_FLAG` |
| Lire RPM | `uint16_t rpm = (data[0] << 8) \| data[1]` |
| Steering signé | `frame.data[2] = (signed char)(-50)` |
| Activer CAN-FD | `setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_canfd, sizeof(enable_canfd))` |
| Analyse MISRA | `cppcheck --enable=all --addon=misra fichier.c` |

---

*Bonne chance ! 🚀*
