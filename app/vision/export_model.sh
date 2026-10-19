#!/bin/sh
# Exporte YOLO11n en ONNX. À lancer sur le PC (dans le venv), jamais sur le Pi :
# l'export a besoin de PyTorch, l'exécution du .onnx seulement d'OpenCV.
set -e
cd "$(dirname "$0")"
yolo export model=yolo11n.pt format=onnx imgsz=320 opset=12 simplify=True
ls -lh yolo11n.onnx
