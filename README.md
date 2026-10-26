# Cultivo-esp32

Controlador de carpa de cultivo con ESP32: 8 relés (luces, extractor/intractor, riego y 2 asignables), sensor SHT40/SHT31 de temperatura y humedad con cálculo de VPD, reloj DS3231 de respaldo y panel web.

- Hardware, lista de compras y pines: [docs/hardware.md](docs/hardware.md)

## Compilar y cargar

Con [PlatformIO](https://platformio.org/) (VS Code + extensión PlatformIO, o CLI):

```
pio run -t upload
pio device monitor
```

## Primer uso

1. Al encender sin WiFi configurado, el ESP32 crea la red **`Cultivo-XXXX`** (clave `cultivo123`).
2. Conectate desde el celular: se abre el portal (si no, entrá a `http://192.168.4.1`).
3. En **WiFi** tocá *Buscar*, elegí tu red, poné la clave y *Conectar*.
4. Ya en tu red, entrá a `http://cultivo.local` o a la IP que muestra el portal. La red `Cultivo-XXXX` se apaga sola a los 2 minutos.

Si el WiFi falla, la red `Cultivo-XXXX` vuelve a aparecer para reconfigurarlo. Manteniendo el botón **BOOT** 5 segundos se borra la red guardada.

## Panel web

- **Inicio:** temperatura, humedad y VPD contra los objetivos, día/noche, gráfico de las últimas 24 h y control rápido de cada relé (Off / Auto / On).
- **Iluminación:** horario por cada relé de luz: 12/12, 16/8, 18/6, 20/4 o personalizado (horas prendida/apagada libres, por ej. 13/13) y hora de encendido.
- **Temperatura y humedad:** rangos de día y de noche (con presets por etapa), histéresis y renovación de aire. Extractor e intractor prenden si hace calor, si hay humedad de más o en la renovación; las salidas asignables pueden ser humidificador, deshumidificador o calefactor.
- **Riego:** hasta 8 riegos por salida (hora + segundos), asistente que los reparte en el horario de luz, “Regar ahora” y una guía rápida.
- **Historial:** día (cada 5 min), semana (por hora) y mes (por día) con temperatura, humedad y VPD (promedio y mín–máx), más horas de luz, coolers y tiempo de riego por día. Semana y mes se guardan en la flash y sobreviven cortes de luz (35 días).
- **Ajustes:** WiFi, nombres y función de cada relé, polaridad del módulo, offset de hoja, clave del AP y zona horaria.

En modo **Auto** cada relé sigue su automatización; On/Off lo fuerza a mano. Si el sensor falla, los coolers quedan prendidos y humidificador/deshumidificador/calefactor apagados. Sin hora válida, luces y riego en Auto quedan apagados.

La página está en `web/index.html`; al compilar, `tools/embed_web.py` la comprime y la mete en el firmware.

La configuración (nombres, tipos y modo de cada relé, offset de hoja para el VPD, clave del AP, zona horaria) queda guardada en la memoria flash y sobrevive a cortes de luz. Los relés arrancan siempre apagados hasta leer la config.

## Acceso desde cualquier lado

- **Telegram (recomendado):** en Ajustes > Telegram cargás el token de un bot creado con @BotFather y tu chat id. Desde cualquier lado: `/estado`, `/luces on|off|auto`, `/rele N on|off|auto`, `/regar N SEG`. Avisa si la temperatura o la humedad se van de rango más de 10 min, si falla el sensor o si el equipo se reinició (corte de luz). No hace falta tocar el router.
- **Panel web completo:** con una VPN hacia tu casa. Lo más simple es [Tailscale](https://tailscale.com) en una compu o Raspberry que quede prendida en tu casa, configurada como *subnet router*, y la app de Tailscale en el celular: entrás a la IP del equipo como si estuvieras en tu WiFi. Si tu router trae VPN (WireGuard/OpenVPN) sirve igual.
- No conviene abrir un puerto del router hacia el ESP32: la página va sin cifrar. Si igual lo hacés, poné contraseña en Ajustes.

## API

| Método | Ruta | Cuerpo |
|---|---|---|
| GET | `/api/status` | — lecturas, hora, WiFi y estado de relés |
| POST | `/api/relay` | `{"id":0,"mode":"on"\|"off"\|"auto"}` o `{"id":4,"pulse":30}` (prender N segundos) |
| GET | `/api/history?range=day\|week\|month` | — día: cada 5 min; semana: por hora; mes: por día |
| GET/POST | `/api/config` | ver `GET` para el formato; en `POST` se mandan solo los campos a cambiar |
| GET | `/api/wifi/scan` | — (llamar hasta que devuelva `networks`) |
| POST | `/api/wifi` | `{"ssid":"...","pass":"..."}` |
| POST | `/api/wifi/forget` | — |
| POST | `/api/time` | `{"epoch":1730000000}` (UTC) |
| POST | `/api/restart` | — |

## Estado

- [x] Hardware y pines
- [x] Firmware base: portal WiFi, SHT + VPD, relés, config persistente, NTP + DS3231
- [x] Automatizaciones (horario de luces, riego, coolers por temperatura/humedad, asignables)
- [x] Panel web por secciones con gráfico de 24 h
