# vision

Script Python qui regarde la webcam, détecte un chien avec YOLO11n et, s'il
reste près de la gamelle quelques images d'affilée, demande un repas à
`feederd`. C'est feederd qui décide (2 repas par jour max).

« Près de la gamelle » = la boîte du chien occupe au moins `--min-area`
(15 % par défaut) de l'image. À régler une fois la webcam installée.

## Installation sur le PC (une seule fois)

```bash
sudo apt install -y python3-venv
cd ~/EC_project/app/vision
python3 -m venv .venv
. .venv/bin/activate
pip install torch torchvision --index-url https://download.pytorch.org/whl/cpu
pip install -r requirements.txt
./export_model.sh            # télécharge yolo11n.pt et crée yolo11n.onnx
```

## Tester

```bash
. .venv/bin/activate
mkdir -p samples
wget -O samples/chien.jpg <URL d'une photo de chien>

python vision.py samples/chien.jpg --frames 1 -v           # une photo
python vision.py samples/chien.mp4 -v --show                # une vidéo (q pour quitter)
```

Avec feederd lancé dans un autre terminal (`../feederd/feederd -i 1`), la
demande de repas reçoit `OK 50` ou `REFUSED ...` ; sinon
`ERROR feederd injoignable`.

## Sur le Pi

Copier `vision.py` et `yolo11n.onnx`, puis `python3 vision.py 0` (webcam).
Le Pi n'a besoin que de Python, NumPy et OpenCV (paquets Buildroot), pas de
PyTorch.
