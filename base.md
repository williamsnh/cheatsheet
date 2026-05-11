## La structure d'un programme C — toujours la même

Tout programme que tu écriras dans ce cours a **exactement cette forme** :

```
┌─────────────────────────────┐
│  1. Les imports (outils)    │  ← toujours les mêmes, copie-colle
├─────────────────────────────┤
│  2. int main(void) {        │  ← le point de départ, toujours pareil
│                             │
│     3. Ouvrir le CAN        │  ← toujours pareil, copie-colle
│                             │
│     4. TON TRUC À TOI       │  ← c'est ici que tu changes des choses
│                             │
│     5. Fermer               │  ← toujours pareil, copie-colle
│  }                          │
└─────────────────────────────┘
```

**Les parties 1, 2, 3 et 5 ne changent jamais.** Tu les copies-colles à chaque fois. Seule la partie 4 change selon ce que tu veux faire.

---

## Le bloc à copier-coller sans jamais y toucher

```c
/* ====== PARTIE 1 : imports - ne jamais changer ====== */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int main(void) {

/* ====== PARTIE 2 : ouvrir le CAN - ne jamais changer ====== */
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_index;
    bind(s, (struct sockaddr *)&addr, sizeof(addr));

/* ====== PARTIE 3 : TON CODE ICI ====== */

    /* ... ce que tu veux faire ... */

/* ====== PARTIE 4 : fermer - ne jamais changer ====== */
    close(s);
    return 0;
}
```

---

## Les 3 actions que tu peux faire — et comment les écrire

### ACTION 1 — Envoyer une trame

> *"Je veux envoyer le message ID=0x321 avec les données 30, 0, 0"*

```c
frame.can_id  = 0x321;  /* ← remplace par l'ID que tu veux */
frame.can_dlc = 3;      /* ← remplace par le nombre d'octets */
frame.data[0] = 30;     /* ← 1er octet */
frame.data[1] = 0;      /* ← 2ème octet */
frame.data[2] = 0;      /* ← 3ème octet */
write(s, &frame, sizeof(frame));  /* ← envoyer (ne pas changer) */
```

**Règle simple :**
- Tu regardes la spec → elle te dit l'ID et ce que signifie chaque octet
- Tu remplis `frame.can_id`, `frame.can_dlc`, et `frame.data[0]`, `[1]`, `[2]`...
- Tu appelles `write()`

---

### ACTION 2 — Attendre et recevoir une trame

> *"Je veux attendre qu'une trame arrive et lire son contenu"*

```c
read(s, &frame, sizeof(frame));  /* ← attendre (ne pas changer) */

/* Maintenant frame contient le message reçu */
/* Tu peux lire : */
printf("ID reçu : %X\n", frame.can_id);   /* afficher l'ID */
printf("Octet 0 : %d\n", frame.data[0]);  /* afficher 1er octet */
printf("Octet 1 : %d\n", frame.data[1]);  /* afficher 2ème octet */
```

**Règle simple :**
- `read()` attend qu'un message arrive, puis le met dans `frame`
- Ensuite tu lis `frame.can_id` pour savoir quel message c'est
- Tu lis `frame.data[0]`, `frame.data[1]`... pour lire les données

---

### ACTION 3 — Répéter quelque chose en boucle

> *"Je veux envoyer la trame toutes les 100ms indéfiniment"*

```c
while (1) {               /* ← "répète pour toujours" */

    /* ... ton action ici ... */

    usleep(100000);       /* ← attendre 100ms (100 000 microsecondes) */
}
```

