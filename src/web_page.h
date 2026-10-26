#pragma once
#include <Arduino.h>

// Página básica de control y configuración. El panel completo (gráficos, historial)
// viene en el paso 4.
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Cultivo</title>
<style>
:root{--bg:#10140f;--card:#1a2118;--fg:#e6ecdf;--mut:#8a9a84;--acc:#7bc950;--off:#3a4436;--err:#e5694d}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:760px;margin:auto;padding:16px}h1{font-size:1.3rem;margin:4px 0 12px}h2{font-size:1rem;margin:0 0 10px;color:var(--mut)}
.card{background:var(--card);border-radius:12px;padding:14px;margin-bottom:14px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}
.big{font-size:1.8rem;font-weight:600}.mut{color:var(--mut);font-size:.85rem}
.relay{display:flex;align-items:center;justify-content:space-between;gap:8px;padding:8px 0;border-top:1px solid #263022}
.relay:first-child{border-top:0}.dot{width:12px;height:12px;border-radius:50%;background:var(--off);display:inline-block;margin-right:8px}
.dot.on{background:var(--acc);box-shadow:0 0 8px var(--acc)}
.seg{display:flex;border-radius:8px;overflow:hidden}.seg button{border:0;padding:8px 12px;background:var(--off);color:var(--fg);cursor:pointer}
.seg button.sel{background:var(--acc);color:#10140f;font-weight:600}
input,select,button.b{background:#0e120d;color:var(--fg);border:1px solid #334030;border-radius:8px;padding:8px;font:inherit}
button.b{background:var(--acc);color:#10140f;border:0;font-weight:600;cursor:pointer}
.row{display:flex;gap:8px;flex-wrap:wrap;margin:6px 0}.row>*{flex:1;min-width:120px}
table{width:100%;border-collapse:collapse}td{padding:4px}.err{color:var(--err)}
</style></head><body><main>
<h1>🌱 Cultivo</h1>
<div class="card"><div class="grid">
<div><div class="mut">Temperatura</div><div class="big" id="t">--</div></div>
<div><div class="mut">Humedad</div><div class="big" id="h">--</div></div>
<div><div class="mut">VPD (hoja)</div><div class="big" id="v">--</div></div>
<div><div class="mut">Hora</div><div class="big" id="clk" style="font-size:1.2rem">--</div><div class="mut" id="clks"></div></div>
</div><div class="mut" id="sens"></div></div>

<div class="card"><h2>Relés</h2><div id="relays"></div>
<div class="mut">Auto = lo manejan las automatizaciones (próximamente).</div></div>

<div class="card"><h2>WiFi</h2><div id="wst" class="mut"></div>
<div class="row"><select id="ssid"><option value="">Elegí una red…</option></select><button class="b" onclick="scan()" style="flex:0">Buscar</button></div>
<div class="row"><input id="ssidm" placeholder="o escribí el nombre de la red"><input id="pass" type="password" placeholder="Clave"></div>
<div class="row"><button class="b" onclick="saveWifi()">Conectar</button><button class="b" style="background:var(--off);color:var(--fg)" onclick="forget()">Olvidar red</button></div>
<div id="wmsg" class="mut"></div></div>

<div class="card"><h2>Configuración</h2>
<table id="cfgr"></table>
<div class="row"><label>Offset hoja (°C)<br><input id="leaf" type="number" step="0.1"></label>
<label>Relés activos en bajo<br><select id="alow"><option value="1">Sí</option><option value="0">No</option></select></label></div>
<div class="row"><label>Clave AP (vacía = abierta)<br><input id="appw"></label><label>Zona horaria (POSIX)<br><input id="tz"></label></div>
<div class="row"><button class="b" onclick="saveCfg()">Guardar</button><button class="b" style="background:var(--off);color:var(--fg)" onclick="syncTime()">Hora del celular</button></div>
<div id="cmsg" class="mut"></div></div>
</main><script>
const $=id=>document.getElementById(id);
const TYPES={generic:'Genérico',light:'Luz',fan_in:'Intractor',fan_out:'Extractor',irrigation:'Riego',humidifier:'Humidificador',dehumidifier:'Deshumidificador',heater:'Calefactor'};
const MODES=[['off','Off'],['auto','Auto'],['on','On']];
async function api(p,b){const r=await fetch(p,b===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});const j=await r.json();if(!r.ok)throw new Error(j.error||r.status);return j}
function render(s){
 const c=s.climate;$('t').textContent=c.valid?c.temperature+' °C':'--';$('h').textContent=c.valid?c.humidity+' %':'--';$('v').textContent=c.valid?c.vpd+' kPa':'--';
 $('sens').innerHTML='Sensor: '+c.sensor+(c.valid?' · punto de rocío '+c.dewPoint+' °C':' <span class="err">sin lectura</span>');
 $('clk').textContent=s.time.valid?s.time.local:'sin hora';$('clks').textContent=(s.time.ntp?'NTP':'')+(s.time.rtc?' · RTC':' · sin RTC');
 $('relays').innerHTML=s.relays.map((r,i)=>`<div class="relay"><div><span class="dot ${r.on?'on':''}"></span>${r.name} <span class="mut">${TYPES[r.type]}</span></div><div class="seg">${MODES.map(([m,l])=>`<button class="${r.mode==m?'sel':''}" onclick="setMode(${i},'${m}')">${l}</button>`).join('')}</div></div>`).join('');
 const w=s.wifi;$('wst').textContent=w.connected?`Conectado a ${w.ssid} · IP ${w.ip} · ${w.rssi} dBm`:(w.ssid?`Sin conexión a ${w.ssid}`:'Sin red configurada')+(w.ap?` · AP "${w.apSsid}" activo`:'');
}
async function refresh(){try{render(await api('/api/status'))}catch(e){}}
async function setMode(i,m){render(await api('/api/relay',{id:i,mode:m}))}
async function scan(){$('wmsg').textContent='Buscando redes…';for(let k=0;k<20;k++){const r=await api('/api/wifi/scan');if(r.networks){$('ssid').innerHTML='<option value="">Elegí una red…</option>'+r.networks.sort((a,b)=>b.rssi-a.rssi).map(n=>`<option>${n.ssid.replace(/</g,'&lt;')}</option>`).join('');$('wmsg').textContent=r.networks.length+' redes';return}await new Promise(r=>setTimeout(r,1000))}$('wmsg').textContent='No se pudo escanear'}
async function saveWifi(){const ssid=$('ssidm').value||$('ssid').value;if(!ssid)return;await api('/api/wifi',{ssid,pass:$('pass').value});$('wmsg').textContent='Conectando… Si funciona, el equipo queda en http://cultivo.local (o la IP que muestre acá).'}
async function forget(){if(confirm('¿Borrar la red guardada y reiniciar?'))await api('/api/wifi/forget',{})}
async function loadCfg(){const c=await api('/api/config');$('leaf').value=c.leafOffset;$('alow').value=c.relayActiveLow?1:0;$('appw').value=c.apPassword;$('tz').value=c.tz;
 $('cfgr').innerHTML=c.relays.map((r,i)=>`<tr><td>${i+1}</td><td><input id="n${i}" maxlength="23" value="${r.name.replace(/"/g,'&quot;')}"></td><td><select id="y${i}">${Object.entries(TYPES).map(([k,l])=>`<option value="${k}" ${k==r.type?'selected':''}>${l}</option>`).join('')}</select></td></tr>`).join('')}
async function saveCfg(){const relays=[...Array(8).keys()].map(i=>({name:$('n'+i).value,type:$('y'+i).value}));
 try{await api('/api/config',{relays,leafOffset:parseFloat($('leaf').value),relayActiveLow:$('alow').value=='1',apPassword:$('appw').value,tz:$('tz').value});$('cmsg').textContent='Guardado';refresh()}catch(e){$('cmsg').innerHTML='<span class="err">'+e.message+'</span>'}}
async function syncTime(){render(await api('/api/time',{epoch:Math.floor(Date.now()/1000)}));$('cmsg').textContent='Hora actualizada'}
refresh();loadCfg();setInterval(refresh,3000);
</script></body></html>)HTML";
