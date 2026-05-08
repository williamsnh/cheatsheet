# 📘 FICHE DE RÉVISION – SAT (Partie Connaissances)

---

# 1️⃣ Introduction – Contexte & Enjeux

## 🎯 Pourquoi ce cours ?
- Beaucoup d’ingénieurs travaillent dans les transports : automobile, ferroviaire, camions, robotaxis.
- Besoin croissant dû à l’autonomie des véhicules.
- Objectif : bases en réseaux embarqués, perception, sécurité.

## 🌍 Enjeux sociétaux
- Plus d’un milliard de voitures dans le monde.
- 1,2M de morts/an sur la route.
- 70% de la population en ville en 2050.
- Objectif : réduire bouchons, pollution, accidents → flottes autonomes partagées.

---

# 2️⃣ Terminologie & Niveaux d’autonomie

## 🔤 Acronymes
- **SDV** : Self‑Driving Vehicle  
- **ADAS** : Advanced Driver Assistance Systems  
- **DBW** : Drive‑By‑Wire  
- **V2V / V2I / V2X**  
- **EV** : Electric Vehicle  
- **V2G / V2H**

## 🧠 Niveaux SAE (J3016)
| Niveau | Nom | Qui conduit ? | Capacités |
|-------|------|----------------|-----------|
| **0** | No automation | Conducteur | Aucune assistance |
| **1** | Driver assistance | Conducteur | Direction **ou** accélération |
| **2** | Partial automation | Conducteur | Direction **et** accélération |
| **3** | Conditional automation | Véhicule (reprise rapide) | Le système détecte ses limites |
| **4** | High automation | Véhicule | Autonome en zones limitées |
| **5** | Full automation | Véhicule | Autonome partout |

---

# 3️⃣ Perception – Capteurs

## 🧩 Types de capteurs
- **Caméras** : riches en infos, sensibles météo/lumière.  
- **Lidars** : distances précises, coûteux.  
- **Radars** : robustes, longue portée.  
- **Ultrasons** : courte portée.  
- **Fusion de capteurs** indispensable.

---

# 4️⃣ In‑Vehicle Networking (IVN)

## 🎯 Objectifs
- Réduire câblage.
- Résister au bruit électromagnétique.
- Multi‑master.
- Arbitration non destructive.

## 🚌 Avant / Après bus
- Avant : architecture point‑à‑point → câbles partout.  
- Après : BUS → centralisation.

## 📡 Débits (du plus lent au plus rapide)
- **LIN** (10 kbit/s)  
- **CAN** (1 Mbit/s)  
- **CAN‑FD** (8 Mbit/s)  
- **FlexRay** (10 Mbit/s)  
- **Ethernet Auto** (100 Mbit/s)

---

# 5️⃣ CAN Bus – Le cœur du QCM

## 🧱 Caractéristiques
- Bus **broadcast**.
- Pas d’adresse → filtrage par identifiant.
- **0 = dominant**, **1 = récessif**.
- Arbitration non destructive → ID le plus petit gagne.

## 📨 Types de trames
- **Data Frame**  
- **Remote Frame**  
- **Error Frame**  
- **Overload Frame**

## 🧩 Structure d’une trame
