# Cross Matrix

Matrice de routage à grille croisée (VST3 / AU), à charger dans Pro Tools via Blue Cat Audio Patchwork.

- **Entrées en ordonnées (lignes), sorties en abscisse (colonnes)**, de 1 à 16 canaux de chaque côté. La matrice prend la largeur de canaux que lui donne l'hôte (mono, stéréo, 5.1, 7.1, ambisonique, discret…).
- **Division** : une entrée peut alimenter plusieurs sorties.
- **Mélange** : plusieurs entrées peuvent être additionnées sur une même sortie.
- Gain par point de croisement, de -60 dB à +12 dB.
- **16 snapshots** (presets) enregistrés dans la session, plus export et import en fichiers.
- **Changement de snapshot automatisable**, avec un fondu réglable et un morph continu entre deux snapshots.

## Installation

Les plug-ins sont déjà compilés et installés :

- `~/Library/Audio/Plug-Ins/VST3/Cross Matrix.vst3`
- `~/Library/Audio/Plug-Ins/Components/Cross Matrix.component`

Dans Patchwork, lancez un rescan des plug-ins, puis insérez **RolandCahen › Cross Matrix** dans un slot.

Pour recompiler :

```bash
cd ~/Developer/CrossMatrix && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j 10
```

## Utilisation de la grille

| Geste | Effet |
|---|---|
| Clic sur une cellule | Active la cellule (0 dB) ou la coupe |
| Glisser verticalement | Règle le gain (Maj enfoncée = réglage fin) |
| Double-clic | Remet la cellule à 0 dB |
| Clic droit sur une cellule | Choix d'une valeur : +6, +3, 0, -3, -4.5, -6 … dB, ou couper |
| Clic sur un en-tête d'entrée | Envoie cette entrée vers toutes les sorties, ou coupe toute la ligne |
| Clic sur un en-tête de sortie | Mélange toutes les entrées sur cette sortie (à 0 dB ou compensé), ou coupe toute la colonne |

Les vumètres d'entrée sont à droite des lignes, ceux de sortie sous les en-têtes de colonnes. Le petit trait blanc en bas de chaque cellule montre le gain réellement appliqué, ce qui permet de suivre un fondu ou un morph en cours.

## Snapshots (presets)

- **Snapshot** : choisit le snapshot actif. C'est aussi celui que la grille modifie.
- **Nom**, **Copier / Coller / Effacer / Identité** : gestion du snapshot actif.
- **Ouvrir / Enregistrer snapshot…** : un snapshot dans un fichier `.xmsnap`.
- **Ouvrir / Enregistrer banque…** : les 16 snapshots dans un fichier `.xmbank`.
  Dossier par défaut : `~/Documents/Cross Matrix Presets`.
- Les 16 snapshots sont enregistrés avec la session Pro Tools (dans l'état de Patchwork). Ils apparaissent aussi comme « programmes » dans le menu de presets de l'hôte.

## Automation

Le plug-in expose 5 paramètres automatisables :

| Paramètre | Rôle |
|---|---|
| **Snapshot** (1–16) | Passe d'un snapshot à un autre. L'automation est en escalier. |
| **Transition** (0–10 000 ms) | Durée du fondu à chaque changement de Snapshot ou de Morph Target. |
| **Morph Target** (1–16) | Second snapshot utilisé par le morph. |
| **Morph** (0–100 %) | Interpolation continue entre Snapshot (0 %) et Morph Target (100 %). Se dessine à la courbe. |
| **Master** (dB) | Gain général de sortie. |

Deux façons de passer d'un preset à l'autre :

1. **Sauts avec fondu** : automatisez *Snapshot*. Chaque changement de valeur déclenche un fondu de la durée réglée dans *Transition* (que vous pouvez aussi automatiser).
2. **Courbe libre** : réglez *Snapshot* = A et *Morph Target* = B, puis dessinez la courbe de *Morph* dans Pro Tools pour aller de A vers B à la vitesse et selon la forme voulues.

Dans Patchwork, ces paramètres doivent être reliés aux paramètres que Patchwork expose à Pro Tools, grâce à son système de mapping de paramètres. Ils deviennent ensuite automatisables dans Pro Tools comme ceux de n'importe quel plug-in.

Tous les changements de gain passent par une rampe (10 ms au minimum), ce qui évite les clics.

## Ordre des canaux

Les en-têtes affichent le nom de chaque canal (L, R, C, Lfe, Ls…) selon le format négocié avec l'hôte. Avec un format discret, ils affichent seulement le numéro du canal.

## Tests

```bash
cmake --build build --target RoutingTest && ./build/RoutingTest_artefacts/Release/RoutingTest
```

Le test vérifie la division, le mélange, la transition progressive, le morph et la sauvegarde/restauration de l'état.
