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

La configuración (nombres, tipos y modo de cada relé, offset de hoja para el VPD, clave del AP, zona horaria) queda guardada en la memoria flash y sobrevive a cortes de luz. Los relés arrancan siempre apagados hasta leer la config.

## API

| Método | Ruta | Cuerpo |
|---|---|---|
| GET | `/api/status` | — lecturas, hora, WiFi y estado de relés |
| POST | `/api/relay` | `{"id":0,"mode":"on"\|"off"\|"auto"}` |
| GET/POST | `/api/config` | ver `GET` para el formato; en `POST` se mandan solo los campos a cambiar |
| GET | `/api/wifi/scan` | — (llamar hasta que devuelva `networks`) |
| POST | `/api/wifi` | `{"ssid":"...","pass":"..."}` |
| POST | `/api/wifi/forget` | — |
| POST | `/api/time` | `{"epoch":1730000000}` (UTC) |
| POST | `/api/restart` | — |

## Estado

- [x] Hardware y pines
- [x] Firmware base: portal WiFi, SHT + VPD, relés, config persistente, NTP + DS3231
- [ ] Automatizaciones (horario de luces, riego, coolers por VPD/temperatura, asignables)
- [ ] Panel web completo con gráficos e historial
