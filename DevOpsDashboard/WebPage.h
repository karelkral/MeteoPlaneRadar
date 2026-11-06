// =============================================================================
//  DevOpsDashboard
//  The configuration page, as one PROGMEM string.
//
//  No CDN, no build step: the device has to be configurable from its own
//  access point, where there is no internet at all. In access-point mode only
//  the WiFi section is shown.
// =============================================================================
#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="dark">
<title>DevOps Dashboard</title><style>
*{box-sizing:border-box}
body{background:#111;color:#eee;font:15px system-ui,sans-serif;margin:0 auto;padding:16px;max-width:560px}
h1{font-size:20px;margin:0}
h2{font-size:16px;margin:0 0 10px;color:#5bf}
.sub{color:#888;font-size:13px;margin:2px 0 14px}
section{background:#1a1a1a;border:1px solid #2a2a2a;border-radius:10px;padding:14px;margin-bottom:14px}
label{display:block;color:#aaa;font-size:13px;margin:10px 0 4px}
input,select{width:100%;padding:10px;background:#111;border:1px solid #333;border-radius:6px;color:#eee;font-size:15px}
input[type=range]{padding:0}
button{padding:10px 14px;margin:10px 6px 0 0;font-size:15px;border:0;border-radius:6px;background:#2d7;color:#000;font-weight:600;cursor:pointer}
button.alt{background:#333;color:#eee}
button.danger{background:#c33;color:#fff}
.row{display:flex;gap:8px}.row>*{flex:1}
table{width:100%;border-collapse:collapse;font-size:13px}
td{padding:4px 0;border-bottom:1px solid #222;vertical-align:top}
td:first-child{color:#888;width:35%}
#msg{position:sticky;top:0;padding:8px 12px;border-radius:6px;margin-bottom:10px;display:none}
#msg.ok{display:block;background:#143;color:#8f8}
#msg.err{display:block;background:#411;color:#f88}
a{color:#5bf}
.hint{color:#777;font-size:12px;margin-top:4px}
</style></head><body>
<h1>DevOps Dashboard</h1>
<div class="sub" id="sub">&nbsp;</div>
<div id="msg"></div>

<section id="secWifi">
<h2>WiFi</h2>
<button class="alt" id="bScan">Scan networks</button>
<label for="ssid">Network</label>
<select id="ssid"><option value="">- scan first or type below -</option></select>
<input id="ssidManual" placeholder="or type the network name">
<label for="wpass">Password</label>
<input id="wpass" type="password" autocomplete="new-password">
<button id="bConnect">Connect</button>
<div id="slots" style="display:none">
<label>Remembered networks (tried in order of signal strength)</label>
<div id="slotRows"></div>
<button class="alt" id="bSlots">Save networks</button>
<div class="hint">Leave a password empty to keep the stored one.</div>
</div>
</section>

<section id="secApi" style="display:none">
<h2>DevOps API</h2>
<label for="apiUrl">URL (http:// or https://, empty = off)</label>
<input id="apiUrl" placeholder="http://192.168.1.10:5000/api/dashboard">
<label for="apiInt">Refresh interval [s]</label>
<input id="apiInt" type="number" min="5" max="3600">
<button id="bApi">Save</button>
</section>

<section id="secDisp" style="display:none">
<h2>Display</h2>
<label for="bri">Brightness <span id="briVal"></span></label>
<input id="bri" type="range" min="0" max="100">
<div class="row">
<button class="alt" data-step="-1">&#9664; Previous</button>
<button class="alt" data-step="1">Next &#9654;</button>
</div>
</section>

<section id="secSys" style="display:none">
<h2>System</h2>
<table id="st"></table>
<p><a href="/update">Firmware update</a></p>
<label for="curPass">Current admin password</label>
<input id="curPass" type="password" autocomplete="current-password">
<label for="newPass">New admin password (a single space removes it)</label>
<input id="newPass" type="password" autocomplete="new-password">
<button class="alt" id="bPass">Change password</button>
<div>
<button class="alt" id="bReboot">Reboot</button>
<button class="danger" id="bReset">Factory reset</button>
</div>
</section>

<script>
const $=id=>document.getElementById(id);
let CFG={};
function msg(t,ok){const m=$("msg");m.textContent=t;m.className=ok?"ok":"err";clearTimeout(msg.t);msg.t=setTimeout(()=>m.className="",5000);}
async function api(path,body){
 const o=body===undefined?{}:{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(body)};
 const r=await fetch(path,o);let j={};try{j=await r.json();}catch(e){}
 if(!r.ok)throw new Error(j.error||r.status);return j;
}
function slotRow(i,w){
 const d=document.createElement("div");d.className="row";
 const s=document.createElement("input");s.id="slS"+i;s.placeholder="SSID "+(i+1);s.value=w.ssid||"";
 const p=document.createElement("input");p.id="slP"+i;p.type="password";p.placeholder=w.hasPass?"(stored)":"password";
 d.append(s,p);return d;
}
async function load(){
 CFG=await api("/api/config");
 $("sub").textContent="v"+CFG.version+"  |  "+CFG.ip+(CFG.apMode?"  |  setup mode":"");
 if(CFG.apMode)return;
 for(const id of["secApi","secDisp","secSys"])$(id).style.display="";
 $("slots").style.display="";
 $("apiUrl").value=CFG.apiUrl;$("apiInt").value=CFG.apiIntervalSec;
 $("apiInt").min=CFG.apiIntervalMin;$("apiInt").max=CFG.apiIntervalMax;
 $("bri").value=CFG.brightness;$("briVal").textContent=CFG.brightness+" %";
 const rows=$("slotRows");rows.textContent="";
 CFG.wifi.forEach((w,i)=>rows.append(slotRow(i,w)));
 status();setInterval(status,5000);
}
async function status(){
 try{
  const s=await api("/api/status");const t=$("st");t.textContent="";
  const L=[["Board",s.board],["IP",s.ip],["WiFi",s.ssid+" ("+s.rssi+" dBm)"],["Time",s.time||"-"],
   ["Clock",s.ntp],["API",s.api],["Screen",s.screen],["Uptime",s.uptime],
   ["Free heap",s.heap+" B"],["Free PSRAM",s.psram+" B"],["Last reset",s.resetReason]];
  for(const[k,v]of L){const r=t.insertRow();r.insertCell().textContent=k;r.insertCell().textContent=v;}
 }catch(e){}
}
$("bScan").onclick=async()=>{
 $("bScan").textContent="Scanning...";
 try{
  const l=await api("/api/scan");const s=$("ssid");s.textContent="";
  const o0=document.createElement("option");o0.value="";o0.textContent="- pick a network -";s.append(o0);
  const seen=new Set();
  l.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{if(!n.ssid||seen.has(n.ssid))return;seen.add(n.ssid);
   const o=document.createElement("option");o.value=n.ssid;o.textContent=n.ssid+"  ("+n.rssi+" dBm)";s.append(o);});
 }catch(e){msg("Scan failed: "+e.message);}
 $("bScan").textContent="Scan networks";
};
$("bConnect").onclick=async()=>{
 const ssid=$("ssidManual").value.trim()||$("ssid").value;
 if(!ssid){msg("Pick or type a network first");return;}
 try{await api("/api/wifi",{ssid:ssid,pass:$("wpass").value});
  msg("Connecting to "+ssid+"... If it works, the device leaves setup mode. Then open http://devopsdashboard.local/",true);
 }catch(e){msg("Failed: "+e.message);}
};
$("bSlots").onclick=async()=>{
 const nets=CFG.wifi.map((w,i)=>({ssid:$("slS"+i).value.trim(),pass:$("slP"+i).value}));
 try{await api("/api/wifilist",{nets:nets});msg("Networks saved",true);load();}catch(e){msg("Failed: "+e.message);}
};
$("bApi").onclick=async()=>{
 try{await api("/api/config",{apiUrl:$("apiUrl").value.trim(),apiIntervalSec:parseInt($("apiInt").value)||30});
  msg("API settings saved",true);
 }catch(e){msg(e.message=="apiUrl"?"The URL has to start with http:// or https://":"Failed: "+e.message);}
};
let briT;
$("bri").oninput=()=>{const v=parseInt($("bri").value);$("briVal").textContent=v+" %";
 clearTimeout(briT);briT=setTimeout(()=>api("/api/config",{brightness:v}).catch(e=>msg("Failed: "+e.message)),250);};
document.querySelectorAll("[data-step]").forEach(b=>b.onclick=()=>
 api("/api/screen",{step:parseInt(b.dataset.step)}).then(status).catch(e=>msg("Failed: "+e.message)));
$("bPass").onclick=async()=>{
 const np=$("newPass").value;if(!np){msg("Type a new password");return;}
 try{await api("/api/config",{password:$("curPass").value,newPassword:np});
  msg(np==" "?"Password removed":"Password changed",true);$("newPass").value="";$("curPass").value="";
 }catch(e){msg(e.message=="password"?"Wrong current password":"Failed: "+e.message);}
};
$("bReboot").onclick=async()=>{if(!confirm("Reboot the device?"))return;
 try{await api("/api/reboot",{});msg("Rebooting...",true);}catch(e){msg("Failed: "+e.message);}};
$("bReset").onclick=async()=>{if(!confirm("Erase all settings including WiFi?"))return;
 try{await api("/api/reset",{password:$("curPass").value});msg("Settings erased, rebooting into setup mode",true);}
 catch(e){msg(e.message=="password"?"Type the current admin password first":"Failed: "+e.message);}};
load().catch(e=>msg("Cannot load settings: "+e.message));
</script></body></html>)rawliteral";
