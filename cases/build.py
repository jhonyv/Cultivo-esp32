"""Cajas para imprimir en 3D: caja de control (ESP32), cajita de sensores de cada carpa y placa
para el keystone de la caja de potencia.

Genera los STL en cases/stl/. Requiere:  pip install manifold3d trimesh numpy matplotlib
Uso:  python cases/build.py
Todas las medidas están en mm; las de arriba de cada caja se pueden tocar sin romper nada.
"""
import os

import numpy as np
import trimesh
from manifold3d import CrossSection, FillRule, Manifold
from matplotlib.font_manager import FontProperties
from matplotlib.textpath import TextPath

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stl")
SEG = 48  # segmentos de los círculos

# ---------- piezas comunes ----------
WALL = 2.0          # pared: el keystone RJ45 traba en paneles de 1,5 a 2,2 mm
LID_T = 2.4         # espesor de la tapa
LIP_H = 3.0         # pestaña de la tapa que entra en la caja
LIP_T = 1.6
FIT = 0.35          # juego entre tapa y caja
POST = 8.0          # columnas de las esquinas para los tornillos de la tapa
POST_HOLE = 2.6     # agujero para tornillo autorroscante M3
LID_HOLE = 3.4
ENGRAVE = 0.6       # profundidad de los textos grabados

KEYSTONE = (15.0, 16.6)      # ancho x alto del calado para un keystone RJ45 hembra
GX20_D = 20.4                # conector aviador GX20 (rosca de 20 mm)
DC_JACK_D = 11.2             # jack DC 5,5x2,1 de panel (si el tuyo es de 12 mm, agrandalo con una mecha)
JST_XH_WIN = {4: 13.4, 3: 10.9}  # ventana para un JST XH acodado de 4 o 3 pines (+1 mm de juego)
JST_WIN_H = 8.2


