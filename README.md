# Projet OCR MVP - Instructions de compilation

Ce dépôt contient désormais un seul module/exécutable qui regroupe l’ancien Solver, Graphics et NeuralNetwork en une seule application OCR.

---

## Prérequis

Avant la compilation, assurez-vous d’avoir installé les dépendances suivantes sur votre système :

- GCC (ou tout autre compilateur C)
- SDL2 (bibliothèques de développement)
- SDL2_ttf (pour l’affichage de texte)
- ImageMagick (pour la conversion de formats d’image)
- make

Sur les systèmes Debian/Ubuntu, vous pouvez les installer avec la commande suivante :
sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev imagemagick

---

## Compilation du projet

Pour compiler l’application OCR, exécutez :

make

ou :

make ocr

Cela produira un seul exécutable : ocr

---

## Exécution de l’application OCR

Lancez l’application avec :

./ocr_epita

---

## Commandes disponibles dans l’application

Pour connaître les commandes utilisables une fois l’application lancée, tapez :

- man : affiche l’aide / manuel

---

## Utiliser le mode automatique

Pour lancer le pré-traitement automatique et résoudre la grille :

- auto : lance le mode automatique

## Nettoyage des fichiers de compilation

Pour supprimer l’exécutable et les fichiers compilés :
make clean

Pour recompiler entièrement le projet depuis zéro :
make re

---

## Remarques

- Assurez-vous que toutes les bibliothèques SDL requises sont correctement installées avant d’exécuter make.