**Règle simple :**
- `while(1)` = boucle infinie (tourne jusqu'à Ctrl+C)
- `usleep(100000)` = pause de 100ms — change le nombre pour changer la vitesse
  - 100 000 = 100ms
  - 500 000 = 500ms = 0.5s
  - 1 000 000 = 1 seconde

---

## Exemple concret : faire avancer la voiture en boucle

Voici comment tu assembles les pièces :

```c
/* PARTIE FIXE - copie-colle */
#include <stdio.h>
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
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_index;
    bind(s, (struct sockaddr *)&addr, sizeof(addr));

    /* ====== MON CODE ====== */

    while (1) {
        /* D'après la spec : ID=0x321, octet0=gaz, octet1=frein, octet2=direction */
        frame.can_id  = 0x321;
        frame.can_dlc = 3;
        frame.data[0] = 30;   /* gaz 30% */
        frame.data[1] = 0;    /* pas de frein */
        frame.data[2] = 0;    /* tout droit */
        write(s, &frame, sizeof(frame));

        usleep(100000);  /* attendre 100ms avant de renvoyer */
    }

    /* ====== FIN FIXE ====== */
    close(s);
    return 0;
}
```

---

## Comment lire la spec pour remplir ton code

Prenons la trame de contrôle du TP :

```
ID : 0x321
DLC : 3
byte 0 → throttle   (0 à 100)
byte 1 → brake      (0 à 100)
byte 2 → steering   (-100 à +100)
```

Tu traduis ça directement :

| Spec dit | Tu écris |
|----------|----------|
| ID : 0x321 | `frame.can_id = 0x321;` |
| DLC : 3 | `frame.can_dlc = 3;` |
| byte 0 = throttle, je veux 50% | `frame.data[0] = 50;` |
| byte 1 = brake, je veux 0% | `frame.data[1] = 0;` |
| byte 2 = steering, je veux tourner à gauche | `frame.data[2] = (uint8_t)(-30);` |

Pour le steering négatif (gauche), il faut écrire `(uint8_t)(-30)` — le `(uint8_t)` devant est obligatoire quand la valeur est négative, sinon ça ne marche pas.

---

## Le mini-guide "si je veux faire X, j'écris Y"

| Je veux... | J'écris... |
|------------|------------|
| Avancer à X% | `frame.data[0] = X;` |
| Freiner à X% | `frame.data[1] = X;` |
| Tourner à droite | `frame.data[2] = 30;` (positif) |
| Tourner à gauche | `frame.data[2] = (uint8_t)(-30);` (négatif avec cast) |
| Tout droit | `frame.data[2] = 0;` |
| Attendre 1 seconde | `usleep(1000000);` |
| Afficher un texte | `printf("mon texte\n");` |
| Afficher un nombre | `printf("valeur : %d\n", mavariable);` |
| Répéter pour toujours | `while(1) { ... }` |

---

## Pour compiler et lancer — toujours pareil

```bash
# Compiler (une fois à chaque modification)
gcc -o nom_du_programme nom_du_fichier.c

# Lancer
./nom_du_programme
```
------------------------------------------------------------------

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

## La structure, colorée par blocs

```
┌─────────────────────────────────────────────┐
│  BLOC 1 : imports (outils)                  │  ← copie-colle, ne change jamais
│  #include <stdio.h>                         │
│  #include <stdlib.h>                        │
│  ...                                        │
│  #include <pthread.h>                       │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 2 : code AVANT le main()              │  ← NOUVEAU par rapport à avant
│  (des fonctions "helpers")                  │
│                                             │
│  static volatile int received[8] = {0};     │
│  static int all_received(void) { ... }      │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  int main(void) {                           │
│                                             │
│    BLOC 3 : ouvrir le CAN                  │  ← copie-colle, ne change jamais
│    int s;                                   │
│    ...                                      │
│    bind(...)                                │
│                                             │
│    BLOC 4 : TON CODE                        │  ← c'est ici que ça change !
│    (attente capteurs + séquence de conduite)│
│                                             │
│    BLOC 5 : fermer                          │  ← copie-colle, ne change jamais
│    close(s);                                │
│    return 0;                                │
│  }                                          │
└─────────────────────────────────────────────┘
```



## Bloc 4 découpé en 3 mini-parties

### Mini-partie A — Attendre les capteurs

```c
printf("Waiting for sensor frames 0xC00..0xC07...\n");
```
→ Affiche un message pour dire "j'attends..."

```c
while (!all_received()) {
```
→ **"Tant que je n'ai PAS reçu toutes les trames capteur, continue à attendre"**
Le `!` veut dire "NON". Donc `!all_received()` = "all_received() est FAUX"

```c
    int n = read(s, &frame, sizeof(frame));
```
→ Attend qu'une trame arrive. La met dans `frame`. `n` = nombre d'octets reçus.

```c
    if (n > 0) {
```
→ "Si j'ai bien reçu quelque chose" (n > 0 = oui j'ai reçu des données)

```c
        if (frame.can_id >= 0xC00 && frame.can_id <= 0xC07) {
```
→ "Si l'ID reçu est entre 0xC00 et 0xC07" (ce sont les trames capteurs du simulateur)

```c
            received[frame.can_id - 0xC00] = 1;
```
→ On note dans une case du tableau qu'on a reçu cette trame.
Par exemple si on reçoit 0xC02 : `0xC02 - 0xC00 = 2` → on coche la case n°2 ✅

```c
            printf("Got 0x%X\n", frame.can_id);
```
→ Affiche quel ID on vient de recevoir

```c
        }
    }
}  ← fin du while
```

**En résumé :** ce bloc tourne en rond et lit les trames qui arrivent. Dès qu'il a vu les 8 trames capteurs (0xC00 à 0xC07) au moins une fois, il s'arrête et passe à la suite.

---

### Mini-partie B — Avancer pendant 3 secondes

```c
for (int i = 0; i < 30; i++) {
```
→ **"Répète 30 fois"**. `i` commence à 0, augmente de 1 à chaque tour, s'arrête quand i = 30.
30 fois × 100ms = **3 secondes**

```c
    frame.can_id  = 0x321;   /* ID de contrôle véhicule */
    frame.can_dlc = 3;        /* 3 octets */
    frame.data[0] = 30;       /* gaz 30% */
    frame.data[1] = 0;        /* frein 0% */
    frame.data[2] = (uint8_t)0; /* tout droit */
    write(s, &frame, sizeof(frame)); /* envoyer */
    usleep(100000); /* attendre 100ms */
```
→ À chaque tour : prépare la trame, envoie, attend 100ms

**La différence avec `while(1)` :** le `for` s'arrête tout seul après 30 tours. Le `while(1)` ne s'arrête jamais.

---

### Mini-partie C — Freiner pendant 1 seconde

```c
frame.data[0] = 0;    /* gaz à 0% */
frame.data[1] = 50;   /* frein à 50% */
```
→ On modifie juste les données (le `can_id` et `can_dlc` sont déjà bons depuis avant, pas besoin de les réécrire)

```c
for (int i = 0; i < 10; i++) {
    write(s, &frame, sizeof(frame));
    usleep(100000);
}
```
→ 10 fois × 100ms = **1 seconde** de freinage

---

## Le Bloc 2 — la fonction "helper"

C'est la partie qui était AVANT le `main`. Elle sert juste à répondre à une question : *"est-ce que j'ai reçu toutes les trames ?"*

```c
static volatile int received[8] = {0};
```
→ Un tableau de 8 cases, toutes initialisées à 0 (= "pas encore reçu")
Quand on reçoit 0xC00 → case 0 passe à 1
Quand on reçoit 0xC01 → case 1 passe à 1... etc.

```c
static int all_received(void) {
    for (int i = 0; i < 8; i++) {
        if (!received[i]) return 0;  /* si une case est encore à 0 → retourne FAUX */
    }
    return 1;  /* toutes les cases sont à 1 → retourne VRAI */
}
```
→ Cette fonction regarde toutes les cases. Si l'une est encore à 0, elle dit "non pas encore". Si toutes sont à 1, elle dit "oui c'est bon !"

**Pourquoi elle est avant le `main` ?** Parce qu'en C, tu dois définir une fonction avant de l'utiliser. Comme une recette de cuisine : tu expliques ce qu'est une béchamel AVANT de dire "ajoute de la béchamel" dans ta recette principale.

---

## Le schéma global du programme

```
Démarrage
    ↓
Ouvrir vcan0
    ↓
Attendre 0xC00... ←──────────────┐
    ↓                            │
Trame reçue ?          Non → revenir attendre
    ↓ Oui
C'est un capteur ?     Non → ignorer
    ↓ Oui
Cocher la case
    ↓
Toutes les cases cochées ?   Non → revenir attendre
    ↓ Oui
Avancer 30 fois (3s)
    ↓
Freiner 10 fois (1s)
    ↓
Fermer et terminer
```

---

## Ce que tu dois retenir

| Concept | Ce que ça fait |
|---------|---------------|
| `while (!all_received())` | Boucle qui attend que tous les capteurs aient répondu |
| `for (int i = 0; i < 30; i++)` | Répète 30 fois puis s'arrête |
| `while(1)` | Répète pour toujours |
| `received[...]= 1` | Coche une case dans un tableau |
| `usleep(100000)` | Pause de 100ms |
| fonction avant le `main` | Une sous-recette qu'on utilise dans la recette principale |

------------------------------------------------------------------------------------------
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

## La métaphore des threads

Imagine que tu conduis une voiture avec un copilote :

- **Le copilote** regarde la route en permanence et note ce qu'il voit
- **Toi** tu conduis en te basant sur ce que dit le copilote

Les deux font leur travail **en même temps**, en parallèle. C'est exactement ça un thread.

```
Thread 1 (reader)     Thread 2 (control)
"je lis les capteurs"  "je conduis"
        ↓                    ↓
   lit 0xC00..0xC07      lit g_cam, g_speed
   met à jour g_cam      calcule steering
   met à jour g_speed    envoie 0x321
        ↓                    ↓
   recommence...         recommence...
```

Les deux tournent **en même temps** et se parlent via des **variables globales** (g_speed, g_cam).

---

## La structure du fichier

```
┌─────────────────────────────────────────────┐
│  BLOC 1 : imports                           │  ← copie-colle
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 2 : variables globales partagées      │  ← NOUVEAU
│  (le copilote et le conducteur se parlent   │
│   via ces variables)                        │
│  g_speed, g_cam, g_cam_ready                │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 3 : reader_thread()                   │  ← le "copilote"
│  (lit les capteurs en continu)              │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 4 : compute_steering()                │  ← une calculette
│  (calcule où tourner)                       │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 5 : control_thread()                  │  ← le "conducteur"
│  (envoie les commandes au véhicule)         │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 6 : main()                            │
│  - ouvre le CAN                             │  ← copie-colle
│  - lance les 2 threads                      │  ← NOUVEAU
│  - attend qu'ils finissent                  │
│  - ferme                                    │  ← copie-colle
└─────────────────────────────────────────────┘
```

---

## BLOC 2 — Les variables globales partagées

```c
static volatile uint8_t g_speed = 0;
```
→ La vitesse actuelle du véhicule. Commence à 0.
Le `g_` devant c'est juste une convention pour dire "c'est une variable Globale" (partagée entre les threads)

```c
static volatile uint8_t g_cam[6][8];
```
→ Un tableau qui stocke ce que voit la caméra.
Imagine un tableau à 6 lignes (les 6 zones de la caméra) et 8 colonnes (les 8 infos par zone).

```
         road stop yield crossing carpark ... ... ...
zone 0 [  92,   0,    0,       0,      0,  0,  0,  0 ]  ← full_left  (0xC00)
zone 1 [  80,   0,    5,       0,      0,  0,  0,  0 ]  ← left       (0xC01)
zone 2 [  64,   0,    0,       3,      0,  0,  0,  0 ]  ← middle_left(0xC02)
zone 3 [  64,   0,    0,       1,      0,  0,  0,  0 ]  ← middle_right(0xC03)
zone 4 [  44,   0,    0,       0,      0,  0,  0,  0 ]  ← right      (0xC04)
zone 5 [  30,   0,    0,       0,      0,  0,  0,  0 ]  ← full_right (0xC05)
```

```c
static volatile int g_cam_ready = 0;
```
→ Un drapeau. Vaut 0 au départ ("caméra pas encore prête"). Passe à 1 dès qu'on a reçu des données caméra.

Le mot `volatile` signifie : *"attention, cette variable peut être modifiée par un autre thread à tout moment, ne la mets pas en cache"*. C'est un détail technique, retiens juste que c'est obligatoire pour les variables partagées entre threads.

---

## BLOC 3 — reader_thread() : le copilote

```c
void *reader_thread(void *arg) {
    int s = *(int *)arg;
```
→ Le thread reçoit le socket `s` en paramètre. Le `*(int *)arg` c'est juste la façon C de dire "donne-moi le socket qu'on m'a passé". Copie-colle, ne change pas.

```c
    struct can_frame frame;
    while (1) {
        read(s, &frame, sizeof(frame));
```
→ Boucle infinie qui lit les trames CAN qui arrivent, une par une.

```c
        if (frame.can_id == 0xC07) {
            g_speed = frame.data[0];
        }
```
→ Si c'est la trame vitesse (0xC07), on sauvegarde la vitesse dans `g_speed`.
D'après la spec : byte 0 de 0xC07 = vitesse en km/h. Donc `frame.data[0]` = la vitesse.

```c
        if (frame.can_id >= 0xC00 && frame.can_id <= 0xC05) {
            int idx = (int)(frame.can_id - 0xC00);
```
→ Si c'est une trame caméra (0xC00 à 0xC05), on calcule l'index.
- 0xC00 → idx = 0 (zone full_left)
- 0xC01 → idx = 1 (zone left)
- 0xC02 → idx = 2 (zone middle_left)
- etc.

```c
            for (int i = 0; i < 8; i++)
                g_cam[idx][i] = frame.data[i];
```
→ On copie les 8 bytes de la trame dans la bonne ligne du tableau g_cam.
Si on reçoit 0xC02 (idx=2), on remplit `g_cam[2][0]`, `g_cam[2][1]`... `g_cam[2][7]`

```c
            g_cam_ready = 1;
        }
    }
    return NULL;
}
```
→ On lève le drapeau "caméra prête" et on recommence la boucle.

---

## BLOC 4 — compute_steering() : la calculette

C'est la formule du TP :
```
Steering = (zones gauches) - (zones droites)
```

Si la route est plus à gauche → résultat positif → tourner à gauche
Si la route est plus à droite → résultat négatif → tourner à droite

```c
int8_t compute_steering(void) {
    const int K[6] = {1, 2, 3, 3, 2, 1};
```
→ Les poids K pour chaque zone. Les zones du milieu comptent plus (poids 3) que les zones extrêmes (poids 1).

```
Zone :     full_left  left  mid_left  mid_right  right  full_right
Index :       0        1       2          3         4        5
Poids K :     1        2       3          3         2        1
```

```c
    int raw = 0;
    for (int i = 0; i < 3; i++)
        raw += K[i] * (int)g_cam[i][0];
```
→ Pour les 3 zones de gauche (i=0,1,2) : on ajoute `poids × quantité_de_route_vue`
`g_cam[i][0]` = byte 0 de la zone i = quantité de **route** vue (d'après la spec, byte 0 = road)

```c
    for (int i = 3; i < 6; i++)
        raw -= K[i] * (int)g_cam[i][0];
```
→ Pour les 3 zones de droite (i=3,4,5) : on soustrait

```c
    if (raw >  100) raw =  100;
    if (raw < -100) raw = -100;
    return (int8_t)raw;
}
```
→ On s'assure que le résultat reste entre -100 et +100 (les limites du steering dans la spec)

**Exemple concret :**
```
Si la route est surtout à gauche :
g_cam[0][0]=80, g_cam[1][0]=70, g_cam[2][0]=60  (beaucoup de route à gauche)
g_cam[3][0]=20, g_cam[4][0]=10, g_cam[5][0]=5   (peu de route à droite)

raw = (1×80 + 2×70 + 3×60) - (3×20 + 2×10 + 1×5)
    = (80+140+180) - (60+20+5)
    = 400 - 85 = 315 → plafonné à 100
→ steering = +100 → tourner à gauche à fond
```

---

## BLOC 5 — control_thread() : le conducteur

```c
void *control_thread(void *arg) {
    int s = *(int *)arg;
    struct can_frame frame;
```
→ Même début que reader_thread, copie-colle.

```c
    while (!g_cam_ready) usleep(10000);
```
→ "Tant que la caméra n'est pas prête, attends 10ms et recommence."
Le conducteur attend que le copilote ait des infos avant de démarrer.

```c
    while (1) {
        int8_t steering = compute_steering();
```
→ Boucle infinie. À chaque tour, on calcule le steering grâce à la calculette.

```c
        uint8_t throttle = (g_speed < 45) ? 30U : 5U;
```
→ **Opérateur ternaire** — c'est un `if` condensé sur une ligne.
Ça se lit : *"si vitesse < 45 km/h alors throttle=30, sinon throttle=5"*

En version longue ça donnerait :
```c
if (g_speed < 45) {
    throttle = 30;
} else {
    throttle = 5;
}
```

```c
        uint8_t brake = (g_speed > 55) ? 30U : 0U;
```
→ Pareil : *"si vitesse > 55 km/h alors freine à 30%, sinon pas de frein"*

**La logique de vitesse en résumé :**
```
Vitesse < 45  →  gaz 30%, frein 0%   (on accélère)
45 < Vitesse < 55  →  gaz 5%, frein 0%   (on lève le pied)
Vitesse > 55  →  gaz 5%, frein 30%   (on freine)
```

```c
        frame.can_id  = 0x321;
        frame.can_dlc = 3;
        frame.data[0] = throttle;
        frame.data[1] = brake;
        frame.data[2] = (uint8_t)steering;
        write(s, &frame, sizeof(frame));
        usleep(50000);
    }
```
→ On envoie la trame de contrôle avec les valeurs calculées, toutes les 50ms (20 fois par seconde).

---

## BLOC 6 — main() : le chef d'orchestre

```c
    pthread_t r_tid, c_tid;
```
→ On crée deux "identifiants" pour nos deux threads (r_tid = reader, c_tid = control)

```c
    pthread_create(&r_tid, NULL, reader_thread,  &s);
    pthread_create(&c_tid, NULL, control_thread, &s);
```
→ On **lance** les deux threads. À partir de là, reader_thread et control_thread tournent en parallèle.

La syntaxe `pthread_create` c'est toujours :
```c
pthread_create(&identifiant, NULL, nom_de_la_fonction, paramètre_à_passer);
```

```c
    pthread_join(r_tid, NULL);
    pthread_join(c_tid, NULL);
```
→ "Attends que les threads se terminent avant de continuer."
Comme les threads sont des `while(1)`, ils ne se terminent jamais → le programme reste actif jusqu'à Ctrl+C.

---

## Le schéma final — tout ensemble

```
main()
  │
  ├── ouvre vcan0
  │
  ├── lance reader_thread ──────────────────────────────┐
  │                                                      │
  ├── lance control_thread ──────────────┐              │
  │                                      │              │
  └── attend (pthread_join)              │              │
                                         ▼              ▼
                              THREAD 2 (conducteur)   THREAD 1 (copilote)
                              attend g_cam_ready      lit les trames CAN
                                      │               met à jour g_speed
                                      │               met à jour g_cam
                                      │               lève g_cam_ready
                                      ▼
                              calcule steering
                              calcule throttle/brake
                              envoie 0x321
                              attend 50ms
                              recommence...
```

------------------------------------------------------------------------------------

### lit les trames 0xC00 a 0xC07 et les affichent sans threads : 
```c
// interface.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int s;
int speed = 0;
int gear = 0;
int motor_speed = 0;
int road[6] = {0}; /* données caméra pour chaque zone */

int main() {
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    /* Création du socket CAN */
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket");
        return 1;
    }

    strcpy(ifr.ifr_name, "vcan0");
    ioctl(s, SIOCGIFINDEX, &ifr);
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        return 1;
    }

    while (1) {
        read(s, &frame, sizeof(struct can_frame));

        unsigned int id = frame.can_id & CAN_EFF_MASK;

        if (id == 0xC06) {
            /* Motor speed sur 2 octets */
            motor_speed = (frame.data[0] << 8) | frame.data[1];
        }
        else if (id == 0xC07) {
            /* Vehicle speed et gear */
            speed = frame.data[0];
            gear  = frame.data[1];
        }
        else if (id >= 0xC00 && id <= 0xC05) {
            /* Données caméra : byte 0 = road */
            road[id - 0xC00] = frame.data[0];
        }

        /* Calcul direction (formule du sujet) */
        int steering = (road[0] + road[1] + road[2]) - (road[3] + road[4] + road[5]);
        char *direction;
        if (steering > 10)
            direction = "<-";
        else if (steering < -10)
            direction = "->";
        else
            direction = "^";

        /* Affichage */
        printf("\033[2J\033[H"); /* efface le terminal */
        printf("Speed: %d km/h\n", speed);
        printf("Gear: %d\n", gear);
        printf("Motor speed: %d rpm\n", motor_speed);
        printf("Action to follow the road: %s\n", direction);
    }

    close(s);
    return 0;
}
```
Parfait, merci ! Ce code est en fait **plus simple** que le road_follower — pas de threads ici, tout se passe dans le `main` ! On va le découper ensemble. 😊

---

## La structure globale

```
┌─────────────────────────────────────────────┐
│  BLOC 1 : imports                           │  ← copie-colle
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 2 : variables globales                │  ← NOUVEAU
│  speed, gear, motor_speed, road[6]          │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  int main() {                               │
│                                             │
│    BLOC 3 : ouvrir le CAN                  │  ← copie-colle (légèrement différent)
│                                             │
│    BLOC 4 : boucle infinie while(1)         │
│      - lire une trame                       │
│      - mettre à jour les variables          │
│      - calculer la direction                │
│      - afficher le dashboard                │
│  }                                          │
└─────────────────────────────────────────────┘
```

---

## BLOC 2 — Les variables globales

```c
int s;
int speed = 0;
int gear = 0;
int motor_speed = 0;
int road[6] = {0};
```

Ces variables sont déclarées **en dehors du main**, tout en haut. Ça veut dire qu'elles sont accessibles partout dans le fichier.

- `s` → le socket CAN (la connexion)
- `speed` → la vitesse du véhicule en km/h
- `gear` → le rapport de boîte (1, 2, 3...)
- `motor_speed` → le régime moteur en RPM
- `road[6]` → un tableau de 6 cases, une par zone de caméra

```
road[0] = quantité de route vue zone full_left  (0xC00)
road[1] = quantité de route vue zone left       (0xC01)
road[2] = quantité de route vue zone middle_left(0xC02)
road[3] = quantité de route vue zone middle_right(0xC03)
road[4] = quantité de route vue zone right      (0xC04)
road[5] = quantité de route vue zone full_right (0xC05)
```

---

## BLOC 3 — Ouvrir le CAN (légèrement différent du template)

Tu remarques deux petites différences par rapport au code de base :

**Différence 1 — vérification des erreurs**
```c
if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
    perror("Socket");
    return 1;
}
```
→ Ici l'ouverture du socket ET la vérification d'erreur sont faites en même temps.
C'est la règle MISRA R13.4 qui dit que c'est pas idéal, mais ça fonctionne.
En version "propre" ça donnerait :
```c
s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
if (s < 0) { perror("Socket"); return 1; }
```

**Différence 2 — memset**
```c
memset(&addr, 0, sizeof(addr));
```
→ Ça remet tous les octets de `addr` à zéro avant de le remplir. C'est une bonne pratique pour éviter d'avoir des valeurs parasites dans la mémoire. `memset` = "memory set" = remplir la mémoire avec une valeur.

**Différence 3 — nom du champ**
```c
addr.can_ifindex = ifr.ifr_ifindex;  /* ton code */
addr.can_ifindex = ifr.ifr_index;    /* mon template */
```
→ C'est la même chose, juste deux noms différents selon la version du système. Les deux marchent.

---

## BLOC 4 — La boucle infinie while(1)

C'est le cœur du programme. À chaque tour de boucle, il se passe **4 choses** :

---

### Étape 1 — Lire une trame

```c
read(s, &frame, sizeof(struct can_frame));
```
→ On attend qu'une trame arrive sur vcan0 et on la met dans `frame`. Le programme est **bloqué ici** jusqu'à ce qu'une trame arrive.

```c
unsigned int id = frame.can_id & CAN_EFF_MASK;
```
→ On extrait l'ID de la trame. Le `& CAN_EFF_MASK` sert à "nettoyer" l'ID en enlevant des bits parasites qui pourraient traîner (notamment le flag 29 bits). Retiens juste que c'est la bonne façon de lire un ID.

---

### Étape 2 — Mettre à jour les variables

```c
if (id == 0xC06) {
    motor_speed = (frame.data[0] << 8) | frame.data[1];
}
```
→ Si c'est la trame moteur (0xC06), on décode le RPM.
D'après la spec, le RPM est sur **2 octets**. On les assemble ainsi :

```
frame.data[0] = octet fort  (ex: 0x6E)
frame.data[1] = octet faible (ex: 0x17)

(0x6E << 8) = 0x6E00
0x6E00 | 0x17 = 0x6E17 = 5998 rpm ✅

Le << 8 veut dire "décale de 8 bits vers la gauche"
= "mets cet octet en position haute"
```

```c
else if (id == 0xC07) {
    speed = frame.data[0];
    gear  = frame.data[1];
}
```
→ Si c'est la trame vitesse (0xC07) :
- byte 0 = vitesse en km/h → on met dans `speed`
- byte 1 = rapport de boîte → on met dans `gear`

```c
else if (id >= 0xC00 && id <= 0xC05) {
    road[id - 0xC00] = frame.data[0];
}
```
→ Si c'est une trame caméra, on stocke juste le byte 0 (= quantité de route) dans la bonne case de `road[]`.
- 0xC00 → `road[0]`
- 0xC01 → `road[1]`
- etc.

---

### Étape 3 — Calculer la direction

```c
int steering = (road[0] + road[1] + road[2]) - (road[3] + road[4] + road[5]);
```
→ C'est la formule du sujet simplifiée (sans les poids K ici, tout vaut 1) :
- On additionne la route vue à **gauche** (zones 0, 1, 2)
- On soustrait la route vue à **droite** (zones 3, 4, 5)

```
Si beaucoup de route à gauche → résultat positif → tourner à gauche (<-)
Si beaucoup de route à droite → résultat négatif → tourner à droite (->)
Si équilibré → résultat proche de 0 → tout droit (^)
```

```c
char *direction;
if (steering > 10)
    direction = "<-";
else if (steering < -10)
    direction = "->";
else
    direction = "^";
```
→ On traduit le nombre en flèche. Le seuil ±10 évite de changer de direction pour rien quand c'est presque droit.

`char *direction` = une variable qui contient du texte (une chaîne de caractères).

---

### Étape 4 — Afficher le dashboard

```c
printf("\033[2J\033[H");
```
→ Efface l'écran du terminal. C'est un code ANSI (une commande spéciale pour le terminal). Sans ça, les lignes s'accumuleraient au lieu de se rafraîchir.

```c
printf("Speed: %d km/h\n", speed);
printf("Gear: %d\n", gear);
printf("Motor speed: %d rpm\n", motor_speed);
printf("Action to follow the road: %s\n", direction);
```
→ Affiche les 4 lignes du dashboard.
- `%d` = affiche un nombre entier
- `%s` = affiche du texte (string)
- `\n` = saut de ligne

---

## Le schéma d'un tour de boucle

```
      ┌─────────────────────────────────────┐
      │         while(1) — un tour          │
      │                                     │
      │  1. read() → attendre une trame     │
      │         ↓                           │
      │  2. C'est quelle trame ?            │
      │     0xC06 → mettre à jour RPM       │
      │     0xC07 → mettre à jour speed/gear│
      │     0xC00-0xC05 → mettre à jour road│
      │         ↓                           │
      │  3. Calculer steering               │
      │     gauche - droite = ?             │
      │     → "<-" / "^" / "->"            │
      │         ↓                           │
      │  4. Effacer écran + afficher        │
      └──────────────┬──────────────────────┘
                     │
                     └── recommencer depuis le début
```

-----------------------------------
### >Lit sur un CAN et écrit dans un autre : 

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#define OBD2_REQ_ID     0x7DFU
#define OBD2_RESP_ID    0x7E8U
#define PID_RPM         0x0CU
#define PID_SPEED       0x0DU
#define PID_THROTTLE    0x11U

#define CAN_ID_RPM      0xC06U
#define CAN_ID_SPEED    0xC07U
#define CAN_ID_CMD      0x321U  /* trame road_follower : data[0]=throttle */

static volatile int g_rpm      = 0;
static volatile int g_speed    = 0;
static volatile int g_throttle = 0;

static int s_vcan0     = -1;
static int s_vcan1_req = -1;  /* lecture requêtes OBD2 */
static int s_vcan1_ans = -1;  /* écriture réponses OBD2 */

/* ── Ouvre un socket CAN ── */
static int open_can(const char *iface)
{
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;

    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) { perror("socket"); return -1; }

    (void)strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1U);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) { perror("ioctl"); return -1; }

    (void)memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return -1;
    }
    return s;
}