def box(x0, y0, z0, x1, y1, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def cyl_z(cx, cy, z0, z1, d):
    return Manifold.cylinder(z1 - z0, d / 2, d / 2, SEG).translate([cx, cy, z0])


def cyl_x(x0, x1, cy, cz, d):
    return Manifold.cylinder(x1 - x0, d / 2, d / 2, SEG).rotate([0, 90, 0]).translate([x0, cy, cz])


def cyl_y(y0, y1, cx, cz, d):
    return Manifold.cylinder(y1 - y0, d / 2, d / 2, SEG).rotate([-90, 0, 0]).translate([cx, y0, cz])


def text2d(s, size, mirror=False):
    """Texto centrado en (0,0) como CrossSection."""
    tp = TextPath((0, 0), s, size=size, prop=FontProperties(family="DejaVu Sans", weight="bold"))
    polys = [p for p in tp.to_polygons() if len(p) >= 3]
    allp = np.vstack(polys)
    cx, cy = (allp.max(0) + allp.min(0)) / 2
    polys = [((p - [cx, cy]) * ([-1, 1] if mirror else [1, 1])).tolist() for p in polys]
    return CrossSection(polys, FillRule.EvenOdd)


def engrave_front(s, size, cx, cz, y_face):
    """Grabado en una cara que mira a -y (se lee de frente)."""
    t = text2d(s, size).extrude(1.0).rotate([90, 0, 0])  # v -> z, extrusión -> -y
    return t.translate([cx, y_face + ENGRAVE, cz])


def engrave_back(s, size, cx, cz, y_face):
    t = text2d(s, size, mirror=True).extrude(1.0).rotate([90, 0, 0])
    return t.translate([cx, y_face + 1.0 - ENGRAVE, cz])


def engrave_right(s, size, cy, cz, x_face):
    t = text2d(s, size).extrude(1.0).rotate([90, 0, 0]).rotate([0, 0, 90])  # u -> y, extrusión -> +x
    return t.translate([x_face - ENGRAVE, cy, cz])


def engrave_left(s, size, cy, cz, x_face):
    t = text2d(s, size, mirror=True).extrude(1.0).rotate([90, 0, 0]).rotate([0, 0, 90])
    return t.translate([x_face - 1.0 + ENGRAVE, cy, cz])


def shell(X, Y, Z, floor):
    """Caja abierta arriba. Interior de (0,0,0) a (X,Y,Z); el piso va debajo de z=0."""
    outer = box(-WALL, -WALL, -floor, X + WALL, Y + WALL, Z)
    inner = box(0, 0, 0, X, Y, Z + 1)
    b = outer - inner
    for px, py in [(0, 0), (X - POST, 0), (0, Y - POST), (X - POST, Y - POST)]:
        b += box(px, py, 0, px + POST, py + POST, Z)
        b -= cyl_z(px + POST / 2, py + POST / 2, Z - 14, Z + 1, POST_HOLE)
    return b


def lid(X, Y, label=None, label_size=8):
    """Tapa plana con pestaña. Se imprime con la cara de afuera sobre la cama."""
    plate = box(-WALL, -WALL, 0, X + WALL, Y + WALL, LID_T)
    ring = box(FIT, FIT, LID_T, X - FIT, Y - FIT, LID_T + LIP_H) - box(
        FIT + LIP_T, FIT + LIP_T, LID_T - 1, X - FIT - LIP_T, Y - FIT - LIP_T, LID_T + LIP_H + 1)
    for px, py in [(0, 0), (X - POST, 0), (0, Y - POST), (X - POST, Y - POST)]:
        ring -= box(px - 1, py - 1, LID_T - 1, px + POST + FIT + 1, py + POST + FIT + 1, LID_T + LIP_H + 1)
    l = plate + ring
    for px, py in [(0, 0), (X - POST, 0), (0, Y - POST), (X - POST, Y - POST)]:
        l -= cyl_z(px + POST / 2, py + POST / 2, -1, LID_T + 1, LID_HOLE)
        l -= cyl_z(px + POST / 2, py + POST / 2, -1, 1.2, 6.5)  # rebaje para la cabeza
    if label:
        # Se lee desde afuera (cara que apoya en la cama), por eso va espejado.
        t = text2d(label, label_size, mirror=True).extrude(1.0).translate([X / 2, Y / 2, ENGRAVE - 1.0])
        l -= t
    return l


def standoffs(points, h, d=6.0, hole=2.2):
    m = Manifold()
    for x, y in points:
        m += cyl_z(x, y, -0.5, h, d) - cyl_z(x, y, 0.5, h + 1, hole)
    return m


# ---------- caja de control (ESP32) ----------
# Adentro: placa perforada de 9x7 cm con el ESP32 DevKit (USB hacia la izquierda), DS3231,
# las resistencias de 100k del suelo y el capacitor. Frente: 2 RJ45 de sensores (uno por carpa).
# Atrás: 2 RJ45 de relés, uno por carpa (con RELAY_LINK=gx20: 2 GX20 de 12 pines para 8 relés por carpa).
# Derecha: jack de 5 V. Izquierda: USB para programar.
RELAY_LINK = os.environ.get("RELAY_LINK", "rj45")  # "rj45": 4 relés por carpa (cultivo-4); "gx20": 8 por carpa (cultivo-8)
CX, CY, CZ = 130.0, 135.0, 45.0
C_FLOOR = 2.4
C_BOARD = (6.0, 32.0, 90.0, 70.0)  # x, y, ancho, alto de la placa perforada
C_STANDOFF_H = 5.0


def control_box():
    b = shell(CX, CY, CZ, C_FLOOR)
    bx, by, bw, bh = C_BOARD
    b += standoffs([(bx + 2.5, by + 2.5), (bx + bw - 2.5, by + 2.5), (bx + 2.5, by + bh - 2.5),
                    (bx + bw - 2.5, by + bh - 2.5)], C_STANDOFF_H)
    kw, kh = KEYSTONE
    z = 24.0
    # Frente: RJ45 de cada carpa
    for i, x in enumerate([40.0, 75.0]):
        b -= box(x - kw / 2, -WALL - 1, z - kh / 2, x + kw / 2, 1, z + kh / 2)
        b -= engrave_front(f"SENSORES {i + 1}" if RELAY_LINK == "rj45" else f"CARPA {i + 1}", 4.5, x, 9.0, -WALL)
    # Atrás: los relés de cada carpa
    if RELAY_LINK == "rj45":
        for x, s in [(40.0, "RELES 1"), (75.0, "RELES 2")]:
            b -= box(x - kw / 2, CY - 1, z - kh / 2, x + kw / 2, CY + WALL + 1, z + kh / 2)
            b -= engrave_back(s, 4.5, x, 9.0, CY + WALL)
    else:
        for x, s in [(40.0, "RELES 1-8"), (90.0, "RELES 9-16")]:
            b -= cyl_y(CY - 1, CY + WALL + 1, x, z, GX20_D)
            b -= engrave_back(s, 4.5, x, 6.5, CY + WALL)
    # Derecha: alimentación
    b -= cyl_x(CX - 1, CX + WALL + 1, 62.0, z, DC_JACK_D)
    b -= engrave_right("5V 3A", 4.5, 62.0, 9.0, CX + WALL)
    # Izquierda: USB del ESP32 para reprogramar (cubre placa con o sin zócalos)
    uy = by + bh / 2
    b -= box(-WALL - 1, uy - 6.5, 8.0, 1, uy + 6.5, 24.0)
    b -= engrave_left("USB", 4.5, uy, 30.0, -WALL)
    # Orejas para atornillar a la pared o a un tablero
    for x in [-WALL - 14, CX + WALL]:
        ear = box(x, CY / 2 - 12, -C_FLOOR, x + 14, CY / 2 + 12, -C_FLOOR + 3)
        ear -= cyl_z(x + 7 if x > 0 else x + 7, CY / 2, -C_FLOOR - 1, 2, 4.5)
        b += ear
    return b


# ---------- cajita de sensores (una por carpa) ----------
# Se cuelga del parante de la carpa (precintos) o se atornilla. El piso es la espalda de la caja.
# Cara de abajo (y = 0): keystone RJ45 + 3 JST XH de 4 (sensores I2C) + 2 JST XH de 3 (suelo).
# Todo mira hacia abajo para que no entre agua. Adentro: placa perforada de 7x5 cm con los
# JST acodados en el borde de abajo, mirando a las ventanas.
TX, TY, TZ = 130.0, 70.0, 32.0
T_FLOOR = 3.0
T_BOARD = (49.5, 3.0, 70.0, 50.0)
T_STANDOFF_H = 5.0
T_WIN_Z = (6.2, 6.2 + JST_WIN_H)  # altura de las ventanas JST sobre la espalda
T_WINDOWS = [4, 4, 4, 3, 3]  # 3 sensores I2C y 2 de suelo


def tent_box():
    b = shell(TX, TY, TZ, T_FLOOR)
    bx, by, bw, bh = T_BOARD
    b += standoffs([(bx + 2.5, by + 2.5), (bx + bw - 2.5, by + 2.5), (bx + 2.5, by + bh - 2.5),
                    (bx + bw - 2.5, by + bh - 2.5)], T_STANDOFF_H)
    # Keystone RJ45, entrada del cable desde la caja de control
    kw, kh = KEYSTONE
    b -= box(22 - kw / 2, -WALL - 1, TZ / 2 - kh / 2, 22 + kw / 2, 1, TZ / 2 + kh / 2)
    # Ventanas para los JST
    x = 48.3
    z0, z1 = T_WIN_Z
    centers = []
    for pins in T_WINDOWS:
        w = JST_XH_WIN[pins]
        b -= box(x, -WALL - 1, z0, x + w, 1, z1)
        centers.append(x + w / 2)
        x += w + 2.6
    b -= engrave_front("SENSORES I2C", 3.2, (centers[0] + centers[2]) / 2, z1 + 5.0, -WALL)
    b -= engrave_front("SUELO", 3.2, (centers[3] + centers[4]) / 2, z1 + 5.0, -WALL)
    b -= engrave_front("RJ45", 3.2, 22, TZ / 2 - kh / 2 - 3.5, -WALL) if TZ / 2 - kh / 2 > 5 else Manifold()
    # Precintos al parante (pares de ranuras) y 2 agujeros para tornillo
    for yy in [35.0, 60.0]:
        for xx in [10.0, 36.0]:
            b -= box(xx - 1.2, yy - 2.75, -T_FLOOR - 1, xx + 1.2, yy + 2.75, 1)
    for xx, yy in [(23.0, 47.5), (90.0, 62.0)]:
        b -= cyl_z(xx, yy, -T_FLOOR - 1, 1, 4.2)
    return b


# ---------- placa para el keystone de la caja de potencia ----------
# Para la caja de térmicas (no impresa): se le hace un agujero de ~20 x 22 mm en una pared y se atornilla
# esta placa por fuera; el keystone traba en ella (necesita 1,5 a 2,2 mm de espesor).
PX, PY = 40.0, 30.0


def keystone_plate():
    kw, kh = KEYSTONE
    p = box(0, 0, 0, PX, PY, WALL)
    p -= box(PX / 2 - kw / 2, PY / 2 - kh / 2, -1, PX / 2 + kw / 2, PY / 2 + kh / 2, WALL + 1)
    for x in [4.5, PX - 4.5]:
        p -= cyl_z(x, PY / 2, -1, WALL + 1, 3.4)
    return p


def save(m, name):
    mesh = m.to_mesh()
    tm = trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts))
    path = os.path.join(OUT, name)
    tm.export(path)
    bb = tm.bounds
    print(f"{name}: {bb[1][0] - bb[0][0]:.0f} x {bb[1][1] - bb[0][1]:.0f} x {bb[1][2] - bb[0][2]:.0f} mm, "
          f"estanco={tm.is_watertight}")
    return tm


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    save(control_box(), "caja-control.stl")
    save(lid(CX, CY, "CULTIVO", 12), "caja-control-tapa.stl")
    save(tent_box(), "cajita-carpa.stl")
    save(lid(TX, TY), "cajita-carpa-tapa.stl")
    if RELAY_LINK == "rj45":
        save(keystone_plate(), "placa-keystone.stl")
