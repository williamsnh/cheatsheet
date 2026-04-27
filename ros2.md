# Fiche Révision ROS2

> **Liens utiles**
> - Tuto officiel ROS2 Jazzy : https://docs.ros.org/en/jazzy/Tutorials.html
> - Git Schéma ROS2 (drone Tello) : https://gitlab.com/AnselmeVDCPE/as_tello_drone_ros2/-/wikis/Scénario-final

---

## Table des matières

1. [Contexte — Qu'est-ce qu'un robot ?](#1-contexte--quest-ce-quun-robot-)
2. [Qu'est-ce que ROS2 ?](#2-quest-ce-que-ros2-)
3. [ROS2 vs ROS1](#3-ros2-vs-ros1)
4. [Concepts fondamentaux de ROS2](#4-concepts-fondamentaux-de-ros2)
5. [Les 3 modes de communication](#5-les-3-modes-de-communication)
6. [Interfaces (types de messages)](#6-interfaces-types-de-messages)
7. [DDS & QoS — Communication avancée](#7-dds--qos--communication-avancée)
8. [Lifecycle nodes](#8-lifecycle-nodes)
9. [CLI & Toolbox](#9-cli--toolbox)
10. [Middleware Naoqi (Aldebaran)](#10-middleware-naoqi-aldebaran)
11. [Workspace & Packages — Pratique](#11-workspace--packages--pratique)
12. [Écrire un Publisher / Subscriber](#12-écrire-un-publisher--subscriber)
13. [Écrire un Service / Client](#13-écrire-un-service--client)
14. [Écrire une Action / Client](#14-écrire-une-action--client)
15. [Fichiers à modifier quand tu ajoutes du code](#15-fichiers-à-modifier-quand-tu-ajoutes-du-code)
16. [Partie TP — Turtlesim](#16-partie-tp--turtlesim)
17. [Pièges classiques](#17-pièges-classiques)
18. [Méga résumé commandes](#18-méga-résumé-commandes)

---

## 1. Contexte — Qu'est-ce qu'un robot ?

### Robot industriel (ISO 10218)

> « Manipulateur à commande automatique, reprogrammable, multi-applications, pouvant être programmé suivant trois axes ou plus, qui peut être fixe ou mobile, destiné à être utilisé dans les applications d'automatisation industrielle. »

**"Manipulateur"** désigne un bras mécanique articulé capable de déplacer et orienter des objets dans l'espace — comme une pince ou un outil de soudure. Le terme vient du latin *manipulus* (main) : c'est un système qui manipule des objets physiques à la place de la main humaine.

Caractéristiques :
- Environnement connu / maîtrisé
- Grande vitesse d'exécution
- Intelligence proche du hardware (bas niveau)
- Peu de coopération inter-robot
- Peu (pas ?) d'interaction avec les humains

### Robot de service (IFR)

> « Un robot qui opère de façon automatique ou semi-automatique pour réaliser des services utiles pour le bien-être des humains et des équipements, excluant les opérations manufacturières. »

Caractéristiques :
- Environnement inconnu / changeant
- Facteurs inconnus nombreux
- Haut niveau d'abstraction
- Nombreuses interactions (humain, robot, environnement)
- Nombreuses coopérations / connexions

---

## 2. Qu'est-ce que ROS2 ?

**ROS = Robot Operating System** — c'est un **middleware** robotique, **pas un vrai OS**. Il fait le lien entre le matériel et les applications logiques.

### Pourquoi ROS2 n'est pas un OS

Un vrai OS (Linux, Windows…) gère directement le matériel (CPU, mémoire, périphériques), ordonnance les processus, et fournit une interface bas niveau entre le hardware et les applications. ROS2 ne fait aucune de ces choses : il tourne *par-dessus* un vrai OS (Ubuntu) et en dépend entièrement.

```
Tes nœuds (App)
      ↓
    ROS2        ← middleware
      ↓
   Ubuntu        ← vrai OS
      ↓
  Hardware
```

### Caractéristiques principales

| Propriété | Valeur |
|-----------|--------|
| Créé en | 2007 (ROS1), 2015 (ROS2) |
| Release | 2010 (ROS1), 2017 (ROS2) |
| Fin de ROS1 | 2025 (Noetic) |
| Licence | BSD (open-source permissive) |
| OS supportés | Linux / Windows / MacOS |
| Transport | DDS (standard industriel) |

### La formule officielle

```
ROS = plumbing + tools + capabilities + community
```

- **Plumbing** : infrastructure de communication
- **Tools** : outils de debug/visualisation
- **Capabilities** : bibliothèques (navigation, vision, bras…)
- **Community** : écosystème de packages partagés

### Pourquoi utiliser ROS2 ?

- Time-to-market réduit
- Production-ready
- Multi-plateforme, multi-domaine
- Pas de blocage fournisseur
- Basé sur des standards open source
- Communauté active
- Supporté par l'industrie
- Interopérabilité avec ROS1 (via `ros1_bridge`)

---

## 3. ROS2 vs ROS1

| Aspect | ROS1 | ROS2 |
|--------|------|------|
| Transport | TCP/UDP ROS custom | **DDS** (standard industrie) |
| OS | Linux seulement | Linux / Windows / MacOS |
| API | rospy / roscpp + **Master** | rclpy / rclcpp / rcl (pas de Master) |
| Sécurité | Aucune | Support DDS Security |
| QoS | Non | Oui (configurable) |
| Lifecycle nodes | Non | Oui (4 états primaires) |
| Interop | — | ros1_bridge disponible |

**Objectifs de ROS2 :**
- Garder ce qui fonctionnait bien sous ROS1
- Améliorer la fiabilité
- Adopter les contraintes du milieu industriel
- Remaniement complet du transfert des messages (DDS)
- Standardisation et documentation améliorée

---

## 4. Concepts fondamentaux de ROS2

### Nœuds (Nodes)

Un **node** est un processus ROS2 qui exécute une tâche spécifique (= un exécutable).

- 1 application logique = 1 nœud
- Écrits en Python (`rclpy`) ou C++ (`rclcpp`)
- S'exécutent indépendamment
- Communiquent via topics, services, actions
- Regroupés dans des **packages**
- Convention : nœuds en **classes** héritant de `Node`

```python
class MonNoeud(Node):
    def __init__(self):
        super().__init__('nom_du_noeud')
        # ...
```

**Commandes utiles :**
```bash
ros2 node list                    # Lister les nodes actifs
ros2 node info /nom_du_node       # Infos sur un node
```

### Packages

Un **package** est concrètement un répertoire contenant un lot de nœuds ou d'outils qui répondent à un besoin / une fonctionnalité.

- Intégrable / plug & play
- Peut contenir uniquement des interfaces (msg, srv, action)
- Compilé avec `colcon build`

**Structure Python :**
```
mon_package/
  mon_package/     ← contient les nœuds
  package.xml
  setup.py         ← remplace CMakeLists.txt
  setup.cfg
```

**Structure C++ :**
```
mon_package/
  include/
  src/
  CMakeLists.txt
  package.xml
```

### Workspace

Zone de travail pour un projet ROS2.

```
ros_ws/
  src/        ← ton code source
  build/      ← fichiers de compilation
  install/    ← résultat installé
  log/        ← journaux
```

Possibilité de superposer des workspaces : **overlays** (ROS2 installation → overlay 1 → overlay 2 → …)

---

## 5. Les 3 modes de communication

### Définitions en une phrase

- **Topic** : Canal de communication unidirectionnel et asynchrone sur lequel un ou plusieurs nœuds publient des données en continu, que d'autres nœuds peuvent écouter à tout moment.
- **Service** : Mécanisme de communication bidirectionnel où un nœud client envoie une requête ponctuelle à un serveur qui lui retourne une réponse unique.
- **Action** : Mécanisme de communication bidirectionnel et asynchrone permettant à un client d'envoyer un objectif à long terme à un serveur, qui lui renvoie des retours réguliers pendant l'exécution avant de lui communiquer le résultat final.

### Tableau comparatif

| Type | Modèle | Direction | Synchronisme | Quand l'utiliser |
|------|--------|-----------|--------------|-----------------|
| **Topic** | Publisher / Subscriber | Unidirectionnel | Asynchrone | Données continues (capteurs, position…) |
| **Service** | Client / Server (1 serveur max) | Bidirectionnel | Synchrone (voir async) | Requête ponctuelle avec réponse rapide |
| **Action** | Client / Server + feedback topic | Bidirectionnel | Asynchrone | Tâche longue avec feedback continu |

> **À retenir :**
> - Un topic accepte **many-to-many**
> - Un service n'a qu'**un seul serveur**
> - Une action combine **Goal + Feedback + Result**

### Commandes CLI — Topics

```bash
ros2 topic list                                                    # Lister les topics
ros2 topic info /topic_name                                        # Infos sur un topic
ros2 topic echo /topic_name                                        # Lire un topic
ros2 topic pub /topic_name std_msgs/msg/String "data: Hello"       # Publier manuellement
```

### Commandes CLI — Services

```bash
ros2 service list                                                  # Lister les services
ros2 service type /service_name                                    # Type d'un service
ros2 service call /service_name type "{param: value}"              # Appeler un service
```

### Commandes CLI — Actions

```bash
ros2 action list                                                   # Lister les actions
ros2 action info /action_name                                      # Infos sur une action
ros2 action send_goal /action_name type "{...}"                    # Envoyer un goal
```

### Commandes CLI — Paramètres

```bash
ros2 param list                                                    # Lister les paramètres
ros2 param get /node_name param_name                               # Lire un paramètre
ros2 param set /node_name param_name value                         # Modifier un paramètre
```

---

## 6. Interfaces (types de messages)

| Extension | Nom | Rôle | Séparateur |
|-----------|-----|------|-----------|
| `.msg` | Messages | Type d'un topic | — |
| `.srv` | Services | Entrées (Request) + sorties (Response) | `---` |
| `.action` | Actions | Goal + Result + Feedback | `---` (×2) |

Il existe des interfaces par défaut mais on peut créer les siennes.

**Exemples :**

```
# example.action
int32 order          # Request (goal)
---
int32[] sequence     # Result
---
int32[] partial_seq  # Feedback
```

```
# geometry_msgs/msg/Pose.msg
geometry_msgs/msg/Point position       # → double x, y, z
geometry_msgs/msg/Quaternion orientation
```

**Voir une interface :**
```bash
ros2 interface show std_msgs/msg/String
ros2 interface show geometry_msgs/msg/Twist
```

---

## 7. DDS & QoS — Communication avancée

### DDS (Data Distribution Service)

Standard industriel remplaçant le Master de ROS1. Communication **data-centrique** : les nœuds s'abonnent à des données, pas à des nœuds. Plus besoin de point central — découverte automatique des nœuds.

**Analogie :** La communication non data-centrique c'est comme des emails (tu dois tous les parcourir). La communication data-centrique c'est comme un calendrier (tu accèdes directement à l'info actuelle).

- Isolement par `ROS_DOMAIN_ID`
- Les nœuds préviennent de leur lancement et de leur arrêt

### QoS (Quality of Service)

Contrôle comment les nœuds reçoivent les données :
- Pas de perte de messages
- Queue de messages (taille configurable)
- Drop des vieux messages
- Pas de message reçu avant le subscribe
- Pas de service perdu

Customisable en ROS2 (par défaut : comportement identique à ROS1).

---

## 8. Lifecycle nodes

Les lifecycle nodes ont des états gérés pour une meilleure robustesse en production.

**4 états primaires :**

| État | Description |
|------|-------------|
| `Unconfigured` | État initial après création |
| `Inactive` | Configuré mais pas actif |
| `Active` | En cours d'exécution (callbacks, timers…) |
| `Finalized` | Terminé, prêt à être détruit |

**6 états de transition :**
`Configuring` · `Activating` · `Deactivating` · `CleaningUp` · `ShuttingDown` · `ErrorProcessing`

---

## 9. CLI & Toolbox

### Commandes CLI complètes

```bash
# Syntaxe générale
ros2 [commande] [paramètre]

# Commandes disponibles :
# action | bag | component | daemon | doctor | interface |
# launch | lifecycle | multicast | node | param | pkg |
# run | security | service | topic | wtf
```

> **Astuce :** Utiliser `TAB` pour l'autocomplétion !

### Outils visuels

| Outil | Rôle |
|-------|------|
| `rviz2` | Visualisation 3D (robot, capteurs, carte…) |
| `rqt_graph` | Graphe des nodes, topics, publishers, subscribers |
| `rqt_console` | Visualisation et filtrage des logs (INFO, WARN, ERROR) |
| `rqt_image_view` | Afficher un flux vidéo/caméra ROS2 |
| `gazebo` | Simulateur physique 3D |
| `stage` | Simulateur 2D léger |
| `TF2` | Gestion des repères/transformations |
| `PCL` | Traitement de nuages de points |
| `MoveIt` | Planification de mouvement |
| `OpenCV` | Vision par ordinateur |

```bash
rqt_graph        # Visualiser le graphe ROS2
rqt_console      # Logs en temps réel
rqt_image_view   # Flux caméra
rviz2            # Visualisation 3D
```

---

## 10. Middleware Naoqi (Aldebaran)

Utilisé par **NAO**, **Pepper**, **Plato** (United Robotics Group).

**Architecture :** Broker → Libraries → Modules → Methods

- Le **Broker** central (NAOqi) expose des modules via `autoload.ini`
- Les **Modules** (ALMemory, ALMotion, ALLeds…) fournissent des méthodes
- Les **Méthodes** sont appelables en Python ou C++ (cross-language)
- **ALMemory** : mémoire partagée centralisée (read/write de variables entre modules)

---

## 11. Workspace & Packages — Pratique

### Créer un workspace

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws
colcon build
source install/setup.bash
```

### Créer un package

```bash
# Python
ros2 pkg create --build-type ament_python mon_package

# C++
ros2 pkg create --build-type ament_cmake mon_package
```

### Compiler et sourcer

```bash
cd ~/ros2_ws
colcon build                              # Tout compiler
colcon build --packages-select mon_pkg   # Un seul package
source install/setup.bash                 # À faire à CHAQUE nouveau terminal
```

### Lancer un node

```bash
ros2 run nom_du_package nom_du_node
ros2 launch nom_du_package fichier.launch.py
```

---

## 12. Écrire un Publisher / Subscriber

### Structure de base commune à tout nœud

```python
import rclpy
from rclpy.node import Node

class MonNoeud(Node):
    def __init__(self):
        super().__init__('nom_du_noeud')
        # ... publishers, subscribers, services, actions ici

def main(args=None):
    rclpy.init(args=args)
    node = MonNoeud()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
```

> **Important :** enregistrer le nœud dans `setup.py` sous `entry_points` :
> `'mon_noeud = mon_package.mon_fichier:main'`

### Publisher Python

```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class MinimalPublisher(Node):
    def __init__(self):
        super().__init__('minimal_publisher')
        self.publisher_ = self.create_publisher(String, 'topic', 10)
        self.timer = self.create_timer(1.0, self.timer_callback)

    def timer_callback(self):
        msg = String()
        msg.data = 'Hello ROS2'
        self.publisher_.publish(msg)

def main(args=None):
    rclpy.init(args=args)
    node = MinimalPublisher()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
```

### Subscriber Python

```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class MinimalSubscriber(Node):
    def __init__(self):
        super().__init__('minimal_subscriber')
        self.subscription = self.create_subscription(
            String, 'topic', self.listener_callback, 10)

    def listener_callback(self, msg):
        self.get_logger().info(f"J'ai reçu: {msg.data}")

def main(args=None):
    rclpy.init(args=args)
    node = MinimalSubscriber()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
```

---

## 13. Écrire un Service / Client

### Différences clés Service vs Action

| Aspect | Service | Action |
|--------|---------|--------|
| Interface | `.srv` — Request + Response | `.action` — Goal + Result + Feedback |
| Callback serveur | 1 callback : `callback(request, response)` | 3 callbacks : `goal_callback`, `cancel_callback`, `execute_callback` |
| Annulation | Non | Oui |
| Feedback en cours | Non | Oui |

### Serveur de service Python

```python
from example_interfaces.srv import AddTwoInts
import rclpy
from rclpy.node import Node

class AddTwoIntsService(Node):
    def __init__(self):
        super().__init__('add_two_ints_server')
        self.srv = self.create_service(AddTwoInts, 'add_two_ints', self.add)

    def add(self, request, response):
        response.sum = request.a + request.b
        return response

def main(args=None):
    rclpy.init(args=args)
    node = AddTwoIntsService()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
```

### Client de service Python

```python
from example_interfaces.srv import AddTwoInts
import rclpy
from rclpy.node import Node

class AddTwoIntsClient(Node):
    def __init__(self):
        super().__init__('add_two_ints_client')
        self.client = self.create_client(AddTwoInts, 'add_two_ints')

        # Attendre que le service soit disponible
        while not self.client.wait_for_service(timeout_sec=1.0):
            self.get_logger().info('Service non disponible...')

        req = AddTwoInts.Request()
        req.a = 2
        req.b = 3
        self.future = self.client.call_async(req)

def main(args=None):
    rclpy.init(args=args)
    node = AddTwoIntsClient()
    rclpy.spin_until_future_complete(node, node.future)
    print(node.future.result().sum)
    node.destroy_node()
    rclpy.shutdown()
```

---

## 14. Écrire une Action / Client

### Serveur d'action Python

```python
from rclpy.action import ActionServer
from action_tutorials_interfaces.action import Fibonacci
import rclpy
from rclpy.node import Node

class FibonacciServer(Node):
    def __init__(self):
        super().__init__('fibonacci_server')
        self._server = ActionServer(
            self,
            Fibonacci,
            'fibonacci',
            self.execute_callback
        )

    async def execute_callback(self, goal_handle):
        fb = Fibonacci.Feedback()
        fb.partial_sequence = [0, 1]

        # Publier du feedback en cours d'exécution
        goal_handle.publish_feedback(fb)

        # ... traitement ...

        goal_handle.succeed()
        result = Fibonacci.Result()
        result.sequence = fb.partial_sequence
        return result

def main(args=None):
    rclpy.init(args=args)
    node = FibonacciServer()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
```

### Client d'action Python

```python
from rclpy.action import ActionClient
from action_tutorials_interfaces.action import Fibonacci
import rclpy
from rclpy.node import Node

class FibonacciClient(Node):
    def __init__(self):
        super().__init__('fibonacci_client')
        self._client = ActionClient(self, Fibonacci, 'fibonacci')

    def send_goal(self, order):
        goal = Fibonacci.Goal()
        goal.order = order
        self._client.send_goal_async(
            goal,
            feedback_callback=self.feedback_callback
        )

    def feedback_callback(self, feedback_msg):
        feedback = feedback_msg.feedback
        self.get_logger().info(f'Feedback: {feedback.partial_sequence}')

def main(args=None):
    rclpy.init(args=args)
    node = FibonacciClient()
    node.send_goal(10)
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
```

---

## 15. Fichiers à modifier quand tu ajoutes du code

### `package.xml` — Dépendances du package

À modifier quand tu utilises un message, `rclpy`/`rclcpp`, ou un launch file.

```xml
<exec_depend>rclpy</exec_depend>
<exec_depend>std_msgs</exec_depend>
<exec_depend>sensor_msgs</exec_depend>
```

### `setup.py` (Python) — Déclaration des nodes

À modifier quand tu ajoutes un node Python.

```python
entry_points={
    'console_scripts': [
        'talker   = mon_package.talker:main',
        'listener = mon_package.listener:main',
    ],
}
```

> ⚠️ Si ton node n'est pas ici → `ros2 run` ne le trouvera pas.

### `CMakeLists.txt` (C++) — Compilation des nodes

```cmake
add_executable(talker src/talker.cpp)
ament_target_dependencies(talker rclcpp std_msgs)
install(TARGETS talker DESTINATION lib/${PROJECT_NAME})
```

---

## 16. Partie TP — Turtlesim

### Lancer Turtlesim

```bash
# Terminal 1 — le simulateur (fenêtre avec la tortue)
ros2 run turtlesim turtlesim_node

# Terminal 2 — la télécommande clavier
ros2 run turtlesim turtle_teleop_key
```

### Comprendre les nodes de Turtlesim

```bash
ros2 node list
# → /turtlesim
# → /teleop_turtle (si lancé)

ros2 node info /turtlesim
```

### Les topics importants de Turtlesim

| Topic | Type | Rôle |
|-------|------|------|
| `/turtle1/cmd_vel` | `geometry_msgs/msg/Twist` | Contrôle de la tortue |
| `/turtle1/pose` | `turtlesim/msg/Pose` | Position et orientation |
| `/turtle1/color_sensor` | `turtlesim/msg/Color` | Couleur sous la tortue |

```bash
# Voir la position de la tortue en direct
ros2 topic echo /turtle1/pose

# Publier une commande de vitesse
ros2 topic pub /turtle1/cmd_vel geometry_msgs/msg/Twist \
  "{linear: {x: 2.0}, angular: {z: 1.0}}"
```

### Les services de Turtlesim

| Service | Type | Rôle |
|---------|------|------|
| `/spawn` | `turtlesim/srv/Spawn` | Créer une nouvelle tortue |
| `/kill` | `turtlesim/srv/Kill` | Supprimer une tortue |
| `/turtle1/set_pen` | `turtlesim/srv/SetPen` | Modifier le stylo |
| `/reset` | `std_srvs/srv/Empty` | Réinitialiser la fenêtre |

```bash
# Créer une nouvelle tortue
ros2 service call /spawn turtlesim/srv/Spawn \
  "{x: 5.0, y: 5.0, theta: 0.0, name: 'toto'}"

# Tuer une tortue
ros2 service call /kill turtlesim/srv/Kill \
  "{name: 'turtle1'}"

# Changer la couleur du stylo
ros2 service call /turtle1/set_pen turtlesim/srv/SetPen \
  "{r: 255, g: 0, b: 0, width: 5, off: 0}"
```

### L'action de Turtlesim

```bash
ros2 action list

# Faire tourner la tortue (action)
ros2 action send_goal /turtle1/rotate_absolute \
  turtlesim/action/RotateAbsolute "{theta: 3.14}"
```

---

## 17. Pièges classiques

| Piège | Symptôme | Solution |
|-------|----------|----------|
| Oublier `source install/setup.bash` | Package introuvable | Sourcer après chaque `colcon build` |
| Mauvais nom de topic | Rien ne s'affiche dans `echo` | Vérifier avec `ros2 topic list` |
| Types de messages différents | Connexion impossible | Vérifier avec `ros2 topic info` |
| Node absent de `setup.py` | `ros2 run` échoue | Ajouter dans `console_scripts` |
| Ne pas relancer `colcon build` | Modifications ignorées | Build après chaque modif |
| Lancer client avant serveur | Client bloque | Utiliser `wait_for_service()` |
| Oublier `goal_handle.succeed()` | Client attend indéfiniment | Toujours terminer avec `succeed()` |
| Script non exécutable | Permission denied | `chmod +x mon_node.py` |

---

## 18. Méga résumé commandes

### Nodes
```bash
ros2 node list                              # Lister les nodes actifs
ros2 node info /nom_du_node                 # Infos sur un node
ros2 run nom_du_package nom_du_node         # Lancer un node
ros2 launch nom_du_package fichier.launch.py  # Lancer un launch file
```

### Topics
```bash
ros2 topic list                             # Lister les topics
ros2 topic info /topic                      # Infos sur un topic
ros2 topic echo /topic                      # Lire un topic
ros2 topic pub /topic type "{...}"          # Publier manuellement
```

### Services
```bash
ros2 service list                           # Lister les services
ros2 service type /service                  # Type d'un service
ros2 service call /service type "{...}"     # Appeler un service
```

### Actions
```bash
ros2 action list                            # Lister les actions
ros2 action info /action                    # Infos sur une action
ros2 action send_goal /action type "{...}"  # Envoyer un goal
```

### Paramètres
```bash
ros2 param list                             # Lister les paramètres
ros2 param get /node param                  # Lire un paramètre
ros2 param set /node param valeur           # Modifier un paramètre
```

### Workspace & Build
```bash
mkdir -p ~/ros2_ws/src                      # Créer le workspace
cd ~/ros2_ws && colcon build                # Compiler
source install/setup.bash                   # Sourcer (obligatoire !)
ros2 pkg create --build-type ament_python mon_package  # Créer package Python
ros2 pkg create --build-type ament_cmake mon_package   # Créer package C++
```

### Visualisation
```bash
rqt_graph        # Graphe nodes/topics
rqt_console      # Logs en temps réel
rqt_image_view   # Flux caméra
rviz2            # Visualisation 3D
```

---

*Fiche générée pour le TP noté ROS2 — CPE Lyon, Frameworks Robotique 2025/2026*
*Profs : Anselme Vandoorne & Raphaël Leber*