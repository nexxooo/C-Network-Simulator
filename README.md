# Simulateur de Réseau Local (LAN)

Ce projet en langage C propose une modélisation et une simulation d'un réseau local (LAN) composé de stations de travail et de commutateurs (switches) reliés par des câbles réseau.

## But

Le projet a pour but de modéliser le fonctionnement physique et logique d'un réseau local et de simuler la convergence du protocole Spanning Tree (STP) afin d'éviter les boucles de routage.

Il permet de :
- Charger dynamiquement des topologies réseau complexes depuis des fichiers de configuration.
- Valider la structure de la topologie (connexité, présence de cycles et structure d'arbre).
- Simuler la distribution des messages BPDU (Bridge Protocol Data Units) entre commutateurs.
- Résoudre l'état de chaque port (Bloqué, Racine, Désigné) pour obtenir un arbre de recouvrement (Spanning Tree) fonctionnel et sans boucle.

### Format du Fichier de Configuration

Les fichiers de configuration (situés dans le dossier `configs/`) décrivent la topologie du réseau local selon le format suivant :

```text
<nb_equipements> <nb_cables>
<type_equipement>;<adresse_mac>;<parametres_specifiques...>
...
<sommet_1>;<sommet_2>;<poids_cable>
...
```

- **Station (Type 1)** : `1;<adresse_mac>;<adresse_ip>`
  - *Exemple* : `1;00:15:5d:db:40:61;176.173.199.138`
- **Switch (Type 2)** : `2;<adresse_mac>;<nb_ports>;<priorite_stp>`
  - *Exemple* : `2;01:45:23:a6:f7:01;8;1024`
- **Liaisons (Câbles)** : `sommet_1;sommet_2;poids` reliants les indices des équipements (de `0` à `nb_equipements - 1`).
  - *Exemple* : `0;1;4` relie l'équipement `0` à l'équipement `1` avec un coût physique de `4`.

## Fonctionnalités

Le simulateur implémente les fonctionnalités réseau fondamentales à l'aide de structures de données dynamiques. Voici la description des fonctions les plus importantes du projet :

### 1. Gestion de la Topologie Réseau
- [charger_reseau](file:///home/nexxo/C-Network-Simulator/src/equipement.c#L106) : Lit un fichier de configuration texte et parse son contenu pour initialiser dynamiquement en mémoire les équipements (stations, switches) et les liaisons (câbles).
- [init_reseau](file:///home/nexxo/C-Network-Simulator/src/equipement.c#L5) et [free_reseau](file:///home/nexxo/C-Network-Simulator/src/equipement.c#L22) : Gèrent respectivement l'allocation initiale de la mémoire et la libération complète des ressources du réseau local.
- [construire_arbre_selon_reseau](file:///home/nexxo/C-Network-Simulator/src/equipement.c#L290) : Crée une copie du réseau en excluant les câbles dont les ports ont été bloqués par le protocole STP, permettant de matérialiser physiquement l'arbre de recouvrement obtenu.

### 2. Algorithmes de Graphes et Analyse Structurelle
- [est_un_arbre](file:///home/nexxo/C-Network-Simulator/src/equipement.c#L213) : Vérifie si le réseau constitue un arbre mathématique valide, c'est-à-dire qu'il est connexe et possède exactement N-1 liaisons pour N équipements (ce qui implique l'absence de cycles).
- [reseau_est_connexe](file:///home/nexxo/C-Network-Simulator/src/equipement.c#L247) : Valide que tous les équipements du réseau peuvent communiquer entre eux en utilisant un parcours de graphe en profondeur récursif défini par [visite_composante_connexe](file:///home/nexxo/C-Network-Simulator/src/equipement.c#L235).
- [obtenir_port_local](file:///home/nexxo/C-Network-Simulator/src/equipement.c#L273) : Détermine à quel numéro de port physique d'un commutateur correspond une liaison (câble) donnée.

### 3. Protocole Spanning Tree (STP - IEEE 802.1D)
- [stp_init](file:///home/nexxo/C-Network-Simulator/src/stp.c#L284) : Fonction principale qui lance le protocole STP. Elle initialise les commutateurs, déclenche la diffusion des trames et résout les états finaux des ports.
- [stp_initialiser_ponts](file:///home/nexxo/C-Network-Simulator/src/stp.c#L6) : Initialise chaque commutateur en tant que racine locale de départ (coût à 0) et positionne temporairement tous les ports à l'état bloqué.
- [stp_diffuser_trames](file:///home/nexxo/C-Network-Simulator/src/stp.c#L143) : Transmet de manière récursive les trames BPDU contenant les informations de routage vers tous les switches adjacents.
- [stp_traiter_trame_recue](file:///home/nexxo/C-Network-Simulator/src/stp.c#L92) : Traite une trame BPDU reçue. Si les informations reçues offrent un meilleur chemin vers une racine ou identifient une racine prioritaire, met à jour les informations du switch et retourne vrai pour propager ces modifications.
- [stp_resoudre_etats_ports](file:///home/nexxo/C-Network-Simulator/src/stp.c#L264) : Analyse le réseau après convergence des échanges de BPDU afin de désigner pour chaque port son état final (RACINE, DÉSIGNÉ ou BLOQUÉ).

### 4. Affichage, Diagnostics et Conversions
- [afficher_reseau](file:///home/nexxo/C-Network-Simulator/src/affichage.c#L35) : Produit un affichage structuré sur la sortie standard détaillant chaque équipement (stations avec IP/MAC et switches avec ports).
- [afficher_etat_port_reseau](file:///home/nexxo/C-Network-Simulator/src/affichage.c#L162) : Affiche l'état de tous les ports de chaque commutateur ainsi que le meilleur BPDU reçu par chacun.
- [str_to_mac](file:///home/nexxo/C-Network-Simulator/src/affichage.c#L81) et [str_to_ipv4](file:///home/nexxo/C-Network-Simulator/src/affichage.c#L92) : Convertissent des chaînes de caractères représentant des adresses MAC ou IP en structures binaires exploitables.

## Installation

### Prérequis

- Un compilateur C supportant la norme C23 (comme un compilateur GCC récent).
- L'outil Make pour exécuter le script de build.
- Un environnement compatible POSIX (Linux, macOS ou WSL sous Windows).

### Compilation et Exécution

Le projet intègre un fichier [Makefile](file:///home/nexxo/C-Network-Simulator/Makefile) à sa racine pour automatiser le cycle de vie du logiciel :

- **Compiler le projet** :
  ```bash
  make
  ```
  Cette commande compile les fichiers sources, place les objets intermédiaires dans le répertoire `obj/` et produit l'exécutable dans `bin/`.

- **Lancer la simulation** :
  ```bash
  make run
  ```

- **Nettoyer les fichiers compilés intermédiaires** :
  ```bash
  make clean
  ```

- **Nettoyer les fichiers compilés et l'exécutable** :
  ```bash
  make fclean
  ```

- **Recompiler entièrement le projet** :
  ```bash
  make re
  ```
