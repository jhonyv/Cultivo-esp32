# Hardware

Pensado para una carpa de 1,5 m con 8 salidas a relé, sensor de temperatura/humedad y reloj de respaldo. Se puede ampliar a **2 carpas y 16 relés** con un expansor PCF8574 y hasta 2 sensores por carpa (ver [Segunda carpa](#segunda-carpa-16-relés-y-2-sensores-por-carpa)).

## Lista de compras

| Cant. | Componente | Notas |
|---|---|---|
| 1 | **ESP32 DevKit V1** (ESP32-WROOM-32, 30 o 38 pines) | Evitar versiones WROVER: usan los GPIO16/17 para PSRAM. |
| 1 | **Módulo de 8 relés 5V con optoacoplador** | Que tenga jumper `JD-VCC` para alimentar las bobinas aparte. Contactos 10 A/250 VAC. |
| 1 | **Sensor SHT40** (o SHT31) en placa breakout, I2C | Mejor si viene con funda/filtro. El SHT40 es más preciso y barato que el SHT31; el firmware detecta cualquiera de los dos. |
| 1 | **RTC DS3231** (módulo ZS-042 o similar) + pila CR2032 | Mantiene la hora sin WiFi. En el ZS-042 conviene sacar el diodo/resistencia de carga si usás CR2032 no recargable. |
| 1 | **Fuente 5V 3A** (riel DIN o tipo cargador de buena calidad) | Alimenta ESP32 + relés. Con los 8 relés activos consumen ~0,6 A. |
| 1 | Cable 4 hilos mallado o UTP (hasta 3 m) | Para llevar el SHT lejos de la caja. |
| 1 | Caja estanca IP65 + borneras/riel DIN | Separar zona de 220 V de la de 5 V. |
| 1 | Termomagnética 10 A + fusibles por salida (opcional pero recomendado) | Especialmente en la salida de luces. |
| — | Cables dupont, termocontraíble, prensacables | |

Para la segunda carpa, sumá: 1 módulo de 8 relés más, 1 **PCF8574** (módulo I2C, dirección 0x20) y los sensores (hasta 2 SHT30/SHT31/SHT40 por carpa).

Opcional para más adelante: humidificador ultrasónico, deshumidificador, sensor de humedad de suelo capacitivo, sensor de nivel del tanque.

## Pines

| Función | GPIO ESP32 | Notas |
|---|---|---|
| Relé 1 – Luz 1 | 16 | |
| Relé 2 – Luz 2 | 17 | |
| Relé 3 – Extractor (cooler out) | 18 | |
| Relé 4 – Intractor (cooler in) | 19 | |
| Relé 5 – Riego 1 | 23 | |
| Relé 6 – Riego 2 | 25 | |
| Relé 7 – Asignable 1 | 26 | |
| Relé 8 – Asignable 2 | 27 | |
| I2C SDA (SHT carpa 1 + DS3231 + PCF8574) | 21 | Bus principal |
| I2C SCL (SHT carpa 1 + DS3231 + PCF8574) | 22 | Bus principal |
| I2C2 SDA (SHT carpa 2) | 32 | Segundo bus, solo si usás la carpa 2 |
| I2C2 SCL (SHT carpa 2) | 33 | Segundo bus, solo si usás la carpa 2 |
| Relés 9-16 | P0-P7 del PCF8574 | No usan GPIO del ESP32 |
| Botón reset WiFi | 0 (BOOT) | Mantener 5 s con el equipo encendido borra la red guardada |
| LED de estado | 2 (LED de la placa) | Fijo = conectado, parpadea = modo AP |

Se eligieron pines que **no son de arranque (strapping)** ni de flash, para que los relés no hagan clic al encender y el ESP32 bootee siempre.

## Conexiones

```
Fuente 5V ──┬── VIN ESP32
            ├── JD-VCC módulo relés   (sacar jumper JD-VCC↔VCC)
            └── GND común (fuente, ESP32 y GND del módulo de relés)

ESP32 3V3 ── VCC módulo relés (lado optoacoplador)
ESP32 GPIO16,17,18,19,23,25,26,27 ── IN1..IN8

ESP32 3V3 ── VCC SHT40 ── VCC DS3231
ESP32 GND ── GND SHT40 ── GND DS3231
ESP32 21  ── SDA SHT40 ── SDA DS3231
ESP32 22  ── SCL SHT40 ── SCL DS3231
```

- Los módulos de relé típicos son **activos en bajo** (IN a GND = relé activado). El firmware viene configurado así; si el tuyo es activo en alto se cambia desde la configuración.
- Alimentando el lado de entrada del módulo con 3V3 y las bobinas con 5V (JD-VCC) los relés conmutan bien con la lógica de 3,3 V del ESP32 y quedan aislados.
- El SHT y el DS3231 comparten el bus I2C (SHT 0x44, DS3231 0x68). Los breakouts ya traen pull-ups.
- Con cable largo al SHT (>2 m) usá cable mallado y, si hay lecturas erráticas, bajá la velocidad I2C (`I2C_FREQ` en `include/config.h`).

## Segunda carpa: 16 relés y 2 sensores por carpa

Los GPIO libres del ESP32 no alcanzan para 8 relés más, así que los relés 9-16 van por un **PCF8574** colgado del mismo bus I2C (solo usa SDA/SCL).

```
PCF8574  VCC ── 3V3      GND ── GND
         SDA ── GPIO21   SCL ── GPIO22
         A0, A1, A2 ── GND        (dirección 0x20)
         P0..P7 ── IN1..IN8 del segundo módulo de relés

Segundo módulo de relés: VCC ── 3V3, JD-VCC ── 5V, GND común (igual que el primero)
```

- El PCF8574 arranca con todas las salidas en alto, o sea **relés apagados** en módulos activos en bajo (los comunes). La polaridad elegida en Ajustes vale para los dos módulos.
- Si el PCF8574 no está conectado, el equipo funciona igual con 8 relés y la página marca los relés 9-16 como "módulo no conectado".

**Sensores:** cada carpa tiene su propio bus I2C, y en cada bus entran 2 SHT: uno en **0x44** y otro en **0x45**. En los breakouts SHT30/SHT31 la dirección se elige con el pin **ADDR**: a GND = 0x44, a VCC = 0x45 (algunos traen un puente para soldar). Los SHT40 vienen de fábrica en 0x44; para el segundo comprá un SHT40-**B** (0x45) o usá un SHT31 en 0x45.

| Carpa | Bus | Sensor A | Sensor B |
|---|---|---|---|
| 1 | GPIO21 / GPIO22 | 0x44 | 0x45 |
| 2 | GPIO32 / GPIO33 | 0x44 | 0x45 |

- El clima de cada carpa es el **promedio** de sus sensores que respondan; si uno falla se usa el otro. En Inicio se ve la lectura de cada sensor.
- Conviene ponerlos a distinta altura (canopy y arriba) o en lados opuestos: el promedio representa mejor la carpa.
- La carpa 2 se activa en Ajustes > Carpas; ahí también se elige a qué carpa pertenece cada relé (por defecto 1-8 carpa 1 y 9-16 carpa 2).

## 220 V – seguridad

- Cortar siempre la **fase** con el relé (no el neutro).
- Luces LED de más de ~300 W o equipos con mucha corriente de arranque: usar el relé para mandar un **contactor** en lugar de cargarlo directo.
- Coolers y bombas inductivas: un snubber RC (o MOV) en paralelo a la carga alarga la vida de los contactos.
- Todo dentro de caja cerrada, lejos del agua y con descarga a tierra.

## Ubicación del sensor

A la altura del canopy, en el centro de la carpa, a la sombra (no bajo la luz directa) y lejos de la salida del humidificador o del intractor.
