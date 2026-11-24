# Cajas para imprimir

STL listos en [`stl/`](stl). Se generan con `build.py` (Python): todas las medidas están arriba de cada caja y se pueden tocar sin romper nada.

```
pip install manifold3d trimesh numpy matplotlib
python cases/build.py                    # versión de 4 relés por carpa (RJ45)
RELAY_LINK=gx20 python cases/build.py    # caja de control para la versión de 8 por carpa (GX20)
```

| Pieza | Cant. | Medidas | Qué tiene |
|---|---|---|---|
| `caja-control.stl` | 1 | 162 × 139 × 47 mm | Placa de 9 × 7 cm con el ESP32 y el DS3231. Frente: RJ45 SENSORES 1 y 2. Atrás: RJ45 RELES 1 y 2. Derecha: jack de 5 V. Izquierda: USB. Orejas para atornillar. |
| `caja-control-tapa.stl` | 1 | 134 × 139 × 5 mm | Grabada "CULTIVO". |
| `cajita-carpa.stl` | 2 | 134 × 74 × 35 mm | Una por carpa. Abajo: RJ45, 3 JST XH de 4 (sensores I2C) y 2 JST XH de 3 (suelo). Espalda: ranuras para precintos al parante y 2 agujeros para tornillo. |
| `cajita-carpa-tapa.stl` | 2 | 134 × 74 × 5 mm | |
| `placa-keystone.stl` | 2 | 40 × 30 × 2 mm | Para la caja de potencia de cada carpa: le hacés un agujero de unos 20 × 22 mm, atornillás la placa por fuera y el keystone traba en ella. |

## Impresión

- **PETG** (aguanta mejor la humedad de la carpa que el PLA), 0,2 mm, 3 perímetros, 20 % de relleno, sin soportes.
- Cajas con el piso en la cama; tapas con la cara lisa en la cama.
- Tapas: 4 tornillos autorroscantes M3 × 10 por caja. Placas perforadas: M2,5 × 6.
- Imprimí primero una cajita y probá el keystone y los JST. Si algo entra justo, se corrige en las medidas de arriba de `build.py` (`KEYSTONE`, `JST_XH_WIN`, `DC_JACK_D`).

El cableado de cada ficha está en [docs/hardware.md](../docs/hardware.md).
