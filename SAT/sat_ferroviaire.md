# Fiche de révision – Sûreté de Fonctionnement (SdF)
## Cours Guillaume VIBERT – ALSTOM

# 1. Définition de la SdF

## Sûreté de fonctionnement = RAMS / FMDS

### RAMS
- **Reliability** → Fiabilité
- **Availability** → Disponibilité
- **Maintainability** → Maintenabilité
- **Safety** → Sécurité / Sûreté

### FMDS
- Fiabilité
- Maintenabilité
- Disponibilité
- Sûreté de fonctionnement

---

# 2. Les notions essentielles

## Fiabilité (Reliability)

Capacité d’un système à fonctionner sans panne pendant une durée donnée.

### Exemple
- porte de train : 100 000 cycles
- calculateur : 25 000h à 200 000h

---

## Disponibilité (Availability)

Probabilité qu’un système soit opérationnel à un instant donné.

### Formule importante
\[
A = \frac{MUT}{MUT + MDT}
\]

Avec :
- MUT = temps moyen de fonctionnement
- MDT = temps moyen d’arrêt

👉 Plus le système :
- tombe rarement en panne
- se répare vite

➡️ plus il est disponible.

---

## Maintenabilité (Maintainability)

Facilité de réparation d’un système.

### Exemple d’exigence
- réparation en moins de 15 min
- peu d’outils
- pas de compétence IT

---

# 3. Métriques à connaître

| Acronyme | Signification |
|---|---|
| MTTF | Mean Time To Failure |
| MTBF | Mean Time Between Failures |
| MTTR | Mean Time To Restore |
| MUT | Mean Up Time |
| MDT | Mean Down Time |

---

# 4. Courbe en baignoire (Bathtub Curve)

## Les 3 phases

### 1. Début de vie
➡️ Beaucoup de pannes
(défauts de fabrication)

### 2. Vie utile
➡️ Taux de panne constant

### 3. Fin de vie
➡️ Vieillissement / usure

---

## Relation importante
\[
MTTF = \frac{1}{\lambda}
\]

avec :
- \(\lambda\) = taux de panne

---

# 5. Sécurité fonctionnelle (Functional Safety)

## Définition IEC61508

Objectif :
> éviter les risques inacceptables.

---

## Le risque dépend de :
- la probabilité d’occurrence
- la gravité du dommage

⚠️ Le risque zéro n’existe pas.

---

# 6. Réduction du risque

## Réduire la gravité
Exemples :
- airbag
- protections

## Réduire la probabilité
Exemples :
- signalisation
- prévention
- architecture sécuritaire

---

# 7. THR et SIL

## THR
### Tolerable Hazard Rate

Fréquence maximale acceptable d’un événement dangereux.

Unité :
\[
h^{-1}
\]

---

## SIL (Safety Integrity Level)

| SIL | THR |
|---|---|
| SIL4 | \(10^{-9} \le THR < 10^{-8}\) |
| SIL3 | \(10^{-8} \le THR < 10^{-7}\) |
| SIL2 | \(10^{-7} \le THR < 10^{-6}\) |
| SIL1 | \(10^{-6} \le THR < 10^{-5}\) |

👉 Plus le SIL est élevé :
- plus le système doit être sûr.

---

# 8. Types de fautes

# A. Random Faults

## Caractéristiques
- aléatoires
- matérielles
- quantifiables

### Exemple
- composant HS

## Traitement
➡️ par l’architecture.

---

# B. Systematic Faults

## Caractéristiques
- dues à une erreur humaine
- reproductibles
- non quantifiables

### Exemple
- bug logiciel

## Traitement
➡️ par le processus :
- validation
- vérification
- qualité

---

# 9. Architectures de sécurité

# A. Inherent Fail-Safe

## Principe
Le système est sûr par construction.

Même en panne :
➡️ pas de situation dangereuse.

---

## Exemple du cours
### Chasse d’eau
Le trou empêche le débordement.

---

## Avantages
- simple
- fiable
- peu coûteux

## Inconvénient
- limité aux fonctions simples

---

# B. Reactive Fail-Safe

## Principe
Deux éléments :
- **worker** → fait le travail
- **checker** → surveille

Si erreur :
➡️ le checker impose un état sûr.

---

## Condition importante
\[
T_{detection} + T_{negation}
\]
doit être très faible.

---

## Avantages
- adapté aux systèmes complexes

## Inconvénients
- réaction très rapide nécessaire
- architecture spécifique

---

## Exemple du cours
Affichage conducteur :
- CPU principal = worker
- microcontrôleur = checker
- si erreur → extinction écran

---

# C. Composite Fail-Safe

## Principe
Plusieurs canaux exécutent la même fonction.

Le système agit seulement si :
➡️ les canaux sont d’accord.

---

# 10. Architectures importantes

# 2oo2

## 2-out-of-2

Les 2 systèmes doivent être d’accord.

### Avantage
- très sûr

### Inconvénient
- disponibilité faible

---

# 2oo3 (TMR)

## Triple Modular Redundancy

3 systèmes :
- vote majoritaire

### Avantages
- sécurité + disponibilité

### Inconvénients
- coûteux
- complexe

---

# 11. Redondance – Diversité – Indépendance

## Redondance
Dupliquer les composants.

---

## Diversité
Utiliser :
- technologies différentes
- logiciels différents

➡️ évite les fautes communes.

---

## Indépendance
Séparer :
- alimentation
- support physique
- communication

---

# 12. Communication sécuritaire (EN50159)

# Menaces principales

| Menace | Description |
|---|---|
| Repetition | répétition message |
| Deletion | suppression |
| Corruption | modification |
| Insertion | ajout |
| Delay | retard |
| Masquerade | faux émetteur |
| Re-sequence | ordre modifié |

---

# Défenses

- CRC
- timeout
- numéro de séquence
- authentification
- timestamp

---

# 13. Consensus Problem

## Two Generals Problem

Impossible de garantir un accord parfait via un réseau non fiable.

Conséquence :
- problème de synchronisation
- problème de vote
- split-brain possible

---

# 14. Analyse de sécurité

## Étapes

### Analyse fonctionnelle
Que doit faire le système ?

### Analyse dysfonctionnelle
Que peut-il mal faire ?

### FMEA
Analyse des modes de défaillance.

### Calcul du risque résiduel
Wrong-side failure rate.

---

# 15. Ce qu’il faut ABSOLUMENT savoir pour l’exam

## Définitions
- RAMS / FMDS
- THR
- SIL
- Random vs Systematic fault

---

## Architectures
Savoir expliquer :
- Inherent fail-safe
- Reactive fail-safe
- Composite fail-safe

👉 avec :
- principe
- avantages
- inconvénients
- exemples

---

## À savoir refaire / expliquer
- formule disponibilité
- courbe baignoire
- 2oo2
- 2oo3
- rôle du checker
- vote majoritaire
- menaces EN50159

---

# 16. Phrase importante du prof

> “Anything that can go wrong will go wrong.”

➡️ Le rôle de l’ingénieur :
- réduire les risques,
- gérer les compromis :
  - sécurité
  - disponibilité
  - coût
  - complexité.