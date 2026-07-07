/*
  web_page.h
  ----------
  Pagina unica do site de configuracao, embutida na flash (PROGMEM).
  Sem dependencia externa: CSS e JS inline. Os olhinhos do topo sao os
  mesmos do OLED, recriados em CSS (retangulos arredondados que piscam).

  A pagina conversa com o firmware por 3 rotas:
    GET  /status  -> JSON com estado atual (preenche a tela)
    GET  /scan    -> JSON com redes proximas (botao "buscar redes")
    POST /wifi  e  POST /config -> formularios de salvar
*/
#pragma once

#include <Arduino.h>

const char WEB_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LittleTosh</title>
<style>
  :root { --bg:#0b0d14; --card:#151926; --ink:#e8ecf6; --dim:#8a93ab; --line:#252b3f; }
  * { box-sizing:border-box; margin:0; padding:0; }
  body { background:var(--bg); color:var(--ink); font:16px/1.5 system-ui,Segoe UI,Roboto,sans-serif;
         max-width:520px; margin:0 auto; padding:24px 16px 48px; }
  .eyes { display:flex; gap:10px; justify-content:center; margin:18px 0 6px; }
  .eye  { width:44px; height:44px; background:#fff; border-radius:12px;
          animation:blink 4s infinite; box-shadow:0 0 18px #ffffff22; }
  @keyframes blink { 0%,92%,100%{transform:scaleY(1)} 95%{transform:scaleY(.08)} }
  h1 { text-align:center; font-size:22px; letter-spacing:.5px; margin-bottom:2px; }
  .sub { text-align:center; color:var(--dim); font-size:13px; margin-bottom:22px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:14px;
          padding:18px; margin-bottom:16px; }
  .card h2 { font-size:15px; margin-bottom:12px; color:var(--ink); }
  label { display:block; font-size:13px; color:var(--dim); margin:10px 0 4px; }
  input, select { width:100%; padding:10px 12px; border-radius:9px; border:1px solid var(--line);
          background:#0e1220; color:var(--ink); font-size:15px; }
  input:focus, select:focus { outline:none; border-color:#5b6bff; }
  .row { display:flex; gap:10px; }
  .row > div { flex:1; }
  button { width:100%; margin-top:14px; padding:11px; border:none; border-radius:9px;
           background:#5b6bff; color:#fff; font-size:15px; font-weight:600; cursor:pointer; }
  button.ghost { background:transparent; border:1px solid var(--line); color:var(--dim);
                 margin-top:8px; font-weight:400; }
  button:active { transform:translateY(1px); }
  #status div { display:flex; justify-content:space-between; padding:5px 0;
                border-bottom:1px dashed var(--line); font-size:14px; }
  #status div:last-child { border-bottom:none; }
  #status span:first-child { color:var(--dim); }
  #nets { margin-top:8px; }
  .net { padding:9px 12px; border:1px solid var(--line); border-radius:9px; margin-top:6px;
         font-size:14px; cursor:pointer; display:flex; justify-content:space-between; }
  .net:hover { border-color:#5b6bff; }
  .net small { color:var(--dim); }
  .msg { text-align:center; color:var(--dim); font-size:13px; margin-top:8px; }
</style>
</head>
<body>
  <div class="eyes"><div class="eye"></div><div class="eye"></div></div>
  <h1>LittleTosh</h1>
  <p class="sub">painel de configura&ccedil;&atilde;o</p>

  <div class="card">
    <h2>Estado</h2>
    <div id="status"><div><span>carregando...</span><span></span></div></div>
  </div>

  <div class="card">
    <h2>Rede WiFi</h2>
    <form method="POST" action="/wifi">
      <label>Nome da rede (SSID)</label>
      <input name="ssid" id="ssid" maxlength="32" required placeholder="MinhaRede">
      <label>Senha</label>
      <input name="pass" id="pass" type="password" maxlength="64" placeholder="(vazio se rede aberta)">
      <button type="submit">Salvar e reiniciar</button>
    </form>
    <button class="ghost" onclick="scan()">Buscar redes pr&oacute;ximas</button>
    <div id="nets"></div>
    <p class="msg">Depois de salvar, ele reinicia e tenta conectar nessa rede.</p>
  </div>

  <div class="card">
    <h2>Sono</h2>
    <form method="POST" action="/config">
      <div class="row">
        <div><label>Dorme &agrave;s</label>
          <input name="sleepHour" id="sleepHour" type="number" min="0" max="23" value="23"></div>
        <div><label>Acorda &agrave;s</label>
          <input name="wakeHour" id="wakeHour" type="number" min="0" max="23" value="7"></div>
      </div>
      <div class="row">
        <div><label>Fica acordado por (min)</label>
          <input name="holdMinutes" id="holdMinutes" type="number" min="1" max="60" value="3"></div>
        <div><label>Fuso (horas de UTC)</label>
          <input name="utcOffsetH" id="utcOffsetH" type="number" min="-12" max="14" value="-3"></div>
      </div>
      <button type="submit">Salvar</button>
    </form>
    <p class="msg">Se mexerem nele durante a noite, ele acorda e volta a dormir depois desse tempo.</p>
  </div>

<script>
function esc(s){const d=document.createElement('div');d.textContent=s;return d.innerHTML;}

fetch('/status').then(r=>r.json()).then(s=>{
  const rows=[["Modo",s.mode],["Rede",s.ssid||"-"],["IP",s.ip],["Sinal",s.rssi?s.rssi+" dBm":"-"],["Hora",s.time]];
  document.getElementById('status').innerHTML =
    rows.map(r=>`<div><span>${r[0]}</span><span>${esc(String(r[1]))}</span></div>`).join('');
  if(s.savedSsid) document.getElementById('ssid').value=s.savedSsid;
  sleepHour.value=s.sleepHour; wakeHour.value=s.wakeHour;
  holdMinutes.value=s.holdMinutes; utcOffsetH.value=Math.round(s.utcOffsetMin/60);
}).catch(()=>{});

function scan(){
  const box=document.getElementById('nets');
  box.innerHTML='<p class="msg">buscando...</p>';
  poll();
  function poll(){
    fetch('/scan').then(r=>r.json()).then(j=>{
      if(j.status==='scanning'){setTimeout(poll,1000);return;}
      if(!j.length){box.innerHTML='<p class="msg">nenhuma rede encontrada</p>';return;}
      box.innerHTML=j.map(n=>
        `<div class="net" onclick="pick('${esc(n.ssid).replace(/'/g,"\\'")}')">`+
        `<span>${esc(n.ssid)}${n.sec?" &#128274;":""}</span><small>${n.rssi} dBm</small></div>`).join('');
    }).catch(()=>{box.innerHTML='<p class="msg">erro ao buscar</p>';});
  }
}
function pick(ssid){
  document.getElementById('ssid').value=ssid;
  document.getElementById('pass').focus();
}
</script>
</body>
</html>)rawliteral";

// Resposta minima apos salvar o WiFi (o chip reinicia logo em seguida).
const char WEB_PAGE_REBOOT[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LittleTosh</title>
<style>body{background:#0b0d14;color:#e8ecf6;font:16px system-ui;display:flex;
flex-direction:column;align-items:center;justify-content:center;height:100vh;gap:12px}
.eyes{display:flex;gap:10px}.eye{width:44px;height:44px;background:#fff;border-radius:12px;
animation:blink 1.2s infinite}@keyframes blink{0%,80%,100%{transform:scaleY(1)}90%{transform:scaleY(.08)}}
p{color:#8a93ab;max-width:300px;text-align:center}</style></head>
<body><div class="eyes"><div class="eye"></div><div class="eye"></div></div>
<b>Salvo! Reiniciando...</b>
<p>Vou tentar conectar na rede que voc&ecirc; configurou. Se n&atilde;o der certo,
o AP "LittleTosh" abre de novo.</p></body></html>)rawliteral";