/* ── Thread : lit vcan0 → RPM, speed, throttle ── */
static void *thread_read_vcan0(void *arg)
{
    struct can_frame frame;
    unsigned int id;
    (void)arg;

    while (1) {
        if (read(s_vcan0, &frame, sizeof(frame)) < 0) {
            perror("read vcan0");
            break;
        }
        id = frame.can_id & CAN_EFF_MASK;

        if (id == CAN_ID_RPM) {
            //g_rpm = ((int)frame.data[0] << 8) | (int)frame.data[1];
            g_rpm = ((int)frame.data[1] << 8) | (int)frame.data[0];
        }
        if (id == CAN_ID_SPEED) {
            g_speed = (int)frame.data[0];
        }
        if (id == CAN_ID_CMD) {
            /* data[0] = throttle en % (0-100) */
            g_throttle = (int)frame.data[0];
        }
    }
    return NULL;
}

/* ── Envoie une réponse OBD2 sur vcan1 ── */
static void send_obd2_response(uint8_t pid, uint8_t A, uint8_t B)
{
    struct can_frame resp;
    (void)memset(&resp, 0, sizeof(resp));

    resp.can_id  = OBD2_RESP_ID;
    resp.can_dlc = 8U;
    resp.data[0] = 0x03U;
    resp.data[1] = 0x41U;
    resp.data[2] = pid;
    resp.data[3] = A;
    resp.data[4] = B;

    if (write(s_vcan1_ans, &resp, sizeof(resp)) < 0) {
        perror("write vcan1");
    }
}

