# Projet OCR POC - Instructions de compilation

Ce dépôt contient trois modules principaux qui constituent la base d’un prototype de système OCR (Optical Character Recognition, ou reconnaissance optique de caractères) :

- **Solver** – Composants logiques et algorithmiques pour le traitement des données.
- **Graphics** – Interface graphique basée sur SDL pour le prétraitement des images (niveau de gris, binarisation, détection de blobs, etc.).
- **NeuralNetwork** – Moteur neuronal prévu pour la reconnaissance de caractères à venir.
---

## Prérequis

Avant la compilation, assurez-vous d’avoir installé les dépendances suivantes sur votre système :

- **GCC** (ou tout autre compilateur C)
- **SDL2** (bibliothèques de développement)
- **SDL2_ttf** (pour l’affichage de texte)
- **ImageMagick** (pour la conversion de formats d’image)
- **make**

Sur les systèmes Debian/Ubuntu, vous pouvez les installer avec la commande suivante :
```bash
sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev imagemagick
```

---

## Compilation du projet

Pour compiler l’ensemble des modules, exécutez simplement :

```bash
make
```

Cela produira trois exécutables :
- `solver`
- `graphics`
- `neural`

Si vous souhaitez compiler uniquement un module spécifique, utilisez l’une des commandes suivantes :
```bash
make solver
make graphics
make neural
```

---

## Exécution du POC graphique

Le module Graphics fournit une interface graphique basée sur SDL2 permettant d’appliquer des traitements d’image avant l’OCR.

Utilisation :
1. Placez une image nommée input.png à la racine du projet.
2. Exécutez le programme :
   ```bash
   ./graphics
   ```
3. Une fenêtre s’ouvrira affichant l’image chargée. Vous pourrez ensuite saisir dans la fenêtre de commande des instructions telles que :
   - `grayscale`
   - `binarize 200`
   - `boxes`

Ces commandes appliquent successivement des transformations sur l’image, visibles en temps réel.

---

## Exécution du POC solver

Pour lancer le programme, tapez la commande suivante dans le terminal :

```bash
./solver grid.txt MOT
```

---

## Exécution du POC neural network

Pour lancer le programme, tapez la commande suivante dans le terminal :

```bash
./neural
```

---

## Nettoyage des fichiers de compilation

Pour supprimer les exécutables et les fichiers compilés :
```bash
make clean
```

Pour recompiler entièrement le projet depuis zéro :
```bash
make re
```

---

## Remarques

- Le module graphique est encore en développement. Certaines fonctionnalités, notamment l’application d’opérations après une rotation de l’image, peuvent provoquer des erreurs de segmentation ou des incohérences visuelles.
- Assurez-vous que toutes les bibliothèques SDL requises sont correctement installées avant d’exécuter make.
- Le module NeuralNetwork est pour l’instant une structure de base destinée à être complétée lors des prochaines phases du projet OCR.

---
