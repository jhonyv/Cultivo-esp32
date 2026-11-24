# Hardware

Pensado para **2 carpas con 4 relés cada una** (Luz, Extractor, Intractor y Riego), sensores que se detectan solos en cada carpa y reloj de respaldo. Hay dos versiones del firmware, iguales salvo la cantidad de relés:

| Versión (environment de PlatformIO) | Relés | Cómo se conectan |
|---|---|---|
| **`cultivo-4`** (la que sube el botón de VS Code) | 4 por carpa, 8 en total | Directo a 8 GPIO del ESP32. Un cable de red por carpa hasta su caja de potencia. |
| `cultivo-8` | 8 por carpa, 16 en total | 1-8 directo al ESP32, 9-16 por un expansor PCF8574. Ver [Versión de 8 relés por carpa](#versión-de-8-relés-por-carpa-cultivo-8). |

Para subir la de 8: `pio run -e cultivo-8 -t upload` (o elegir `env:cultivo-8` abajo en la barra de PlatformIO). Al pasar de una a otra, los relés vuelven a sus nombres por defecto y el resto de la configuración (WiFi, Telegram, climas, sensores) se conserva.

Hay un **mapa de conexión** con dibujo y las cajas para imprimir en [`cases/`](../cases).

## Lista de compras

| Cant. | Componente | Notas |
|---|---|---|
| 1 | **ESP32 DevKit V1** (ESP32-WROOM-32, 30 o 38 pines) | Evitar versiones WROVER: usan los GPIO16/17 para PSRAM. |
| 2 | **Módulo de 4 relés 5 V con optoacoplador** | Uno por carpa. Que tenga jumper `JD-VCC` para alimentar las bobinas aparte. Contactos 10 A/250 VAC. |
| 2 | **Fuente 5 V para riel DIN** (tipo HDR-15-5) | Una por caja de potencia, para las bobinas de los relés. |
| 1 | **Fuente 5 V 3 A** tipo cargador + jack de panel 5,5 × 2,1 | Alimenta el ESP32 y los sensores. Así la caja de control no tiene 220 V. |
| 1 | **RTC DS3231** (módulo ZS-042 o similar) + pila CR2032 | Mantiene la hora sin WiFi. En el ZS-042 conviene sacar el diodo/resistencia de carga si usás CR2032 no recargable. |
| 1-2 por carpa | **Sensor SHT40** (o SHT31) en placa breakout, I2C | Mejor si viene con funda/filtro. El firmware detecta cualquiera de los dos. |
| 8 | **Keystone RJ45 Cat6 hembra** | 4 en la caja de control, 1 por cajita de sensores y 1 por caja de potencia. |
| 4 | **Cable Cat6 con fichas** | 2 a las cajitas de sensores y 2 a las cajas de potencia (hasta ~3 m). |
| 2 | Térmica (y diferencial de 30 mA si no hay uno en el tablero) | Una por caja de potencia. |
| 8 | Tomas para riel DIN | 4 por caja de potencia. |
| — | Placas perforadas, JST XH, resistencias de 100 kΩ, capacitor de 470 µF, tornillos | Detalle en el mapa de conexión. |

Por carpa, opcionales y detectados solos: un segundo SHT, un **SCD40/SCD41** (CO₂), un **BH1750** (luz) y hasta 2 **sensores capacitivos de humedad de suelo v1.2/v2.0** con una resistencia de **100 kΩ** cada uno. Ver [Sensores de cada carpa](#sensores-de-cada-carpa-cable-rj45).

Opcional para más adelante: humidificador ultrasónico, deshumidificador, sensor de nivel del tanque.

## Pines

| Función | GPIO ESP32 | Notas |
|---|---|---|
| Carpa 1: relés 1-4 (Luz, Extractor, Intractor, Riego) | 16, 17, 18, 19 | RJ45 "RELES 1" |
| Carpa 2: relés 5-8 (Luz, Extractor, Intractor, Riego) | 23, 25, 26, 27 | RJ45 "RELES 2" |
| I2C SDA (sensores carpa 1 + DS3231) | 21 | Bus principal |
| I2C SCL (sensores carpa 1 + DS3231) | 22 | Bus principal |
| I2C2 SDA (sensores carpa 2) | 4 | Segundo bus |
| I2C2 SCL (sensores carpa 2) | 5 | Segundo bus |
| Suelo carpa 1, sensores 1 y 2 | 36 (VP) y 39 (VN) | Analógicos, opcionales |
| Suelo carpa 2, sensores 1 y 2 | 32 y 33 | Analógicos, opcionales |
| Tanque TRIG / ECHO | 13, 14 / 34, 35 | Opcional, ver abajo |
| Botón reset WiFi | 0 (BOOT) | Mantener 5 s con el equipo encendido borra la red guardada |
| LED de estado | 2 (LED de la placa) | Fijo = conectado, parpadea = modo AP |

Los relés están en pines que **no son de arranque (strapping)** ni de flash, para que no hagan clic al encender y el ESP32 bootee siempre. El GPIO5 sí es de arranque, pero en el bus I2C queda en alto por la pull-up, que es justo lo que necesita para bootear. Los sensores de suelo van en pines del ADC1 (32 a 39), los únicos analógicos que leen con el WiFi prendido.

Los nombres y funciones son los de fábrica: en Ajustes > Relés cada uno se renombra, se le cambia la función (luz, extractor, riego, humidificador, aire, genérico…) o se pasa a la otra carpa, sin tocar cables.

## Relés: cable RJ45 a la caja de potencia de cada carpa

Cada carpa tiene su **caja de potencia** (caja de térmicas con riel DIN): térmica, una fuente de 5 V para riel DIN, el módulo de 4 relés y 4 tomas DIN. Desde la caja de control sale un cable de red (Cat5e/Cat6) por carpa, con el mismo tipo de keystone que los sensores.

| Pin (T568B) | Hilo | Señal | En el módulo de relés |
|---|---|---|---|
| 1 | blanco-naranja | GND | — (ver abajo) |
| 2 | naranja | Relé 1 · Luz | IN1 |
| 3 | blanco-verde | GND | — |
| 4 | azul | 3,3 V | VCC |
| 5 | blanco-azul | GND | — |
| 6 | verde | Relé 2 · Extractor | IN2 |
| 7 | blanco-marrón | Relé 3 · Intractor | IN3 |
| 8 | marrón | Relé 4 · Riego | IN4 |

| Carpa | IN1 | IN2 | IN3 | IN4 |
|---|---|---|---|---|
| 1 | GPIO16 | GPIO17 | GPIO18 | GPIO19 |
| 2 | GPIO23 | GPIO25 | GPIO26 | GPIO27 |

```
Caja de potencia de cada carpa (jumper JD-VCC sacado):
  RJ45 pin 4 (3,3 V) ── VCC   (ficha de entrada del módulo)
  RJ45 pines 2, 6, 7, 8 ── IN1, IN2, IN3, IN4
  Fuente DIN 5 V  + ── JD-VCC      − ── GND (ficha del jumper, al lado de JD-VCC)
```

- El optoacoplador se prende con VCC e IN, que vienen los dos del ESP32, y las bobinas con la fuente de 5 V de la caja. Así **los 220 V y el ruido de las bobinas quedan aislados** del ESP32. Por eso los GND del cable no se conectan al módulo (quedan sueltos y aislados en esa punta): sirven de blindaje para las señales.
- El 3,3 V y los GND están en los mismos pines que en el cable de sensores: si se cruzan los cables por error no hay cortocircuito. Igual rotulá las puntas.
- Los módulos de relé típicos son **activos en bajo** (IN a GND = relé activado). El firmware viene configurado así; si el tuyo es activo en alto se cambia en Ajustes > Equipo.
- **Probalo en la mesa antes de armar.** Algunos módulos traen el LED indicador en serie con el optoacoplador y con 3,3 V en VCC enganchan justo. Si un relé no engancha firme, usá un módulo con jumper de disparo alto/bajo ("high/low level trigger"): se alimenta con la fuente de 5 V (DC+ y DC−), los GND del cable van a DC−, IN igual que arriba, y se elige la polaridad en Ajustes.

## Alimentación de la caja de control

```
Cargador 5 V 3 A ── jack ──┬── VIN ESP32
                           ├── 470 µF a GND, cerca del ESP32
                           └── GND común (ESP32, DS3231, GND de los 4 RJ45)

ESP32 3V3 ── DS3231 VCC ── pin 4 de los 4 RJ45 (sensores y VCC de los módulos de relés)
ESP32 21/22 ── DS3231 SDA/SCL ── RJ45 sensores carpa 1
```

- Si usás dos SCD40 (uno por carpa), cada uno pide picos de 200 mA en 3,3 V. Sumá un regulador AMS1117 de 3,3 V alimentado desde los 5 V y sacá de ahí el pin 4 de los RJ45 de sensores, así no se cae el ESP32.

## Versión de 8 relés por carpa (`cultivo-8`)

Los GPIO libres del ESP32 no alcanzan para 16 relés, así que en esta versión los relés 9-16 van por un **PCF8574** colgado del bus I2C principal (solo usa SDA/SCL), y cada carpa usa un módulo de 8 relés. Por defecto cada carpa tiene Luz 1, Luz 2, Extractor, Intractor, Riego y 3 asignables.

```
Relés 1-8 (carpa 1): GPIO16,17,18,19,23,25,26,27 ── IN1..IN8

PCF8574  VCC ── 3V3      GND ── GND
         SDA ── GPIO21   SCL ── GPIO22
         A0, A1, A2 ── GND        (dirección 0x20)
         P0..P7 ── IN1..IN8 del módulo de la carpa 2

Cada módulo de 8: VCC ── 3V3, JD-VCC ── 5 V (jumper sacado)
```

- Un RJ45 no alcanza para 8 señales más alimentación: se usa una ficha aviador **GX20 de 12 pines** por carpa (8 señales, 3,3 V, 5 V y 2 GND). La caja de control para esta versión se genera con `RELAY_LINK=gx20 python3 cases/build.py`.
- El PCF8574 arranca con todas las salidas en alto, o sea **relés apagados** en módulos activos en bajo (los comunes). La polaridad elegida en Ajustes vale para los dos módulos.
- Si el PCF8574 no está conectado, el equipo funciona igual con 8 relés y la página marca los relés 9-16 como "módulo no conectado".

## Sensores de cada carpa (cable RJ45)

Cada carpa tiene **su propio bus I2C** y su cable de red (Cat5e/Cat6, hasta ~3 m) desde la caja del ESP32 hasta una cajita adentro de la carpa. En la cajita hay fichas JST en paralelo para enchufar los sensores, con cables cortos (30-50 cm).

| Par del Cat6 (T568B) | Hilo de color | Hilo blanco |
|---|---|---|
| 1 (naranja) | SDA | GND |
| 2 (verde) | SCL | GND |
| 3 (azul) | 3,3 V | GND |
| 4 (marrón) | Suelo 1 | Suelo 2 |

| Carpa | SDA | SCL | Suelo 1 | Suelo 2 |
|---|---|---|---|---|
| 1 | GPIO21 | GPIO22 | GPIO36 (VP) | GPIO39 (VN) |
| 2 | GPIO4 | GPIO5 | GPIO32 | GPIO33 |

Los tres GND se unen en las dos puntas. Cada señal viaja trenzada con un GND, así no le entra ruido de los coolers.

**Detección automática.** Cada 30 s el equipo revisa qué hay en el bus de cada carpa y muestra solo lo que encuentra, con nombre puesto:

| Sensor | Dirección | Aparece como |
|---|---|---|
| SHT30/31/40 | 0x44 y 0x45 | Temp/Hum (Temp/Hum 1 y 2 si hay dos) |
| SCD40/SCD41 | 0x62 | CO₂ |
| BH1750 | 0x23 (ADDR a GND) o 0x5C (ADDR a VCC) | Luz |

- Con 2 SHT, el clima (y el VPD) es el **promedio** de los dos; si uno falla, sigue con el otro. En los SHT30/31 la dirección se elige con el pin **ADDR** (a GND = 0x44, a VCC = 0x45). Los SHT40 vienen en 0x44; para el segundo usá un SHT40-**B** o un SHT31 en 0x45.
- El SCD40 también mide temperatura y humedad, pero se calienta un poco: se usa solo si no hay ningún SHT.
- Si desenchufás un sensor, a los pocos segundos desaparece del panel. Si lo enchufás, aparece solo en menos de 30 s.

**Resistencias pull-up.** En I2C las líneas SDA y SCL las sube a 3,3 V una resistencia (pull-up) y los sensores solo las bajan. Casi todos los módulos traen la suya de 4,7 kΩ o 10 kΩ, y quedan en paralelo. Con hasta 3 o 4 módulos en el mismo bus sigue dentro de lo que banca el I2C, y con el cable largo hasta ayuda: **no hace falta sacar ni agregar nada**. Si alguna vez ves lecturas que saltan, primero bajá la velocidad del bus (`I2C_FREQ` en `include/config.h`); si sigue, sacá la resistencia de los módulos que estén más lejos (cortando el puente que dice PU, o las dos resistencias marcadas 472 o 103 al lado de SDA/SCL).

## Humedad de suelo (opcional)

Sensores **capacitivos** v1.2/v2.0, hasta 2 por carpa, por el par 4 del cable de esa carpa. Se alimentan del mismo 3,3 V y GND que los sensores I2C.

```
Sensor capacitivo: VCC ── 3,3 V   GND ── GND   AOUT ── Suelo 1 (o Suelo 2)

En la caja del ESP32, en cada entrada de suelo:
GPIO36/39/32/33 ──┬── hilo del sensor
                  └── 100 kΩ ── GND
```

- La resistencia de 100 kΩ a GND es para la **detección automática**: sin sensor el pin lee 0 y el panel no muestra nada. Con sensor, la tarjeta de humedad de suelo aparece sola en Riego.
- Alimentalos con **3,3 V**, no con 5 V: así la salida nunca pasa de lo que lee el ESP32.
- Usá sensores **capacitivos**, no los resistivos de dos patitas: esos se corroen en semanas. Protegé la plaquita de arriba con termocontraíble o silicona y enterralos solo hasta la rayita.
- Hasta 5 o 6 m de cable andan bien; el equipo promedia 16 lecturas por vez. **Calibralos ya instalados**, con todo el cable puesto (Riego > Humedad de suelo > Calibrar).
- La humedad de suelo queda siempre en el historial. **Control del riego por sensor de suelo** viene apagado: el riego lo manda el horario. Prendido, en cada relé de riego se elige:
  - **Saltear** un riego programado si la tierra está arriba de un %.
  - **Regar extra** si baja de un %: solo en Auto, con luces prendidas, al menos 1 h entre riegos extra y con un tope por día.
- **Avisar por Telegram si baja de X %** funciona aunque el control esté apagado.

## Nivel del tanque (opcional)

Un sensor ultrasónico **a prueba de agua JSN-SR04T** (o AJ-SR04M) por carpa, montado en la tapa del tanque apuntando al agua.

| Carpa | TRIG | ECHO |
|---|---|---|
| 1 | GPIO13 | GPIO34 |
| 2 | GPIO14 | GPIO35 |

```
JSN-SR04T  VCC ── 5V      GND ── GND
           TRIG ── GPIO13 (carpa 1) / GPIO14 (carpa 2)
           ECHO ── 1 kΩ ──┬── GPIO34 (carpa 1) / GPIO35 (carpa 2)
                          └── 2 kΩ ── GND      (divisor: el ECHO sale a 5 V)
```

- El sensor tiene que quedar al menos **25 cm** por encima del agua con el tanque lleno (más cerca no mide).
- Se activa en Riego > Tanque: cargás la distancia con el tanque vacío y lleno (o con los botones "lectura actual") y los litros.
- **Avisarme debajo de X %** manda una alerta por Telegram; **No prender la bomba debajo de X %** bloquea el riego de esa carpa (horario, extra y "Regar ahora") para que la bomba no trabaje en seco.

## 220 V – caja de potencia y seguridad

- Entrada: fase y neutro del tablero → **térmica** → (diferencial de 30 mA, si no hay uno en el tablero de la casa: con agua cerca va sí o sí).
- La fase va al **COM** de cada relé y el contacto **NO** a la fase de su toma. Neutro y tierra, comunes a los 4 tomas.
- Cortar siempre la **fase** con el relé (no el neutro).
- Luces LED de más de ~300 W o equipos con mucha corriente de arranque: usar el relé para mandar un **contactor** en lugar de cargarlo directo.
- Coolers y bombas inductivas: un snubber RC (o MOV) en paralelo a la carga alarga la vida de los contactos.
- Todo dentro de caja cerrada, lejos del agua y con descarga a tierra.

## Ubicación del sensor

A la altura del canopy, en el centro de la carpa, a la sombra (no bajo la luz directa) y lejos de la salida del humidificador o del intractor.