/* ── main ── */
int main(void)
{
    pthread_t tid;
    struct can_frame req;
    unsigned int id;
    uint8_t pid;

    s_vcan0     = open_can("vcan0");
    s_vcan1_req = open_can("vcan1");  /* lecture requêtes */
    s_vcan1_ans = open_can("vcan1");  /* écriture réponses */

    if (s_vcan0 < 0 || s_vcan1_req < 0 || s_vcan1_ans < 0) {
        return EXIT_FAILURE;
    }

    if (pthread_create(&tid, NULL, thread_read_vcan0, NULL) != 0) {
        perror("pthread_create");
        return EXIT_FAILURE;
    }

    (void)printf("=== studentOBD2 actif (vcan0->vcan1) ===\n");

    while (1) {
        if (read(s_vcan1_req, &req, sizeof(req)) < 0) {
            perror("read vcan1");
            break;
        }

        id = req.can_id & CAN_EFF_MASK;
        if (id != OBD2_REQ_ID)    { continue; }
        if (req.data[1] != 0x01U) { continue; }

        pid = req.data[2];
        (void)printf("Requete PID 0x%02X\n", (unsigned int)pid);

        if (pid == PID_SPEED) {
            send_obd2_response(PID_SPEED, (uint8_t)g_speed, 0x00U);
        }
        else if (pid == PID_RPM) {
            int raw = g_rpm;
            send_obd2_response(PID_RPM,
                               (uint8_t)((raw >> 8) & 0xFF),
                               (uint8_t)(raw        & 0xFF));
        }
        else if (pid == PID_THROTTLE) {
            uint8_t raw = (uint8_t)((g_throttle * 255) / 100);
            send_obd2_response(PID_THROTTLE, raw, 0x00U);
        }
        else {
            (void)printf("PID 0x%02X non supporte\n", (unsigned int)pid);
        }
    }

    return EXIT_SUCCESS;
}
```

## La métaphore pour comprendre

Imagine un **traducteur** dans une réunion entre deux personnes qui ne parlent pas la même langue :

```
Simulateur (vcan0)          Traducteur (studentOBD2)          Terminal OBD2 (vcan1)
"voici speed=27,            ←── lit en permanence              "donne-moi la vitesse !"
 rpm=2700..."                                                          ↓
                             stocke les valeurs                 ← "vitesse = 27 km/h"
