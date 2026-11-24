# Hardware

Pensado para una carpa de 1,5 m con 8 salidas a relé, sensor de temperatura/humedad y reloj de respaldo. Se puede ampliar a **2 carpas y 16 relés** con un expansor PCF8574 y sensores detectados solos en cada carpa (ver [Segunda carpa](#segunda-carpa-16-relés) y [Sensores de cada carpa](#sensores-de-cada-carpa-cable-rj45)).

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

Para la segunda carpa, sumá: 1 módulo de 8 relés más, 1 **PCF8574** (módulo I2C, dirección 0x20) y los sensores.

Por carpa, opcionales y detectados solos: un segundo SHT, un **SCD40/SCD41** (CO₂), un **BH1750** (luz) y hasta 2 **sensores capacitivos de humedad de suelo v1.2/v2.0** con una resistencia de **100 kΩ** cada uno. Ver [Sensores de cada carpa](#sensores-de-cada-carpa-cable-rj45).

Opcional para más adelante: humidificador ultrasónico, deshumidificador, sensor de nivel del tanque.

## Pines

| Función | GPIO ESP32 | Notas |
|---|---|---|
| Relé 1 – Luz 1 | 16 | |
| Relé 2 – Luz 2 | 17 | |
| Relé 3 – Extractor (cooler out) | 18 | |
| Relé 4 – Intractor (cooler in) | 19 | |
| Relé 5 – Riego (bomba) | 23 | |
| Relé 6 – Asignable 1 | 25 | |
| Relé 7 – Asignable 2 | 26 | |
| Relé 8 – Asignable 3 | 27 | |
| I2C SDA (SHT carpa 1 + DS3231 + PCF8574) | 21 | Bus principal |
| I2C SCL (SHT carpa 1 + DS3231 + PCF8574) | 22 | Bus principal |
| I2C2 SDA (sensores carpa 2) | 4 | Segundo bus, solo si usás la carpa 2 |
| I2C2 SCL (sensores carpa 2) | 5 | Segundo bus, solo si usás la carpa 2 |
| Suelo carpa 1, sensores 1 y 2 | 36 (VP) y 39 (VN) | Analógicos, opcionales |
| Suelo carpa 2, sensores 1 y 2 | 32 y 33 | Analógicos, opcionales |
| Tanque TRIG / ECHO | 13, 14 / 34, 35 | Opcional, ver abajo |
| Relés 9-16 | P0-P7 del PCF8574 | No usan GPIO del ESP32 |
| Botón reset WiFi | 0 (BOOT) | Mantener 5 s con el equipo encendido borra la red guardada |
| LED de estado | 2 (LED de la placa) | Fijo = conectado, parpadea = modo AP |

Los relés están en pines que **no son de arranque (strapping)** ni de flash, para que no hagan clic al encender y el ESP32 bootee siempre. El GPIO5 sí es de arranque, pero en el bus I2C queda en alto por la pull-up, que es justo lo que necesita para bootear. Los sensores de suelo van en pines del ADC1 (32 a 39), los únicos analógicos que leen con el WiFi prendido.

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

## Segunda carpa: 16 relés

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
- La carpa 2 se activa en Ajustes > Carpas; ahí también se elige a qué carpa pertenece cada relé (por defecto 1-8 carpa 1 y 9-16 carpa 2).

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

**Resistencias pull-up.** En I2C las líneas SDA y SCL las sube a 3,3 V una resistencia (pull-up) y los sensores solo las bajan. Casi todos los módulos traen la suya de 4,7 kΩ o 10 kΩ. Con 3 módulos en el mismo bus quedan 3 en paralelo y la línea queda demasiado "fuerte", y con cable largo puede fallar. Dejá **una sola por bus**: la del módulo más cercano al ESP32, o un par de 4,7 kΩ soldadas en la cajita. En los otros módulos sacá la resistencia o cortá el puente que la conecta (suele decir PU o tener dos resistencias marcadas 472 o 103 al lado de SDA/SCL).

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

## 220 V – seguridad

- Cortar siempre la **fase** con el relé (no el neutro).
- Luces LED de más de ~300 W o equipos con mucha corriente de arranque: usar el relé para mandar un **contactor** en lugar de cargarlo directo.
- Coolers y bombas inductivas: un snubber RC (o MOV) en paralelo a la carga alarga la vida de los contactos.
- Todo dentro de caja cerrada, lejos del agua y con descarga a tierra.

## Ubicación del sensor

A la altura del canopy, en el centro de la carpa, a la sombra (no bajo la luz directa) y lejos de la salida del humidificador o del intractor.
