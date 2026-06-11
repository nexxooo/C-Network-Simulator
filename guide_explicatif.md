# Guide Explicatif : Simulation de Réseau Local (LAN) en C

Ce guide présente l'architecture, le fonctionnement et le rôle détaillé de chaque fonction et structure de données de votre projet de simulation réseau local (LAN) avec gestion du **Spanning Tree Protocol (STP)** et de la **commutation de trames Ethernet**.

---

## 1. Structures de Données principales
Définies dans [include/equipement.h](file:///home/nexxo/SAE-S21/include/equipement.h).

### A. Identification & Adressage
* **`MAC` :** Représente une adresse physique Ethernet sur 6 octets (`uint8_t bytes[6]`).
* **`IPV4` :** Représente une adresse logique IP sur 4 octets (`uint8_t bytes[4]`).

### B. Équipements du Réseau
* **`station` :** Représente une machine terminale (ordinateur, serveur) avec une adresse MAC et une adresse IP.
* **`port` :** Modélise une interface physique d'un switch. Il stocke :
  * Son numéro de port.
  * Son état STP (`ETAT_PORT_BLOQUE`, `ETAT_PORT_INCONNU`, `ETAT_PORT_DESIGNE`, `ETAT_PORT_RACINE`).
  * Le meilleur message BPDU reçu sur cette interface.
* **`table_de_commutation` :** Une entrée associant une adresse `MAC` à un index de port physique (`interface_port`).
* **`switch_` :** Modélise un commutateur avec :
  * Sa propre adresse MAC.
  * Un tableau dynamique de structures `port`.
  * Sa priorité STP (utilisée pour élire le switch racine).
  * Sa table de commutation dynamique (`tab`).
  * Ses variables STP (`racine`, `cout_vers_racine`, `port_racine`).
* **`equipement` :** Structure polymorphe (union) contenant soit une `station`, soit un `switch_`, identifiée par l'énumération `type_equ` (`STATION` ou `SWITCH`).

### C. Topologie Réseau (Graphe)
* **`cable` :** Représente un lien physique (arête pondérée) connectant deux équipements identifiés par leur index dans le réseau (`sommet1` et `sommet2`), avec un coût (débit).
* **`reseau_local` :** Le graphe global. Il contient un tableau dynamique d'équipements (`equipements`) et un tableau dynamique de liens physiques (`cables`).

---

## 2. Chargement et Gestion de la Topologie
Implémenté dans [src/equipement.c](file:///home/nexxo/SAE-S21/src/equipement.c).

| Fonction | Rôle / Description | Fonctionnement Interne |
| :--- | :--- | :--- |
| `init_reseau` | Initialise la structure du graphe réseau local. | Alloue la mémoire initiale pour les équipements et les câbles en utilisant la capacité par défaut. |
| `free_reseau` | Libère proprement toute la mémoire du réseau. | Parcourt les équipements pour libérer les allocations internes des switches (`ports` et `tab`) afin d'éviter les fuites de mémoire, puis libère les tableaux globaux. |
| `init_switch` | Initialise un commutateur individuel. | Alloue dynamiquement le tableau de ports physiques et la table de commutation. Initialise tous les ports à l'état `ETAT_PORT_INCONNU`. |
| `ajouter_equipement` | Ajoute un nouvel équipement au réseau. | Gère la réallocation dynamique de la mémoire (via `realloc` avec `TAILLE_REALOC`) si le tableau d'équipements est plein. |
| `ajouter_cable` | Connecte deux équipements via un câble. | Gère la réallocation dynamique du tableau de câbles. |
| `charger_reseau` | Charge la topologie depuis un fichier. | Ouvre le fichier de config, parse la première ligne pour obtenir les dimensions, puis lit chaque ligne pour initialiser et connecter les stations et switches. Convertit les adresses IP/MAC textuelles en octets. |

---

## 3. Algorithmes de Graphe et Topologie
Implémenté dans [src/equipement.c](file:///home/nexxo/SAE-S21/src/equipement.c).

* **`est_un_arbre` :** Détermine si le réseau actuel ne contient aucun cycle et est entièrement connecté. (Vérifie si le réseau est connexe et possède exactement $N-1$ liaisons).
* **`sommets_adjacent` :** Remplit un tableau avec les indices des voisins connectés à un équipement donné. Utile pour les parcours de graphe.
* **`visite_composante_connexe` :** Fonction récursive effectuant un parcours en profondeur (DFS) pour marquer tous les sommets atteignables à partir d'un nœud de départ.
* **`reseau_est_connexe` :** Vérifie si le réseau entier est d'un seul bloc (tous les sommets doivent être marqués visités après un seul parcours DFS).
* **`obtenir_port_local` :** Retourne le numéro de port local sur un switch associé à un câble donné en comptant les câbles raccordés à ce switch.

---

## 4. Spanning Tree Protocol (STP 802.1d)
Implémenté dans [src/stp.c](file:///home/nexxo/SAE-S21/src/stp.c).

> [!NOTE]
> Le protocole STP évite les boucles réseau infinies en coupant logiquement certains câbles (changement d'état des ports en `ETAT_PORT_BLOQUE`).

```mermaid
graph TD
    A[Initialisation : Chaque switch se croit Racine] --> B[Diffusion des BPDUs sur tous les ports actifs]
    B --> C{Réception BPDU plus avantageux ?}
    C -- Oui -- > D[Mise à jour : Nouvelle Racine + Coût recalculé]
    D --> B
    C -- Non --> E[Convergence : Élection stable]
    E --> F[Résolution de l'état final des ports : RACINE, DÉSIGNÉ, BLOQUÉ]
```

### Détail des fonctions STP :
* **`stp_initialiser_ponts` :** Configure chaque commutateur pour qu'il se considère initialement comme la racine du réseau (`cout = 0` et `racine = mac_du_switch`), et bloque tous les ports.
* **`creer_bpdu_802_1d` & `encapsuler_bpdu_dans_trame` :** Créent un paquet BPDU contenant le Root ID (la racine connue), le Coût (distance cumulée) et le Transmitting ID (MAC de l'émetteur), puis l'encapsulent dans une trame Ethernet multicast destinée à l'adresse réservée `01:80:C2:00:00:00`.
* **`bpdu_est_meilleur` :** Implémente la règle de priorité 802.1d pour comparer deux messages BPDU :
  1. Plus petit ID de racine connu.
  2. En cas d'égalité, plus petit coût vers la racine.
  3. En cas d'égalité, plus petit ID de switch transmetteur (MAC).
* **`stp_traiter_trame_recue` :** Analyse un BPDU reçu sur un port :
  * Calcule le coût final (coût du BPDU + coût physique du câble).
  * **Apprentissage MAC :** Enregistre l'adresse MAC du switch voisin dans sa table de commutation.
  * Sauvegarde le meilleur BPDU reçu sur ce port.
  * Si le BPDU reçu est globalement meilleur que celui du switch actuel, met à jour sa racine, son coût et marque ce port comme candidat "port racine".
* **`stp_diffuser_trames` :** Transmet le BPDU actuel d'un switch à tous ses voisins direct de type switch. Si l'état d'un voisin change lors du traitement, ce dernier propage récursivement le changement.
* **`stp_resoudre_etats_ports` :** Une fois la racine élue, cette fonction détermine l'état final de chaque port :
  * Le commutateur racine passe tous ses ports en `DÉSIGNÉ` (ouverts).
  * Les autres switches élisent **un seul** port `RACINE` (celui qui a reçu le meilleur BPDU).
  * Pour les ports non-racines restants : ils passent en `DÉSIGNÉ` si le switch actuel offre un meilleur chemin vers la racine que le switch en face. Sinon, ils sont configurés en `BLOQUÉ`.
* **`stp_init` :** Orchestre l'ensemble des étapes du protocole STP jusqu'à la convergence complète.

---

## 5. Commutation et Apprentissage Ethernet (L'Étape 3)
Implémenté dans [src/equipement.c](file:///home/nexxo/SAE-S21/src/equipement.c) et utilisé dans [src/main.c](file:///home/nexxo/SAE-S21/src/main.c).

Cette partie simule le transfert de données utilisateur (trames de stations) à travers les liens ouverts par le STP.

### A. Les fonctions de commutation
* **`creer_trame_ethernet` :** Fabrique une trame de données utilisateur brute avec adresses MAC source et destination, le code du protocole supérieur (IPv4 `0x0800` par exemple) et les données textuelles.
* **`switch_apprendre_mac` :** Enregistre une association `(Adresse MAC -> Port d'entrée)` dans la table de commutation du switch. Si la table est pleine, elle est réallouée dynamiquement sur le tas.
* **`switch_trouver_port` :** Interroge la table de commutation pour savoir si le switch connaît le port physique correspondant à une adresse MAC destination.
* **`obtenir_voisin_par_port` :** Retrouve le câble et l'index de l'équipement qui est branché sur un numéro de port spécifique d'un switch.
* **`envoyer_trame` & `propager_trame_recue` :** Simulent le voyage physique d'une trame :
  * La station source émet la trame sur son unique câble.
  * Le switch récepteur vérifie que le port d'entrée n'est pas bloqué par le STP.
  * **Apprentissage :** Le switch apprend sur quel port se trouve l'adresse MAC source de la trame.
  * **Aiguillage :**
    * Si la MAC destination est connue dans la table de commutation, le switch l'envoie **uniquement** sur ce port (commutation monocast/unicast), à condition que le port ne soit pas bloqué.
    * Si la destination est inconnue ou s'il s'agit d'un broadcast (`FF:FF:FF:FF:FF:FF`), le switch l'envoie sur tous ses ports actifs (non bloqués par STP), à l'exclusion du port d'arrivée (inondation/flooding).
  * La propagation s'effectue récursivement jusqu'à ce que la trame atteigne la station cible.

---

## 6. Déroulement d'un Scénario Typique (Simulation)
Lorsque vous compilez et exécutez le projet via [src/main.c](file:///home/nexxo/SAE-S21/src/main.c) :

1. **Chargement :** Le réseau est chargé (ex. `config2.txt`). Les tables de commutation sont vides, les ports sont inconnus.
2. **Exécution du STP :** Les switches s'échangent des BPDUs. 
   * Ils élisent la racine (le switch avec le plus petit ID MAC ou la priorité la plus faible).
   * Ils bloquent les câbles redondants pour couper les cycles.
   * **Apprentissage STP :** Les switches apprennent les adresses MAC de leurs voisins directs switches (leurs tables commencent à se remplir).
3. **Première Transmission (Station 1 → Station 2) :**
   * La Station 1 envoie une trame.
   * Le switch connecté apprend la MAC de la Station 1.
   * N'ayant pas encore appris la MAC de la Station 2, le switch **inonde (floode)** la trame.
   * La trame se propage sur les chemins non bloqués de l'arbre STP.
   * Tous les switches traversés apprennent au passage que la Station 1 est accessible via le port où ils ont reçu la trame.
   * La Station 2 finit par recevoir la trame et l'accepte.
4. **Deuxième Transmission (Station 2 → Station 1) :**
   * La Station 2 répond.
   * Le switch local apprend la MAC de la Station 2.
   * Lors de la recherche de la destination (Station 1), le switch trouve son adresse dans sa table (car elle a été apprise à l'étape précédente).
   * La trame est envoyée en **monocast** directement sur le port approprié. Aucun autre port n'est pollué.
5. **Affichage final :** Le programme affiche les tables de commutation de chaque switch, montrant les adresses apprises lors du STP (switches voisins) et lors des échanges de données (stations).
