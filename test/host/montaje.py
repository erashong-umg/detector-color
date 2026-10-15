#!/usr/bin/env python3
"""Convierte los .ppm de la interfaz simulada en montajes PNG ampliados x2."""
import glob
import sys

from PIL import Image

carpeta = sys.argv[1]
imagenes = [Image.open(a).convert("RGB") for a in sorted(glob.glob(carpeta + "/*.ppm"))]
ESC, MARGEN = 2, 8
LADO = 240 * ESC


def montaje(lista, nombre, columnas):
    filas = (len(lista) + columnas - 1) // columnas
    lienzo = Image.new("RGB", (columnas * (LADO + MARGEN) + MARGEN, filas * (LADO + MARGEN) + MARGEN), (70, 70, 70))
    for i, im in enumerate(lista):
        x = MARGEN + (i % columnas) * (LADO + MARGEN)
        y = MARGEN + (i // columnas) * (LADO + MARGEN)
        lienzo.paste(im.resize((LADO, LADO), Image.NEAREST), (x, y))
    lienzo.save(carpeta + "/" + nombre)


montaje(imagenes[0:4], "montaje_principal.png", 2)
montaje(imagenes[4:8], "montaje_menus.png", 2)
montaje(imagenes[8:10], "montaje_mover_info.png", 2)
print("%d pantallas -> montaje_*.png en %s" % (len(imagenes), carpeta))
