#!/usr/bin/env python3
"""Détecte le chien près de la gamelle et demande un repas à feederd.

N'utilise qu'OpenCV et NumPy, disponibles dans Buildroot : le modèle YOLO
est exporté en ONNX au préalable sur le PC (voir export_model.sh).
La décision de servir ou non reste à feederd : ce script ne fait que demander.
"""
import argparse
import socket
import sys
import time

import cv2
import numpy as np

DOG_CLASS = 16  # index de « dog » dans les 80 classes COCO


def log(msg):
    print(time.strftime("[%Y-%m-%d %H:%M:%S]"), msg, flush=True)


def letterbox(frame, size):
    """Redimensionne en gardant les proportions et complète en gris,
    comme à l'entraînement de YOLO. Renvoie l'image, l'échelle et le décalage."""
    h, w = frame.shape[:2]
    scale = min(size / w, size / h)
    nw, nh = round(w * scale), round(h * scale)
    dx, dy = (size - nw) // 2, (size - nh) // 2
    canvas = np.full((size, size, 3), 114, dtype=np.uint8)
    canvas[dy:dy + nh, dx:dx + nw] = cv2.resize(frame, (nw, nh))
    return canvas, scale, dx, dy


def detect_dog(net, frame, size, conf_min):
    """Renvoie (confiance, (x, y, w, h)) du chien le plus probable, ou None.

    On ne garde que la meilleure détection : pas besoin de NMS puisqu'on
    veut seulement savoir si un chien est là, et à quelle distance."""
    img, scale, dx, dy = letterbox(frame, size)
    net.setInput(cv2.dnn.blobFromImage(img, 1 / 255.0, swapRB=True))
    out = net.forward()[0].T  # (N, 84) : cx, cy, w, h puis 80 scores de classe
    scores = out[:, 4 + DOG_CLASS]
    best = int(np.argmax(scores))
    conf = float(scores[best])
    if conf < conf_min:
        return None
    cx, cy, bw, bh = out[best, :4]
    x = (cx - bw / 2 - dx) / scale
    y = (cy - bh / 2 - dy) / scale
    return conf, (int(x), int(y), int(bw / scale), int(bh / scale))


def request_meal(sock_path):
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.settimeout(30)  # le moteur peut mettre quelques secondes
            s.connect(sock_path)
            s.sendall(b"REQUEST_MEAL\n")
            return s.recv(256).decode().strip()
    except OSError as e:
        return f"ERROR feederd injoignable ({e})"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("source", help="numéro de webcam (0) ou chemin d'une vidéo / image")
    p.add_argument("--model", default="yolo11n.onnx")
    p.add_argument("--size", type=int, default=320, help="taille d'entrée du modèle")
    p.add_argument("--conf", type=float, default=0.5, help="confiance minimale")
    p.add_argument("--min-area", type=float, default=0.15,
                   help="part de l'image occupée par le chien pour le considérer proche")
    p.add_argument("--frames", type=int, default=5,
                   help="images consécutives « chien proche » avant de demander")
    p.add_argument("--cooldown", type=float, default=60,
                   help="secondes minimum entre deux demandes")
    p.add_argument("--socket", default="/tmp/feeder.sock")
    p.add_argument("--show", action="store_true", help="affiche la vidéo annotée")
    p.add_argument("-v", "--verbose", action="store_true", help="une ligne par image")
    args = p.parse_args()

    net = cv2.dnn.readNetFromONNX(args.model)
    src = int(args.source) if args.source.isdigit() else args.source
    cap = cv2.VideoCapture(src)
    if not cap.isOpened():
        sys.exit(f"impossible d'ouvrir la source {args.source}")
    if isinstance(src, int):  # webcam : inutile de capturer en haute définition
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    log(f"vision prête : {args.source}, modèle {args.model}")
    streak = 0
    last_request = None

    while True:
        ok, frame = cap.read()
        if not ok:
            break

        t0 = time.monotonic()
        det = detect_dog(net, frame, args.size, args.conf)
        ms = (time.monotonic() - t0) * 1000

        close = False
        if det:
            conf, (x, y, w, h) = det
            area = w * h / (frame.shape[0] * frame.shape[1])
            close = area >= args.min_area
        streak = streak + 1 if close else 0

        if args.verbose:
            info = f"chien {conf:.2f}, {area:.0%} de l'image" if det else "pas de chien"
            log(f"{ms:4.0f} ms  {info}  série={streak}")

        now = time.monotonic()
        if streak >= args.frames and (last_request is None
                                      or now - last_request >= args.cooldown):
            last_request = now
            log(f"chien proche depuis {streak} images -> {request_meal(args.socket)}")

        if args.show:
            if det:
                color = (0, 200, 0) if close else (0, 200, 255)
                cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)
                cv2.putText(frame, f"dog {conf:.2f} {area:.0%}", (x, max(y - 8, 16)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
            cv2.imshow("vision", frame)
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    cap.release()
    if args.show:
        cv2.waitKey(0)  # garde la dernière image affichée (utile pour une photo)
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
