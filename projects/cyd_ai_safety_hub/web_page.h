// Trang web nhúng trong firmware: bảng số liệu, chat với AI, cài đặt.
#pragma once
#include <Arduino.h>

static const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD AI Safety Hub</title>
<style>
:root{--bg:#0f1420;--card:#1a2233;--tx:#e8edf5;--dim:#8b96a8;--ok:#28c85a;--warn:#fac81e;--dan:#ff7814;--emg:#e61e1e;--acc:#3c8cff}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--tx)}
header{padding:12px 16px;background:var(--card);display:flex;justify-content:space-between;align-items:center}
h1{font-size:18px;margin:0}nav{display:flex;gap:6px;padding:8px 16px}nav button{flex:1}
button{background:var(--acc);color:#fff;border:0;border-radius:8px;padding:10px;font-size:15px;cursor:pointer}
button.sec{background:#33405a}button.red{background:var(--emg)}
main{padding:0 16px 24px;max-width:760px;margin:auto}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}
.card{background:var(--card);border-radius:12px;padding:12px;margin-top:10px}.lbl{color:var(--dim);font-size:13px}.val{font-size:26px;font-weight:700;margin-top:4px}
#lvl{font-size:22px;font-weight:800;padding:14px;border-radius:12px;text-align:center;margin-top:10px}
#chat{height:52vh;overflow-y:auto;display:flex;flex-direction:column;gap:8px}
.msg{padding:10px 12px;border-radius:12px;max-width:88%;white-space:pre-wrap;line-height:1.4}.me{align-self:flex-end;background:var(--acc)}.bot{align-self:flex-start;background:#26314a}
.row{display:flex;gap:8px;margin-top:10px}input,select,textarea{width:100%;padding:10px;border-radius:8px;border:1px solid #33405a;background:#0b0f18;color:var(--tx);font-size:15px}
label{display:block;margin-top:10px;font-size:14px;color:var(--dim)}.hid{display:none!important}svg{width:100%;height:120px}
small{color:var(--dim)}
</style></head><body>
<header><h1>🌿 CYD AI Safety Hub</h1><small id="clock"></small></header>
<nav><button onclick="tab('dash')">Số liệu</button><button onclick="tab('chat')">Trò chuyện AI</button><button onclick="tab('set')">Cài đặt</button></nav>
<main>
<section id="dash">
 <div id="lvl">...</div>
 <div class="row"><button class="sec" onclick="silence(10)">🔕 Tắt còi 10 phút</button><button class="sec" onclick="silence(0)">🔔 Bật lại còi</button></div>
 <div class="card"><div class="lbl">Không khí trong phòng</div><div class="val" id="air">--</div>
  <div class="grid"><div><div class="lbl">CO₂</div><div class="val" id="co2">--</div></div><div><div class="lbl">Nhiệt độ</div><div class="val" id="rt">--</div></div><div><div class="lbl">Độ ẩm</div><div class="val" id="hu">--</div></div></div>
  <div class="lbl" style="margin-top:8px">CO₂ 3 giờ qua (đường vàng = 1000 ppm)</div><svg id="g" viewBox="0 0 180 100" preserveAspectRatio="none"></svg></div>
 <div class="card"><div class="lbl">Khu vực sạc xe</div><div class="grid">
  <div><div class="lbl">Nhiệt cục sạc</div><div class="val" id="ct">--</div></div><div><div class="lbl">Tốc độ tăng</div><div class="val" id="rr">--</div></div>
  <div><div class="lbl">Khói / khí</div><div class="val" id="sm">--</div></div><div><div class="lbl">Điện áp</div><div class="val" id="vv">--</div></div>
  <div><div class="lbl">Dòng sạc</div><div class="val" id="ia">--</div></div><div><div class="lbl">Phiên sạc</div><div class="val" id="cs">--</div></div></div></div>
 <div class="card"><small id="sens"></small></div>
</section>
<section id="chat" class="hid card"></section>
<div id="chatbar" class="row hid"><input id="q" placeholder="Hỏi bất cứ điều gì..." onkeydown="if(event.key=='Enter')ask()"><button onclick="ask()">Gửi</button></div>
<section id="set" class="hid">
 <div class="card"><b>AI (Claude)</b>
  <label>Anthropic API key <small id="kst"></small></label><input id="akey" type="password" placeholder="sk-ant-... (để trống = giữ nguyên)">
  <label>Model</label><select id="model"><option>claude-opus-5-5</option><option>claude-sonnet-5-5</option><option>claude-haiku-5-5</option></select>
  <label>Mức suy nghĩ (effort)</label><select id="effort"><option>low</option><option>medium</option><option>high</option></select>
  <label><input type="checkbox" id="ws" style="width:auto"> Cho phép tìm kiếm web</label></div>
 <div class="card"><b>Telegram</b>
  <label>Bot token <small id="tst"></small></label><input id="tok" type="password" placeholder="123456:ABC... (để trống = giữ nguyên)">
  <small>Đã ghép đôi: <span id="chats"></span> chat. Mã PIN ghép đôi = mật khẩu trang web.</small>
  <div class="row"><button class="sec" onclick="post('/api/unpair',{}).then(load)">Hủy ghép đôi tất cả</button></div></div>
 <div class="card"><b>Ngưỡng cảnh báo</b><div class="grid">
  <div><label>Nhiệt theo dõi °C</label><input id="t_warn" type="number" step="1"></div><div><label>Nhiệt nguy cơ °C</label><input id="t_danger" type="number" step="1"></div>
  <div><label>Nhiệt khẩn cấp °C</label><input id="t_emerg" type="number" step="1"></div><div><label>Tăng nhanh °C/phút</label><input id="rise" type="number" step="0.5"></div>
  <div><label>Khói theo dõi %</label><input id="s_warn" type="number" step="1"></div><div><label>Khói nguy cơ %</label><input id="s_danger" type="number" step="1"></div>
  <div><label>Dòng tối đa A (0=tắt)</label><input id="i_max" type="number" step="0.5"></div><div><label>Áp tối đa V (0=tắt)</label><input id="v_max" type="number" step="0.5"></div>
  <div><label>Báo CO₂ ppm</label><input id="co2a" type="number" step="50"></div><div><label>Điện trở shunt Ω</label><input id="shunt" type="number" step="0.001"></div></div></div>
 <div class="card"><b>Ổ cắm thông minh (tùy chọn)</b>
  <label>URL bật (vd Tasmota: http://192.168.1.50/cm?cmnd=Power%20On)</label><input id="pon">
  <label>URL ngắt (vd http://192.168.1.50/cm?cmnd=Power%20Off)</label><input id="poff">
  <label><input type="checkbox" id="acut" style="width:auto"> Tự động ngắt khi KHẨN CẤP</label></div>
 <div class="row"><button onclick="save()">💾 Lưu cài đặt</button></div>
 <div class="row"><button class="sec" onclick="if(confirm('Khởi động lại?'))post('/api/reboot',{})">Khởi động lại</button>
  <button class="red" onclick="if(confirm('Xóa Wi-Fi và vào chế độ cài đặt?'))post('/api/wifi_reset',{})">Cài lại Wi-Fi</button></div>
 <p><small id="ver"></small></p>
</section>
</main>
<script>
const $=id=>document.getElementById(id);const LV=['AN TOÀN','THEO DÕI','NGUY CƠ','KHẨN CẤP'];const LC=['var(--ok)','var(--warn)','var(--dan)','var(--emg)'];
function tab(t){for(const s of['dash','chat','set'])$(s).classList.toggle('hid',s!=t);$('chatbar').classList.toggle('hid',t!='chat');if(t=='set')load()}
function f(v,d,u){return v==null?'--':v.toFixed(d)+u}
function post(u,b){return fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)}).then(r=>r.json())}
function silence(m){post('/api/silence',{minutes:m}).then(poll)}
function graph(a){let p='',x=0;const n=a.length;a.forEach((v,i)=>{if(v<0){x=0;return}const X=(180-n+i),Y=100-Math.min(100,Math.max(0,(v-400)/16));p+=(x?'L':'M')+X+' '+Y+' ';x=1});
 const y1=100-(1000-400)/16;$('g').innerHTML='<rect width="180" height="100" fill="#0b0f18"/><line x1="0" x2="180" y1="'+y1+'" y2="'+y1+'" stroke="#fac81e" stroke-dasharray="3 3"/><path d="'+p+'" fill="none" stroke="#3c8cff" stroke-width="1.5"/>'}
function poll(){fetch('/api/state').then(r=>r.json()).then(s=>{const l=$('lvl');l.textContent='Sạc xe: '+LV[s.level]+(s.reasons?' – '+s.reasons:'')+(s.silenced?' (còi tắt)':'');l.style.background=LC[s.level];l.style.color='#000';
 $('air').textContent=s.air;$('co2').textContent=f(s.co2,0,' ppm');$('rt').textContent=f(s.room_t,1,'°C');$('hu').textContent=f(s.hum,0,'%');
 $('ct').textContent=f(s.charger_t,1,'°C');$('rr').textContent=f(s.rise,1,'°C/p');$('sm').textContent=s.smoke_state||f(s.smoke,0,'%');$('vv').textContent=f(s.v,1,' V');$('ia').textContent=f(s.a,2,' A');
 $('cs').textContent=s.charging?(Math.round(s.charge_min)+' phút, '+Math.round(s.wh)+' Wh'):'Không sạc';$('clock').textContent=s.time||'';$('sens').textContent=s.sensors;graph(s.hist)}).catch(()=>{})}
function add(t,c){const d=document.createElement('div');d.className='msg '+c;d.textContent=t;$('chat').appendChild(d);$('chat').scrollTop=1e9;return d}
function ask(){const q=$('q').value.trim();if(!q)return;$('q').value='';add(q,'me');const d=add('Đang suy nghĩ...','bot');
 post('/api/chat',{text:q}).then(r=>{if(!r.id){d.textContent=r.error||'Lỗi';return}const t=setInterval(()=>fetch('/api/chat?id='+r.id).then(x=>x.json()).then(x=>{if(x.reply)d.textContent=x.reply;if(x.done)clearInterval(t)}),1500)})}
function load(){fetch('/api/settings').then(r=>r.json()).then(s=>{$('kst').textContent=s.akey_set?'(đã nhập)':'(chưa có)';$('tst').textContent=s.tok_set?'(đã nhập)':'(chưa có)';
 $('model').value=s.model;$('effort').value=s.effort;$('ws').checked=s.web_search;$('chats').textContent=s.chats;
 for(const k of['t_warn','t_danger','t_emerg','rise','s_warn','s_danger','i_max','v_max','co2a','shunt'])$(k).value=s[k];$('pon').value=s.plug_on;$('poff').value=s.plug_off;$('acut').checked=s.auto_cut;$('ver').textContent=s.version})}
function save(){const b={model:$('model').value,effort:$('effort').value,web_search:$('ws').checked,plug_on:$('pon').value,plug_off:$('poff').value,auto_cut:$('acut').checked};
 if($('akey').value)b.akey=$('akey').value;if($('tok').value)b.tok=$('tok').value;
 for(const k of['t_warn','t_danger','t_emerg','rise','s_warn','s_danger','i_max','v_max','co2a','shunt'])b[k]=parseFloat($(k).value);
 post('/api/settings',b).then(r=>{alert(r.ok?'Đã lưu':'Lỗi: '+r.error);$('akey').value='';$('tok').value='';load()})}
poll();setInterval(poll,2000);
</script></body></html>)HTML";
