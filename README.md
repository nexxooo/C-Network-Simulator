# Simulation de Réseau Local (LAN)

Ce projet en langage C propose une modélisation et une simulation d'un réseau local (LAN) composé de stations de travail et de commutateurs (switches) reliés par des câbles réseau. Il permet de charger dynamiquement des topologies réseau complexes depuis des fichiers de configuration, d'initialiser les tables de commutation et de manipuler des trames Ethernet.

---

## 📝 Description

Le projet simule le fonctionnement physique et logique d'un réseau local. Il modélise les concepts fondamentaux des réseaux informatiques :
*   **Adressage physique (MAC)** et **adressage logique (IPv4)**.
*   **Équipements réseaux** :
    *   **Stations** (ordinateurs terminaux dotés d'une adresse MAC et IP).
    *   **Switches** (commutateurs chargés d'aiguiller les trames via des tables de commutation).
*   **Liaisons physiques** : Connexions physiques entre équipements représentées par des câbles réseau pondérés (permettant d'évaluer le coût d'un lien).
*   **Trames Ethernet** : Structure de données représentant une trame standard (préambule, SFD, adresses source/destination, type, données utiles de 1500 octets maximum et FCS).

---

## 🚀 Fonctionnalités

### 1. Modélisation et Structures de Données
*   Gestion dynamique de la mémoire pour l'allocation et la réallocation des équipements et des liaisons physiques.
*   Représentation précise des trames Ethernet (brutes et formatées).
*   Tables de commutation associées à chaque switch pour mapper les adresses MAC aux ports physiques.

### 2. Chargement de Configurations Réseau
*   Lecture et parsing automatique de fichiers texte structurés décrivant la topologie du réseau (nombre d'équipements, câbles, caractéristiques de chaque élément).
*   Conversion à la volée des adresses IP (ex. `192.168.1.1`) et adresses MAC (ex. `00:15:5d:db:40:61`) de chaînes de caractères en octets exploitables en mémoire.

### 3. Affichage et Diagnostics
*   **Affichage de la topologie** : Résumé statistique (nombre d'équipements, nombre de câbles) et détails des interconnexions physiques.
*   **Diagnostic de trames** :
    *   `afficher_tram_user` : Présentation claire et structurée des informations clés (MAC source, MAC destination).
    *   `afficher_tram_brute` : Analyse bas niveau affichant la trame complète en hexadécimal (incluant le préambule, le SFD et le bourrage de données).
*   **Visualisation des tables de commutation** : Représentation graphique sous forme de ports des associations MAC/Interface.

### ⚠️ Protocole Spanning Tree (STP) — *À faire / En cours*
Bien que les structures de données prennent déjà en compte les attributs nécessaires au protocole **STP** (comme la priorité des commutateurs pour l'élection du *Root Bridge* et la pondération des câbles pour le calcul du coût des chemins), **l'implémentation de l'algorithme d'évitement des boucles (Spanning Tree Protocol) reste à réaliser.**

---

## 🛠️ Installation et Compilation

### Prérequis
*   Un compilateur C supportant la norme C23 (comme **GCC** récent).
*   L'outil **Make** pour automatiser la compilation.
*   Un environnement de type Unix/Linux (ou WSL sous Windows).

### Compilation
Un fichier [Makefile](file:///home/nexxo/SAE-S21/Makefile) est fourni à la racine du projet. Utilisez les commandes suivantes :

*   **Compiler le projet** :
    ```bash
    make
    ```
    *Cette commande génère les fichiers objets dans le dossier `obj/` et l'exécutable `mon_programme` dans le dossier `bin/`.*

*   **Exécuter le programme** :
    ```bash
    make run
    ```

*   **Nettoyer les fichiers objets temporaires** :
    ```bash
    make clean
    ```

*   **Nettoyer tous les fichiers compilés (objets et exécutable)** :
    ```bash
    make fclean
    ```

*   **Recompiler entièrement le projet** :
    ```bash
    make re
    ```

---

## 📂 Structure du Projet

```text
SAE-S21/
├── bin/                 # Exécutables compilés (ex. mon_programme)
├── configs/             # Fichiers de configuration réseau (*.txt)
├── include/             # Fichiers d'en-tête (.h)
│   ├── affichage.h      # Prototypes pour l'affichage et les conversions de chaînes
│   └── equipement.h     # Définitions des structures (MAC, IP, station, switch, cable, trame)
├── obj/                 # Fichiers objets (.o) temporaires
├── src/                 # Code source (.c)
│   ├── main.c           # Point d'entrée de l'application
│   ├── affichage.c      # Implémentation des fonctions de diagnostic et d'affichage
│   └── equipement.c     # Initialisation, allocation, libération et parsing réseau
├── test/                # Tests unitaires / d'intégration
├── Makefile             # Fichier de compilation automatisée
└── README.md            # Ce fichier
```

---

## 📝 Format du Fichier de Configuration

Les fichiers de configuration (situés dans le dossier `configs/`) décrivent la topologie du réseau local selon le format suivant :

```text
<nb_equipements> <nb_cables>
<type_equipement>;<adresse_mac>;<parametres_specifiques...>
...
<sommet_1>;<sommet_2>;<poids_cable>
...
```

### Détail des lignes d'équipements :
*   **Station (Type 1)** : `1;<adresse_mac>;<adresse_ip>`
    *   *Exemple* : `1;00:15:5d:db:40:61;176.173.199.138`
*   **Switch (Type 2)** : `2;<adresse_mac>;<nb_ports>;<priorite_stp>`
    *   *Exemple* : `2;01:45:23:a6:f7:01;8;1024`

### Détail des liaisons (Câbles) :
*   `sommet_1` et `sommet_2` représentent les indices de l'équipement dans le réseau (de `0` à `nb_equipements - 1`).
*   *Exemple* : `0;1;4` relie l'équipement `0` à l'équipement `1` avec un coût/poids physique de `4`.