```

- **vcan0** = le bus du simulateur (plein de trames capteurs)
- **vcan1** = le bus OBD2 (le terminal pose des questions, studentOBD2 répond)
- **studentOBD2** = le traducteur qui écoute les deux en même temps

---

## La structure globale

```
┌─────────────────────────────────────────────┐
│  BLOC 1 : imports + définitions (#define)   │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 2 : variables globales partagées      │
│  g_rpm, g_speed, g_throttle                 │
│  s_vcan0, s_vcan1_req, s_vcan1_ans          │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 3 : open_can()                        │
│  une fonction pour ouvrir UN bus CAN        │
│  (utilisée 3 fois dans le main)             │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 4 : thread_read_vcan0()               │
│  lit les capteurs du simulateur en continu  │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 5 : send_obd2_response()              │
│  envoie une réponse OBD2 sur vcan1          │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  BLOC 6 : main()                            │
│  - ouvre vcan0 ET vcan1                     │
│  - lance le thread de lecture               │
│  - boucle : attend requêtes OBD2            │
│             et répond                       │
└─────────────────────────────────────────────┘
```

---

## BLOC 1 — Les #define : donner des noms aux nombres

```c
#define OBD2_REQ_ID     0x7DFU
#define OBD2_RESP_ID    0x7E8U
#define PID_RPM         0x0CU
#define PID_SPEED       0x0DU
#define PID_THROTTLE    0x11U
```

Un `#define` c'est comme créer un **surnom** pour un nombre. Partout où tu écrirais `0x7DF`, tu écris `OBD2_REQ_ID` à la place. C'est plus lisible et si tu veux changer la valeur, tu le fais à un seul endroit.

Le `U` à la fin veut dire "Unsigned" (nombre positif). C'est une règle MISRA.

```
OBD2_REQ_ID  = 0x7DF  → l'ID que le terminal envoie pour faire une requête
OBD2_RESP_ID = 0x7E8  → l'ID avec lequel studentOBD2 répond
PID_RPM      = 0x0C   → code OBD2 pour demander le régime moteur
PID_SPEED    = 0x0D   → code OBD2 pour demander la vitesse
PID_THROTTLE = 0x11   → code OBD2 pour demander l'accélérateur
```

---

## BLOC 2 — Les variables globales

```c
static volatile int g_rpm      = 0;
static volatile int g_speed    = 0;
static volatile int g_throttle = 0;
```
→ Les 3 valeurs que le thread lit sur vcan0 et que le main utilise pour répondre.
Même principe que dans road_follower : `volatile` car partagées entre threads.

```c
static int s_vcan0     = -1;
static int s_vcan1_req = -1;
static int s_vcan1_ans = -1;
```
→ Les 3 sockets (connexions CAN). **C'est la grande nouveauté de ce code : 3 sockets !**

```
s_vcan0     → connecté à vcan0 (lit les capteurs du simulateur)
s_vcan1_req → connecté à vcan1 (lit les REQUÊTES OBD2 qui arrivent)
s_vcan1_ans → connecté à vcan1 (envoie les RÉPONSES OBD2)
```

Pourquoi deux sockets sur vcan1 ? Pour séparer proprement la lecture et l'écriture. C'est une bonne pratique.

---

## BLOC 3 — open_can() : la fonction "ouvre un bus"

C'est exactement le code d'init CAN qu'on copie-colle d'habitude dans le main, mais mis dans **une fonction séparée** pour pouvoir l'appeler 3 fois facilement.

```c
static int open_can(const char *iface)
{
```
→ La fonction prend en paramètre le **nom de l'interface** (`"vcan0"` ou `"vcan1"`) et retourne le socket.

```c
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    (void)strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1U);
```
→ `strncpy` c'est comme `strcpy` mais plus sécurisé (limite le nombre de caractères copiés). Le `(void)` devant dit "je sais que cette fonction retourne quelque chose mais je l'ignore volontairement" — c'est encore une règle MISRA.

```c
    return s;
}
```
→ Elle retourne le numéro du socket. Dans le main on l'appelle comme ça :
```c
s_vcan0     = open_can("vcan0");
s_vcan1_req = open_can("vcan1");
s_vcan1_ans = open_can("vcan1");
```

---

## BLOC 4 — thread_read_vcan0() : le copilote

```c
static void *thread_read_vcan0(void *arg)
{
    (void)arg;   /* on n'utilise pas le paramètre → MISRA R2.7 */
```

```c
    while (1) {
        if (read(s_vcan0, &frame, sizeof(frame)) < 0) {
            perror("read vcan0");
            break;   /* si erreur de lecture → on sort de la boucle */
        }
        id = frame.can_id & CAN_EFF_MASK;
```
→ Lit les trames de vcan0 en continu. On vérifie que `read` ne retourne pas une erreur (< 0).

```c
        if (id == CAN_ID_RPM) {
            g_rpm = ((int)frame.data[1] << 8) | (int)frame.data[0];
        }
```
→ Pour le RPM (0xC06) : assemble les 2 octets. 
⚠️ **Remarque** : tu vois le commentaire juste au-dessus ?
```c
//g_rpm = ((int)frame.data[0] << 8) | (int)frame.data[1];
g_rpm =  ((int)frame.data[1] << 8) | (int)frame.data[0];
```
La ligne commentée c'est la version "logique" (octet fort en premier). La ligne active a les octets **inversés** — c'est parce que le simulateur envoie dans l'ordre inverse (little-endian). Ton prof a dû le découvrir en testant !

```c
        if (id == CAN_ID_SPEED) {
            g_speed = (int)frame.data[0];   /* byte 0 = vitesse km/h */
        }
        if (id == CAN_ID_CMD) {
            g_throttle = (int)frame.data[0]; /* byte 0 = throttle % */
        }
```
→ Même principe : on met à jour les variables globales au fur et à mesure.

---

## BLOC 5 — send_obd2_response() : fabriquer une réponse OBD2

C'est ici qu'on fabrique la trame de réponse OBD2. Le format est standardisé :

```
byte 0 : 0x03       → "j'envoie 3 octets de données"
byte 1 : 0x41       → "c'est une réponse au mode 01"  (0x40 + 0x01)
byte 2 : pid        → quel paramètre on répond (RPM, speed...)
byte 3 : A          → valeur (octet haut ou valeur unique)
byte 4 : B          → valeur (octet bas, parfois 0x00)
```

```c
static void send_obd2_response(uint8_t pid, uint8_t A, uint8_t B)
{
    struct can_frame resp;
    (void)memset(&resp, 0, sizeof(resp));  /* tout à zéro d'abord */

    resp.can_id  = OBD2_RESP_ID;   /* 0x7E8 */
    resp.can_dlc = 8U;
    resp.data[0] = 0x03U;
    resp.data[1] = 0x41U;
    resp.data[2] = pid;
    resp.data[3] = A;
    resp.data[4] = B;

    write(s_vcan1_ans, &resp, sizeof(resp));  /* envoie sur vcan1 */
}
```

---

## BLOC 6 — main() : le chef d'orchestre

```c
    s_vcan0     = open_can("vcan0");
    s_vcan1_req = open_can("vcan1");
    s_vcan1_ans = open_can("vcan1");
```
→ On ouvre les 3 connexions grâce à la fonction open_can.

```c
    if (s_vcan0 < 0 || s_vcan1_req < 0 || s_vcan1_ans < 0) {
        return EXIT_FAILURE;
    }
```
→ Si l'une des 3 connexions a échoué (valeur < 0), on arrête tout.
`EXIT_FAILURE` = façon propre de dire "le programme s'est terminé avec une erreur" (vaut 1).
`EXIT_SUCCESS` = "tout s'est bien passé" (vaut 0). C'est MISRA qui préfère ces noms à 0 et 1.

```c
    pthread_create(&tid, NULL, thread_read_vcan0, NULL);
```
→ On lance le thread qui lit vcan0 en permanence. À partir de là, `g_rpm`, `g_speed` et `g_throttle` se mettent à jour tout seuls en arrière-plan.

### La boucle principale — attendre et répondre aux requêtes

```c
    while (1) {
        read(s_vcan1_req, &req, sizeof(req));
```
→ On attend qu'une requête OBD2 arrive sur vcan1.

```c
        id = req.can_id & CAN_EFF_MASK;
        if (id != OBD2_REQ_ID) { continue; }
```
→ Si ce n'est pas une requête OBD2 (ID != 0x7DF), on ignore et on recommence.
`continue` = "passe directement au prochain tour de boucle".

```c
        if (req.data[1] != 0x01U) { continue; }
```
→ Le byte 1 doit valoir 0x01 (= "mode courant" en OBD2). Sinon on ignore.

```c
        pid = req.data[2];
```
→ Le byte 2 contient le PID demandé (0x0C = RPM, 0x0D = speed, 0x11 = throttle).

```c
        if (pid == PID_SPEED) {
            send_obd2_response(PID_SPEED, (uint8_t)g_speed, 0x00U);
        }
```
→ Si on demande la vitesse : on répond avec `g_speed` en octet A, 0 en octet B.

```c
        else if (pid == PID_RPM) {
            int raw = g_rpm;
            send_obd2_response(PID_RPM,
                               (uint8_t)((raw >> 8) & 0xFF),
                               (uint8_t)(raw        & 0xFF));
        }
```
→ Pour le RPM : la valeur est sur 2 octets donc on la découpe.
```
raw = 2700
raw >> 8        = 0x0A  → octet A (partie haute)
raw & 0xFF      = 0x8C  → octet B (partie basse)
```

```c
        else if (pid == PID_THROTTLE) {
            uint8_t raw = (uint8_t)((g_throttle * 255) / 100);
            send_obd2_response(PID_THROTTLE, raw, 0x00U);
        }
```
→ Le throttle est en % (0-100) mais OBD2 veut une valeur entre 0 et 255. On convertit :
```
g_throttle = 30%
30 * 255 / 100 = 76  → c'est la valeur OBD2 correspondante
```

---

## Le schéma complet

```
        vcan0                    studentOBD2                    vcan1
          │                           │                           │
          │   0xC06 (RPM)             │                           │
          │──────────────────► thread_read_vcan0                  │
          │   0xC07 (speed)           │ met à jour                │
          │──────────────────►  g_rpm, g_speed, g_throttle        │
          │   0x321 (throttle)        │                           │
          │──────────────────►        │                           │
          │                           │                           │
          │                        main()  ◄── 0x7DF PID=0x0D ───│
          │                           │    (demande speed)        │
          │                           │                           │
          │                           │──── 0x7E8 speed=27 ──────►│
          │                           │    (réponse)              │
```


------------------------------------------------------------------

# Réponses CAN BUS

### 1. Développez l'acronyme CAN

**Controller Area Network**

---

### 2. Est-ce que le bus CAN peut avoir plusieurs maîtres ?

✅ **Oui**

Le CAN est un bus **multi-maître** : n’importe quel nœud peut transmettre si le bus est libre.

---

### 3. Peut-on mixer du CAN 2.0A et du CAN 2.0B sur un même BUS ?

✅ **Oui**

À condition que les contrôleurs soient compatibles.
CAN 2.0A = identifiant 11 bits
CAN 2.0B = identifiant 29 bits

---

### 4. Si une trame ne contient pas de données, ça peut-être parce que

✅ **DLC = 0**
✅ **RTR = 1**

Explications :

* DLC=0 → longueur des données nulle
* RTR=1 → trame Remote (demande de données)

❌ CRC=0 → faux
❌ SOF=1 → faux (SOF est dominant = 0)

---

### 5. Sélectionnez l'ID le plus prioritaire

* 0x123
* 0x321
* 0xC801

✅ **0x123**

En CAN, **plus l’identifiant est petit, plus la priorité est haute**.

---

### 6. Combien de données peut-on transférer dans une trame CAN (hors CAN-FD, hors identifiant) ?

✅ **8 octets**

---

### 7. Combien de données peut-on transférer dans une trame CAN-FD ?

✅ **64 octets**

---

# Rendement des trames

### 8. Rendement d’une trame CAN2.0B avec 8 octets de données

En CAN 2.0B :

* données utiles = 8 octets = 64 bits
* taille typique trame ≈ 128 bits (avec overhead)

Donc :

[
\eta = \frac{64}{128} = 0.5
]

\eta = \frac{64}{128} = 0.5 = 50%

✅ **Rendement ≈ 50 %**


### Explication du champ d’application

* **CAN** → optimisé pour petites données temps réel embarquées (automobile, industriel)
* **Ethernet** → optimisé pour gros volumes de données réseau

---

### 10. Quelle type de trame fait une demande d'information ?

✅ **remote**

---

### 11. Quelle trame viole sciemment les règles des trames CAN ?

✅ **error**

La trame d’erreur force une violation pour signaler une erreur.

---

### 12. Qui peut initier une trame sur un bus idle ?

✅ **N'importe quel contrôleur CAN**

---

### 13. Le CAN utilise des adresses pour savoir à qui envoyer un message

✅ **Faux**

Le CAN fonctionne par **identifiants de messages**, pas par adresses.

---

### 14. Quand deux contrôleurs démarrent en même temps, un contrôleur gagne l'arbitrage en envoyant un bit récessif quand les autres envoient un bit dominant

✅ **Faux**

C’est l’inverse :

* dominant = 0
* récessif = 1

Le gagnant envoie le bit dominant.

---

### 15. Un bus CAN

✅ **doit avoir une résistance de 120 ohms aux 2 extrémités du bus**
✅ **est prévu pour fonctionner avec une paire torsadée blindée**
✅ **ne doit pas dépasser 40 m à 1 Mbit/s**

Les 3 affirmations sont vraies.

---

### 16. Le bit stuffing consiste à ajouter un bit inverse si _____ bits consécutifs sont observés

✅ **5**

Après 5 bits identiques, on ajoute un bit inverse.

---

### 17. Ligne de commande slcand

Commande :

```bash
sudo slcand -o -s8 -t hw -S 3000000 /dev/ttyUSB0 slcan0
```

❌ **Faux**

Car :

* `-s8` correspond à **1 Mbit/s**
* `-S 3000000` = 3 Mbit/s UART

Ce n’est donc PAS un bus CAN à 300000 bauds.

