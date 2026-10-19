# Distributeur de croquettes (projet perso)

Distributeur automatique pour chien sur Raspberry Pi 4, image Linux construite
avec Buildroot. Le développement se fait dans WSL (Ubuntu 24.04), dans
`~/EC_project` (jamais sous `/mnt/c`).

## Architecture

- **feederd** (`app/feederd/`, C) : démon de contrôle. Seul à piloter le moteur
  et seul à décider. Règles : 2 repas max par jour, 4 h minimum entre deux,
  compteur persistant sur disque, refus si l'horloge n'est pas synchronisée.
  Interface : socket Unix, commandes texte (`REQUEST_MEAL`, `STATUS`).
  Le moteur passe par `motor.h` : `motor_sim.c` sur PC, `motor_gpio.c` à
  écrire quand le moteur sera choisi (pas encore choisi).
- **vision** (à venir, Python) : webcam USB + YOLO11n exporté en ONNX
  (imgsz 320), exécuté avec OpenCV DNN (pas de PyTorch sur la cible).
  Détecte un chien (classe COCO 16) qui s'approche et envoie `REQUEST_MEAL`.
  Plus tard : ré-entraînement pour reconnaître le chien précis.
  Principe : la vision demande, feederd décide.

## Buildroot

- `buildroot/` : sous-module git, version 2025.02 LTS, jamais modifié.
- `br2-external/` (nom `DISTRIBUTEUR`) : tout le travail du projet
  (`configs/distributeur_defconfig`, `package/`, `board/rootfs-overlay/`).
- Build hors arbre dans `output/` (ignoré par git) :
  `make -C buildroot O=$PWD/output BR2_EXTERNAL=$PWD/br2-external distributeur_defconfig`
  puis `make -C output`.
- Config actuelle : base `raspberrypi4_64_defconfig` + mot de passe root,
  dropbear (SSH), overlay.

## État

- Pas encore de Pi utilisable ni de matériel (moteur, webcam) : tout se teste
  sur PC en simulation.
- Prochaines étapes : tester feederd, script vision sur vidéos de chiens,
  paquets Buildroot (feederd, Python, OpenCV), script init, NTP.
