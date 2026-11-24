# Cultivo-esp32

Controlador para 2 carpas de cultivo con ESP32: **4 relés por carpa** (Luz, Extractor, Intractor y Riego, cada uno reasignable), sensores que se detectan solos en cada carpa (SHT40/SHT31 con VPD, CO₂, luz, humedad de suelo), reloj DS3231 de respaldo y panel web. La página tiene un selector Carpa 1 / Carpa 2 y cada carpa lleva su clima, horarios, alertas e historial.

Hay una versión de **8 relés por carpa** (16 en total, con un expansor PCF8574) para más adelante.

- Hardware, lista de compras y pines: [docs/hardware.md](docs/hardware.md)
- Cajas para imprimir en 3D: [cases/](cases)

## Compilar y cargar

Con [PlatformIO](https://platformio.org/) (VS Code + extensión PlatformIO, o CLI):

```
pio run -t upload        # versión de 4 relés por carpa (cultivo-4, la de siempre)
pio device monitor
```

Hay dos versiones del firmware, iguales salvo la cantidad de relés:

| Environment | Relés |
|---|---|
| `cultivo-4` (por defecto, la que sube el botón de VS Code) | 4 por carpa, 8 en total, directo al ESP32 |
| `cultivo-8` | 8 por carpa, 16 en total, los 9-16 por un PCF8574 |

Para la de 8: `pio run -e cultivo-8 -t upload`, o elegí `env:cultivo-8` en la barra de abajo de VS Code. Al cambiar de versión, los relés vuelven a sus nombres por defecto y el resto de la configuración queda.

## Primer uso

1. Al encender sin WiFi configurado, el ESP32 crea la red **`Cultivo-XXXX`** (clave `cultivo123`).
2. Conectate desde el celular: se abre el portal (si no, entrá a `http://192.168.4.1`).
3. En **WiFi** tocá *Buscar*, elegí tu red, poné la clave y *Conectar*.
4. Ya en tu red, entrá a `http://cultivo.local` o a la IP que muestra el portal. La red `Cultivo-XXXX` se apaga sola a los 2 minutos.

Si el WiFi falla, la red `Cultivo-XXXX` vuelve a aparecer para reconfigurarlo. Manteniendo el botón **BOOT** 5 segundos se borra la red guardada.

## Panel web

- **Inicio:** temperatura, humedad y VPD contra los objetivos, día/noche, gráfico de las últimas 24 h y control rápido de cada relé (Off / Auto / On).
- **Iluminación:** un horario para todas las luces de la carpa (cada luz con su Off / Auto / On): 12/12, 16/8, 18/6, 20/4 o personalizado (horas prendida/apagada libres, por ej. 13/13) y hora de encendido.
- **Temperatura y humedad:** rangos de día y de noche (con presets por etapa); la humedad se controla por **VPD objetivo** (humidificador/deshumidificador), la temperatura con aire acondicionado/calefactor; histéresis y renovación de aire. Cada extractor/intractor trabaja **por clima** (prende si hace calor, si hay humedad de más o en la renovación), **por horario** (franjas) o **permanente** (24/7); cualquier salida se puede pasar a humidificador, deshumidificador, calefactor o aire acondicionado (prende con calor junto con los coolers; pensado para el aire de la sala).
- **Riego:** hasta 12 riegos por salida (hora + segundos), “Regar ahora”, una guía rápida y un **plan por etapa** (vegetativo, estiramiento, flora media, flora tardía; coco o tierra) que arma P0/P1/P2/P3 con los segundos justos según el caudal calibrado de la bomba, las macetas y sus litros.
- **Historial:** día (cada 5 min), semana (por hora) y mes (por día) con temperatura, humedad y VPD (promedio y mín–máx), más horas de luz, coolers y tiempo de riego por día. Semana y mes se guardan en la flash y sobreviven cortes de luz (35 días).
- **Ajustes:** WiFi, nombres y función de cada relé, polaridad del módulo, offset de hoja, clave del AP y zona horaria.

En modo **Auto** cada relé sigue su automatización; On/Off lo fuerza a mano. Si el sensor falla, los coolers quedan prendidos y humidificador/deshumidificador/calefactor/aire apagados. Sin hora válida, luces y riego en Auto quedan apagados.

La página está en `web/index.html`; al compilar, `tools/embed_web.py` la comprime y la mete en el firmware.

La configuración (nombres, tipos y modo de cada relé, offset de hoja para el VPD, clave del AP, zona horaria) queda guardada en la memoria flash y sobrevive a cortes de luz. Los relés arrancan siempre apagados hasta leer la config.

## Nivel del tanque (opcional)

Con un ultrasónico JSN-SR04T en la tapa del tanque, la página muestra el % y los litros, avisa por Telegram si baja de lo que elijas y puede bloquear la bomba para que no trabaje en seco. Cableado en [docs/hardware.md](docs/hardware.md#nivel-del-tanque-opcional).

## Sensores que se detectan solos

Cada carpa tiene su propio bus I2C (cable RJ45 hasta una cajita en la carpa). El equipo detecta solo lo que enchufes, le pone nombre y muestra únicamente eso: hasta 2 SHT (Temp/Hum, se promedian para el VPD), un SCD40/41 (CO₂) y un BH1750 (luz). Pines, colores del cable y pull-ups en [docs/hardware.md](docs/hardware.md#sensores-de-cada-carpa-cable-rj45).

## Humedad de suelo (opcional)

Hasta 2 sensores capacitivos por carpa, directo a pines analógicos. Si hay alguno enchufado, la humedad aparece sola en Riego y queda en el historial; también se puede pedir un aviso por Telegram si la tierra baja de un %. El **control del riego por sensor de suelo** viene apagado, porque el riego lo manda el horario. Si lo prendés, cada riego puede **saltearse** si la maceta sigue mojada o dar **riegos extra** si se seca (con tope diario). Cableado en [docs/hardware.md](docs/hardware.md#humedad-de-suelo-opcional).

## Sin internet o sin router

Todo lo que es automático (luces, clima, riego, historial) corre en el ESP32 y no necesita internet; la hora la mantiene el DS3231.

- **Sin internet pero con el WiFi de casa andando:** con el celular en ese WiFi, entrá a `http://cultivo.local` o a la IP que muestra Ajustes.
- **Sin router:** el equipo levanta su red propia **Cultivo-XXXX** (aparece sola hasta 1 min después de caerse el WiFi). Conectá el celular, con la clave `cultivo123` salvo que la hayas cambiado, y abrí **192.168.4.1**: es el mismo panel completo.
- **Sin WiFi en el lugar:** conectate a la red propia, andá a Ajustes > WiFi y tocá **Activar modo offline (sin WiFi)**. Borra la red guardada y deja la red propia siempre prendida, también después de un corte de luz. Cada vez que llegues, conectás el celular a **Cultivo-XXXX** y abrís **192.168.4.1**. La nube y Telegram no andan en este modo. Si el celular avisa que la red no tiene internet, elegí seguir conectado.
- En Ajustes > Equipo, **Red propia del equipo: Siempre prendida** la deja disponible todo el tiempo, aunque haya WiFi. Mientras haya un celular conectado a la red propia y no haya WiFi de casa, el equipo reintenta conectarse cada 5 min en vez de cada 1, para no cortarte.

## Acceso desde cualquier lado

- **Telegram (recomendado):** en Ajustes > Telegram cargás el token de un bot creado con @BotFather y tu chat id. Desde cualquier lado: `/estado`, `/luces on|off|auto`, `/rele N on|off|auto`, `/regar N SEG`. Avisa si la temperatura o la humedad se van de rango más de 10 min, si falla el sensor o si el equipo se reinició (corte de luz). No hace falta tocar el router.
- **Panel en la nube (sin comprar nada):** el equipo se conecta por MQTT seguro a un broker gratis ([HiveMQ Cloud](https://console.hivemq.cloud), plan Serverless) y el mismo panel, publicado en una web, habla con él por ahí. En Ajustes > Nube cargás la URL del cluster, usuario y contraseña; después abrís el panel en la nube y entrás con esos datos y el ID del equipo. Desde la nube no se puede cambiar el WiFi ni la conexión a la nube (para no dejar el equipo inaccesible).
- **Panel web completo por VPN:** con una VPN hacia tu casa. Lo más simple es [Tailscale](https://tailscale.com) en una compu o Raspberry que quede prendida en tu casa, configurada como *subnet router*, y la app de Tailscale en el celular: entrás a la IP del equipo como si estuvieras en tu WiFi. Si tu router trae VPN (WireGuard/OpenVPN) sirve igual.
- No conviene abrir un puerto del router hacia el ESP32: la página va sin cifrar. Si igual lo hacés, poné contraseña en Ajustes.

## API

| Método | Ruta | Cuerpo |
|---|---|---|
| GET | `/api/status` | — lecturas por carpa y por sensor, hora, WiFi y estado de relés |
| POST | `/api/relay` | `{"id":0,"mode":"on"\|"off"\|"auto"}` o `{"id":4,"pulse":30}` (prender N segundos) |
| GET | `/api/history?range=day\|week\|month&tent=0\|1` | — día: cada 5 min; semana: por hora; mes: por día (por carpa) |
| GET/POST | `/api/config` | ver `GET` para el formato; en `POST` se mandan solo los campos a cambiar |
| GET | `/api/wifi/scan` | — (llamar hasta que devuelva `networks`) |
| POST | `/api/wifi` | `{"ssid":"...","pass":"..."}` |
| POST | `/api/wifi/forget` | — |
| POST | `/api/wifi/offline` | — (no usar WiFi de casa: red propia siempre prendida) |
| POST | `/api/time` | `{"epoch":1730000000}` (UTC) |
| POST | `/api/restart` | — |

## Estado

- [x] Hardware y pines
- [x] Firmware base: portal WiFi, SHT + VPD, relés, config persistente, NTP + DS3231
- [x] Automatizaciones (horario de luces, riego, coolers por temperatura/humedad, asignables)
- [x] Panel web por secciones con gráfico de 24 h
