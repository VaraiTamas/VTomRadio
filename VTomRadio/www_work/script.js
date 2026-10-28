var hostname = window.location.hostname;
var modesd = false;
var modedlna = false;
var playlistTimer = null;
var currentSource = 'web';
const query = window.location.search;
const params = new URLSearchParams(query);
const yoTitle = 'VTom Radio';
let audiopreview=null;
if(params.size>0){
  if(params.has('host')) hostname=params.get('host');
}
var websocket;
var wsQueue = [];
var wserrcnt = 0;
var wstimeout;
var loaded = false;
var currentItem = 0;
var independentEncodersAvailable = true;

function wsSend(message) {
  if (!message) return;
  if (websocket && websocket.readyState === WebSocket.OPEN) {
    websocket.send(message);
    return;
  }
  wsQueue.push(message);
}

function flushWsQueue() {
  if (!websocket || websocket.readyState !== WebSocket.OPEN) return;
  while (wsQueue.length > 0) {
    websocket.send(wsQueue.shift());
  }
}

window.addEventListener('load', onLoad);

function loadCSS(href){ const link = document.createElement("link"); link.rel = "stylesheet"; link.href = href; document.head.appendChild(link); }
function loadJS(src, callback){ const script = document.createElement("script"); script.src = src; script.type = "text/javascript"; script.async = true; script.onload = callback; document.head.appendChild(script); }

function initWebSocket() {
  clearTimeout(wstimeout);
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(`ws://${hostname}/ws`);
  websocket.onopen    = onOpen;
  websocket.onclose   = onClose;
  websocket.onmessage = onMessage;
}

// Render initial UI without waiting for WebSocket handshake.
function onLoad(event) {
  if (!loaded) {
    const initialMode = (typeof playMode === 'undefined' || !playMode) ? 'ap' : playMode;
    continueLoading(initialMode); // playMode comes from variables.js
    loaded = true;
  }
  // Fail-safe: never leave the UI blocked by spinner forever.
  setTimeout(() => {
    const p = getId('progress');
    const c = getId('content');
    if (p && !p.classList.contains('hidden')) {
      hideSpinner();
      if (c && (!c.innerHTML || c.innerHTML.trim() === '')) {
        c.innerHTML = '<div style="padding:20px;text-align:center;">Page load timeout. Open <a href="/webboard">/webboard</a>.</div>';
      }
    }
  }, 8000);
  initWebSocket();
}

function onOpen(event) {
  console.log('Connection opened');
  if (!loaded) {
    const initialMode = (typeof playMode === 'undefined' || !playMode) ? 'ap' : playMode;
    continueLoading(initialMode); //playMode in variables.js
    loaded = true;
  }
  wserrcnt=0;
  flushWsQueue();
}
function onClose(event) {
  wserrcnt++;
  wstimeout=setTimeout(initWebSocket, wserrcnt<10?2000:120000);
}
function secondToTime(seconds){
  if(seconds>=3600){
    return new Date(seconds * 1000).toISOString().substring(11, 19);
  }else{
    return new Date(seconds * 1000).toISOString().substring(14, 19);
  }
}
function showById(show,hide){
  show.forEach(item=>{ getId(item).classList.remove('hidden'); });
  hide.forEach(item=>{ getId(item).classList.add('hidden'); });
}
function onMessage(event) {
  if (window.location.pathname === '/dlna.html') { //DLNA mod
    return;
  }
  try{
    const data = JSON.parse(escapeData(event.data));
    /*ir*/
    if(typeof data.ircode !== 'undefined'){
      getId('protocol').innerText=data.protocol;
      classEach('irrecordvalue', function(el){ if(el.hasClass("active")) el.innerText='0x'+data.ircode.toString(16).toUpperCase(); });
      return;
    }
    if(typeof data.irvals !== 'undefined'){
      classEach('irrecordvalue', function(el,i){ var val = data.irvals[i]; if(val>0) el.innerText='0x'+val.toString(16).toUpperCase(); else el.innerText=""; });
      return;
    }
    /*end ir*/
    if(typeof data.redirect !== 'undefined'){
      getId("mdnsnamerow").innerHTML=`<h3 style="line-height: 37px;color: #aaa; margin: 0 auto;">redirecting to ${data.redirect}</h3>`;
      setTimeout(function(){ window.location.href=data.redirect; }, 4000);
      return;
    }
    if(typeof data.playermode !== 'undefined') { //Web, SD, DLNA
      const prevSource = currentSource;
      modesd = data.playermode=='modesd';
      modedlna = data.playermode=='modedlna';
      setSourceButtons(data.playermode);
      if(modesd || modedlna) showById(['bitinfo'],['snuffle']);
      else showById(['bitinfo'],['timelinesvg','soundsvg','snuffle']);
      getId('toggleplaylist').classList.toggle('hidden', !(modesd || modedlna));
      getId('editplaylist').classList.toggle('hidden', modesd);
      showById(['volslider'],['sdslider']);
      updateToggleIcon();
      getId('editplaylist').classList.remove('active');
      getId('pleditorwrap').classList.add('hidden');
      getId('plsvg').classList.remove('hidden');
      getId('editbacksvg').classList.add('hidden');
      // Repeated playermode messages arrive in bursts; load the playlist once after a source change.
      if(prevSource!=currentSource){
        if(playlistTimer) clearTimeout(playlistTimer);
        playlistTimer = setTimeout(function(){
          playlistTimer = null;
          generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime());
        }, 250);
      }
      // Add hiding elements with modeweb class in SD mode
      classEach('modeweb', function(el){ el.classList.toggle('hidden', modesd); });
      getId('searchradio').classList.toggle('hidden', modesd || modedlna);
      return;
    }
    if(typeof data.sdinit !== 'undefined') {
      getId('src-sd').classList.toggle('hidden', data.sdinit!=1);
      if(data.sdinit==1) {
        getId('playernav').classList.add("sd");
        getId('volmbutton').classList.add("hidden");
      }else{
        getId('playernav').classList.remove("sd");
        getId('volmbutton').classList.remove("hidden");
      }
    }
    if(typeof data.sdpos !== 'undefined' && getId("sdpos")){
      if(data.sdtpos==0 && data.sdtend==0){
        getId("sdposvalscurrent").innerHTML="00:00";
        getId("sdposvalsend").innerHTML="00:00";
        getId("sdpos").value = data.sdpos;
        fillSlider(getId("sdpos"));
      }else{
        getId("sdposvalscurrent").innerHTML=secondToTime(data.sdtpos);
        getId("sdposvalsend").innerHTML=secondToTime(data.sdtend);
        getId("sdpos").value = data.sdpos;
        fillSlider(getId("sdpos"));
      }
      return;
    }
    if(typeof data.sdmin !== 'undefined' && getId("sdpos")){
      getId("sdpos").attr('min',data.sdmin); 
      getId("sdpos").attr('max',data.sdmax); 
      return;
    }
    if(typeof data.snuffle!== 'undefined'){
      if(data.snuffle==1){
        getId("snuffle").classList.add("active");
      }else{
        getId("snuffle").classList.remove("active");
      }
      return;
    }
    if(typeof data.payload !== 'undefined'){
      data.payload.forEach(item=> {
        setupElement(item.id, item.value);
      });
    }else{
      if(typeof data.current !== 'undefined') { setCurrentItem(data.current); return; }
      if(typeof data.file !== 'undefined') { generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime()); wsSend('submitplaylistdone=1'); return; }
      if(typeof data.act !== 'undefined'){ data.act.forEach(showclass=> { classEach(showclass, function(el) { el.classList.remove("hidden"); }); }); return; }
      Object.keys(data).forEach(key=>{
        setupElement(key, data[key]);
      });
    }
  }catch(e){
    console.log("ws.onMessage error:", event.data);
  }
}
function escapeData(data){
  let m=data.match(/{.+?:\s"(.+?)"}/);
  if(m!==null){
    let m1 = m[1];
    if(m1.indexOf('"') !== -1){
      let mq=m1.replace(/["]/g, '\\\"');
      return data.replace(m1,mq);
    }
  }
  return data;
}
function getId(id,patent=document){
  return patent.getElementById(id);
}
function classEach(classname, callback) {
  document.querySelectorAll(`.${classname}`).forEach((item, index) => callback(item, index));
}
function quoteattr(s) {
  return ('' + s)
    .replace(/&/g, '&amp;')
    .replace(/'/g, '&apos;')
    .replace(/"/g, '&quot;')
    .replace(/</g, '&lt;');
}
HTMLElement.prototype.attr = function(name, value=null){
  if(value!==null){
    return this.setAttribute(name, value);
  }else{
    return this.getAttribute(name);
  }
}
HTMLElement.prototype.hasClass = function(className){
  return this.classList.contains(className);
}
function fillSlider(sl){
  const slaveid = sl.dataset.slaveid;
  const value = (sl.value-sl.min)/(sl.max-sl.min)*100;
  if(slaveid) getId(slaveid).innerText=sl.value;
  sl.style.background = 'linear-gradient(to right, var(--accent-dark) 0%,  var(--accent-dark) ' + value + '%, var(--odd-bg-color) ' + value + '%, var(--odd-bg-color) 100%)';
}
function setupElement(id,value){
  if(id==='encindavail'){
    independentEncodersAvailable = (value === true || value === 1 || value === "1" || value === "true");
    updateIndependentEncodersUi(true);
    return;
  }

  const element = getId(id);
  if(element){
    if(element.classList.contains("checkbox")){
      element.classList.remove("checked");
      const checked = (value === true || value === 1 || value === "1" || value === "true");
      if(checked) element.classList.add("checked");
    }
    if(element.classList.contains("classchange")){
      element.attr("class", "classchange");
      element.classList.add(value);
    }
    if(element.classList.contains("text")){
      element.innerText=value;
    }
    if(element.type==='text' || element.type==='number' || element.type==='password' || element.type==='time'){
      element.value=value;
    }
    if(element.tagName==='SELECT'){
      element.value=String(value);
    }
    if(element.type==='range'){
      element.value=value;
      fillSlider(element);
    }
  }
  if(id==='clockfont' || id==='clockfontmono') {
    updateClockFontMonoUi(false);
  }
  if(id==='encind') {
    updateIndependentEncodersUi(false);
  }
}

function updateClockFontMonoUi(forceSync){
  const fontSel = getId('clockfont');
  const monoToggle = getId('clockfontmono');
  if(!fontSel || !monoToggle) return;

  const isDigi7 = String(fontSel.value) === '0';
  monoToggle.classList.toggle('disabled', !isDigi7);
  monoToggle.attr('aria-disabled', isDigi7 ? 'false' : 'true');

  if(!isDigi7 && monoToggle.classList.contains('checked')){
    monoToggle.classList.remove('checked');
    if(forceSync){
      wsSend('clockfontmono=0');
    }
  }
}

function updateIndependentEncodersUi(forceSync){
  const independentToggle = getId('encind');
  if(!independentToggle) return;

  independentToggle.classList.toggle('disabled', !independentEncodersAvailable);
  independentToggle.attr('aria-disabled', independentEncodersAvailable ? 'false' : 'true');

  if(!independentEncodersAvailable && independentToggle.classList.contains('checked')){
    independentToggle.classList.remove('checked');
    if(forceSync){
      wsSend('encodersindependent=0');
    }
  }
}
/***--- playlist ---***/
function setCurrentItem(item){
  currentItem=item;
  const playlist = getId("playlist");
  if(!playlist) return;
  let topPos = 0, lih = 0;
  playlist.querySelectorAll('li').forEach((item, index)=>{ item.attr('class','play'); if(index+1==currentItem){ item.classList.add("active"); topPos = item.offsetTop; lih = item.offsetHeight; } });
  playlist.scrollTo({ top: (topPos-playlist.offsetHeight/2+lih/2), left: 0, behavior: 'smooth' });
}
function initPLEditor(){
  ple= getId('pleditorcontent');
  if(!ple) return;
  let html='';
  ple.innerHTML="";
  pllines = getId('playlist').querySelectorAll('li');
  pllines.forEach((item,index)=>{
    html+=`<li class="pleitem" id="${'plitem'+index}"><span class="grabbable" draggable="true">${("00"+(index+1)).slice(-3)}</span>
      <span class="pleinput plecheck"><input type="checkbox" class="plcb" /></span>
      <input class="pleinput plename" type="text" value="${quoteattr(item.dataset.name)}" maxlength="140" />
      <input class="pleinput pleurl" type="text" value="${item.dataset.url}" maxlength="140" />
      <span class="pleinput pleplay" data-command="preview">&#9658;</span>
      <input class="pleinput pleovol" type="number" min="-64" max="64" step="1" value="${item.dataset.ovol}" />
      <input class="pleinput pleovol" type="text" value="${quoteattr(item.dataset.genre||'')}" maxlength="64" placeholder="genre" />
      </li>`;
  });
  ple.innerHTML=html;
}
function handlePlaylistData(fileData) {
  const ul = getId('playlist');
  ul.innerHTML='';
  if (!fileData) return;
  const lines = fileData.split('\n');
  let li='', html='';
  for(var i = 0;i < lines.length;i++){
    let line = lines[i].split('\t');
    if(line.length==3 || line.length==4){
      const active=(i+1==currentItem)?' class="active"':'';
      const genre = (line.length==4?line[3].trim():"");
      li=`<li${active} attr-id="${i+1}" class="play" data-name="${line[0].trim()}" data-url="${line[1].trim()}" data-ovol="${line[2].trim()}" data-genre="${genre}"><span class="text">${line[0].trim()}</span><span class="count">${i+1}</span></li>`;
      html += li;
    }
  }
  ul.innerHTML=html;
  setCurrentItem(currentItem);
  if(!modesd) initPLEditor();
  buildGenreIndexFromCSV(fileData);
  annotatePlaylistWithGenres();
  filterPlaylist();
}
let __genreIndexByItem = {};
let __genreList = [];
function buildGenreIndexFromCSV(fileData){
  __genreIndexByItem = {};
  const set = new Set();
  if(!fileData) return;
  const lines = fileData.split('\n');
  let itemIdx = 0;  
  for(let i=0;i<lines.length;i++){
    const raw = lines[i].replace(/\r$/, '');
    if(raw.trim()==='') continue;
    const cols = raw.split('\t');
    const name = (cols[0]||'').trim();
    const isGroup = name.startsWith('***') && name.endsWith('***');
    if(isGroup) continue; 
    itemIdx++; 
    const genre = (cols[3]||'').trim();
    __genreIndexByItem[itemIdx] = genre;
    if(genre) set.add(genre);
  }
  __genreList = Array.from(set).sort((a,b)=>a.localeCompare(b));
}
function alignPlaylistBg(){
  const pl = getId('playlist');
  if(!pl) return;
  const lis = pl.children;
  let last = null, k = 0;
  for(let i = lis.length - 1; i >= 0; i--){
    if(lis[i].style.display !== 'none'){ last = lis[i]; k = i + 1; break; }
  }
  const bottom = last ? last.offsetTop + last.offsetHeight : 0;
  // Az üres rész csíkozása közvetlenül az utolsó sor után, a sor paritásának megfelelően folytatódik
  pl.style.backgroundSize = '2px 80px';
  pl.style.backgroundPositionY = (bottom + (k % 2 ? 40 : 0)) + 'px';
}
function filterPlaylist() {
  const customInput = getId('custominput');
  const playlist = getId('playlist');
  if (!playlist || !customInput) return;
  const searchTerm = customInput.value.toLowerCase();
  const items = playlist.querySelectorAll('li');
  items.forEach(item => {
    const name = item.textContent.toLowerCase();
    item.style.display = name.includes(searchTerm) ? '' : 'none';
  });
  alignPlaylistBg();
}
function annotatePlaylistWithGenres(){
  const nodes = document.querySelectorAll('#playlist [attr-id]');
  nodes.forEach((el)=>{
    const idStr = el.getAttribute('attr-id');
    const idx = parseInt(idStr,10);
    if(!isNaN(idx)){
      const g = __genreIndexByItem[idx] || '';
      el.dataset.genre = g;
    }
  });
}
function generatePlaylist(path){
  getId('playlist').innerHTML='<div id="progress"><span id="loader"></span></div>';
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        handlePlaylistData(xhr.responseText);
      } else {
        handlePlaylistData(null);
      }
    }
  };
  xhr.open("GET", path);
  xhr.send(null);
}
function plAdd(){
  let ple=getId('pleditorcontent');
  let plitem = document.createElement('li');
  let cnt=ple.getElementsByTagName('li');
  plitem.attr('class', 'pleitem');
  plitem.attr('id', 'plitem'+(cnt.length));
  plitem.innerHTML = '<span class="grabbable" draggable="true">'+("00"+(cnt.length+1)).slice(-3)+'</span>\
      <span class="pleinput plecheck"><input type="checkbox" /></span>\
      <input class="pleinput plename" type="text" value="" maxlength="140" />\
      <input class="pleinput pleurl" type="text" value="" maxlength="140" />\
      <span class="pleinput pleplay" data-command="preview">&#9658;</span>\
      <input class="pleinput pleovol" type="number" min="-30" max="30" step="1" value="0" />\
      <input class="pleinput pleovol" type="text" value="" maxlength="64" placeholder="genre" />';
  ple.appendChild(plitem);
  ple.scrollTo({
    top: ple.scrollHeight,
    left: 0,
    behavior: 'smooth'
  });
}
function plRemove(){
  let items=getId('pleditorcontent').getElementsByTagName('li');
  let pass=[];
  for (let i = 0; i <= items.length - 1; i++) {
    if(items[i].getElementsByTagName('span')[1].getElementsByTagName('input')[0].checked) {
      pass.push(items[i]);
    }
  }
  if(pass.length==0) {
    alert('Choose something first');
    return;
  }
  for (var i = 0; i < pass.length; i++)
  {
    pass[i].remove();
  }
  items=getId('pleditorcontent').getElementsByTagName('li');
  for (let i = 0; i <= items.length-1; i++) {
    items[i].getElementsByTagName('span')[0].innerText=("00"+(i+1)).slice(-3);
  }
}
function submitPlaylist(){
  var items=getId("pleditorcontent").getElementsByTagName("li");
  var output="";
  for (var i = 0; i <= items.length - 1; i++) {
    inputs=items[i].getElementsByTagName("input");
    // inputs: 0 checkbox, 1 name, 2 url, 3 ovol, 4 genre
    if(inputs[1].value == "" || inputs[2].value == "") continue;
    let ovol = inputs[3].value;
    if(ovol < -30) ovol = -30;
    if(ovol > 30) ovol = 30;
    let genre = inputs[4].value || '';
    output+=inputs[1].value+"\t"+inputs[2].value+"\t"+ovol+"\t"+genre+"\n";
  }
  let file = new File([output], "tempplaylist.csv",{type:"text/plain;charset=utf-8", lastModified:new Date().getTime()});
  let container = new DataTransfer();
  container.items.add(file);
  let fileuploadinput=getId("file-upload");
  fileuploadinput.files = container.files;
  doPlUpload(fileuploadinput);
  toggleTarget(0, 'pleditorwrap');
}
function doPlUpload(finput) {
  wsSend("submitplaylist=1");
  var formData = new FormData();
  formData.append("plfile", finput.files[0]);
  var xhr = new XMLHttpRequest();
  xhr.open("POST",`http://${hostname}/upload`,true);
  xhr.send(formData);
  finput.value = '';
}
function updateToggleIcon(){
  if(!(modesd || modedlna)) return;
  const timelineVisible = !getId('sdslider').classList.contains('hidden');
  // The button shows the view it switches to
  getId('timelinesvg').classList.toggle('hidden', timelineVisible);
  getId('soundsvg').classList.toggle('hidden', !timelineVisible);
}
function toggleTarget(el, id){
  const target = getId(id);
  if(id=='pleditorwrap'){
    audiopreview.pause();
    audiopreview.src='';
    getId('previewinfo').innerHTML='';
  }
  if(id=='sliderswap'){
    getId('sdslider').classList.toggle('hidden');
    getId('volslider').classList.toggle('hidden');
    getId('bitinfo').classList.toggle('hidden');
    if(modesd) getId('snuffle').classList.toggle('hidden');
    updateToggleIcon();
    return;
  }
  if(target){
    target.classList.toggle("hidden");
    if(id!='pleditorwrap') getId(target.dataset.target).classList.toggle("active");
    if(id=='pleditorwrap'){
      const editing = !target.classList.contains('hidden');
      getId('plsvg').classList.toggle('hidden', editing);
      getId('editbacksvg').classList.toggle('hidden', !editing);
      if(modedlna) getId('toggleplaylist').classList.toggle('hidden', editing);
    }
  }
}
function checkboxClick(cb, command){
  if(cb.classList.contains('disabled')) return;
  cb.classList.toggle("checked");
  wsSend(`${command}=${cb.classList.contains("checked")?1:0}`);
}
function sliderInput(sl, command){
  wsSend(`${command}=${sl.value}`);
  fillSlider(sl);
}
function handleWiFiData(fileData) {
  if (!fileData) return;
  var lines = fileData.split('\n');
  for(var i = 0;i < lines.length;i++){
    let line = lines[i].split('\t');
    if(line[0].trim() !== "" && getId("ssid"+i)){
      getId("ssid"+i).value=line[0].trim();
      const passEl=getId("pass"+i);
      if(passEl){
        passEl.value=line.length>=2 ? line[1].trim() : "";
      }
    }
  }
}
function getWiFi(path){
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        handleWiFiData(xhr.responseText);
      } else {
        handleWiFiData(null);
      }
    }
  };
  xhr.open("GET", path);
  xhr.send(null);
}
function initWiFiSettingsView(){
  const results=getId('wifi-scan-results');
  if(!results) return;
  const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
  const fetchWifiScan=async()=>{
    for(let attempt=0;attempt<8;attempt++){
      const response=await fetch('/wifiscan',{cache:'no-store'});
      const running=response.headers.get('X-WiFi-Scan-Running')==='1';
      const networks=await response.json();
      if(Array.isArray(networks)&&networks.length>0) return networks;
      if(!running) return Array.isArray(networks)?networks:[];
      await wait(350);
    }
    return [];
  };
  document.querySelectorAll('.wifi-scan-button').forEach(button=>{
    button.addEventListener('click',async()=>{
      const ssidInput=getId(button.dataset.ssidTarget);
      if(!ssidInput) return;
      const credentialRow=button.closest('.wifi-credential');
      if(credentialRow) {
        credentialRow.after(results);
        results.scrollIntoView({behavior:'smooth',block:'nearest'});
      }
      button.classList.add('hl');
      results.classList.remove('hidden');
      results.innerHTML='<div class="wifi-scan-result">Searching...</div>';
      try{
        const networks=await fetchWifiScan();
        results.innerHTML='';
        if(!networks.length){
          results.innerHTML='<div class="wifi-scan-result">No networks found</div>';
          return;
        }
        networks.forEach(network=>{
          const item=document.createElement('div');
          item.className='wifi-scan-result';
          const name=document.createElement('span');
          name.textContent=network.ssid||'';
          const signal=document.createElement('span');
          signal.className='wifi-scan-rssi';
          signal.textContent=`${network.rssi} dBm`;
          item.append(name,signal);
          item.addEventListener('click',()=>{
            ssidInput.value=network.ssid||'';
            results.classList.add('hidden');
          });
          results.appendChild(item);
        });
      }catch(error){
        results.innerHTML='<div class="wifi-scan-result">Scan failed</div>';
      }finally{
        button.classList.remove('hl');
      }
    });
  });
  document.querySelectorAll('.wifi-eye-button').forEach(button=>{
    button.addEventListener('click',()=>{
      const passInput=getId(button.dataset.passTarget);
      if(!passInput) return;
      if(passInput.type==='password'){
        passInput.type='text';
        button.classList.add('hl');
        button.title='Hide password';
        button.innerHTML='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.49 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"/><line x1="1" y1="1" x2="23" y2="23"/></svg>';
      }else{
        passInput.type='password';
        button.classList.remove('hl');
        button.title='Show password';
        button.innerHTML='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>';
      }
    });
  });
}
function applyTZ(){
  wsSend("tzh="+getId("tzh").value);
  wsSend("tzm="+getId("tzm").value);
  wsSend("sntp2="+getId("sntp2").value);
  wsSend("sntp1="+getId("sntp1").value);
}
function exportSettings(){
  const rows=[];
  const processedCmds=new Set();
  document.querySelectorAll('#settingscontent [data-command]').forEach(el=>{
    if(el.classList.contains('local')) return;
    const cmd=el.dataset.command;
    if(processedCmds.has(cmd)) return;
    processedCmds.add(cmd);
    if(el.tagName==='INPUT'||el.tagName==='SELECT'){
      rows.push([cmd,el.value]);
    }else if(el.classList.contains('checkbox')){
      rows.push([cmd,el.classList.contains('checked')?'1':'0']);
    }
  });
  const specialFields=[
    {id:'tzh',cmd:'tzh'},{id:'tzm',cmd:'tzm'},
    {id:'sntp1',cmd:'sntp1'},{id:'sntp2',cmd:'sntp2'},
    {id:'wlat',cmd:'lat'},{id:'wlon',cmd:'lon'},{id:'wkey',cmd:'key'},
    {id:'dateformat',cmd:'dateformat'},{id:'mdns',cmd:'mdnsname'}
  ];
  specialFields.forEach(s=>{
    const el=getId(s.id);
    if(el&&el.value) rows.push([s.cmd,el.value]);
  });
  const csv=rows.map(r=>r[0]+','+r[1]).join('\n');
  const blob=new Blob([csv],{type:'text/csv'});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob);
  const stamp = new Date().toISOString().replace(/[:T]/g, "-").slice(0, 19);
  a.download='VTomRadio_settings_'+stamp+'.csv';
  a.click();
  URL.revokeObjectURL(a.href);
}
function importSettings(file){
  const specialCmdToId={
    'tzh':'tzh','tzm':'tzm','sntp1':'sntp1','sntp2':'sntp2',
    'lat':'wlat','lon':'wlon','key':'wkey',
    'dateformat':'dateformat','mdnsname':'mdns'
  };
  const reader=new FileReader();
  reader.onload=function(e){
    const lines=e.target.result.split('\n').filter(line=>line.trim());
    let index=0;
    const processLine=()=>{
      if(index>=lines.length) return;
      const line=lines[index++];
      const comma=line.indexOf(',');
      if(comma<0){
        processLine();
        return;
      }
      const cmd=line.substring(0,comma).trim();
      const val=line.substring(comma+1).trim();
      if(!cmd){
        processLine();
        return;
      }
      const elByCmd=document.querySelector('[data-command="'+cmd+'"]');
      if(elByCmd){
        if(elByCmd.tagName==='INPUT'||elByCmd.tagName==='SELECT'){
          elByCmd.value=val;
          if(elByCmd.type==='range') fillSlider(elByCmd);
        }else if(elByCmd.classList.contains('checkbox')){
          elByCmd.classList.toggle('checked',val==='1');
        }
      }else if(specialCmdToId[cmd]){
        const el=getId(specialCmdToId[cmd]);
        if(el) el.value=val;
      }
      wsSend(cmd+'='+val);
      setTimeout(processLine,50);
    };
    processLine();
  };
  reader.readAsText(file);
}
function applyDateFormat(){
  const sel = getId("dateformat");
  if(!sel) return;
  wsSend("dateformat=" + parseInt(sel.value, 10));
  wsSend("gettimezone=1");
}
function rebootSystem(info){
  getId("settingscontent").innerHTML=`<h2>${info}</h2>`;
  getId("settingsdone").classList.add("hidden");
  getId("navigation").classList.add("hidden");
  setTimeout(function(){ window.location.href=`http://${hostname}/`; }, 5000);
}
function submitWiFi(){
  var output="";
  var items=document.getElementsByClassName("wifi-credential");
  for (var i = 0; i <= items.length - 1; i++) {
    const inputs=items[i].getElementsByTagName("input");
    if(inputs[0].value == "") continue;
    const ps=inputs[1].value;
    output+=inputs[0].value+"\t"+ps+"\n";
  }
  if(output!=""){ // Well, let's say, quack.
    let file = new File([output], "tempwifi.csv",{type:"text/plain;charset=utf-8", lastModified:new Date().getTime()});
    let container = new DataTransfer();
    container.items.add(file);
    let fileuploadinput=getId("file-upload");
    fileuploadinput.files = container.files;
    var formData = new FormData();
    formData.append("wifile", fileuploadinput.files[0]);
    var xhr = new XMLHttpRequest();
    xhr.open("POST","/upload",true);
    xhr.timeout = 15000;
    xhr.onload = function(){
      if(xhr.status >= 200 && xhr.status < 300){
        getId("settingscontent").innerHTML="<h2>Settings saved. Rebooting...</h2>";
        getId("settingsdone").classList.add("hidden");
        getId("navigation").classList.add("hidden");
        setTimeout(function(){ window.location.href=`http://${hostname}/`; }, 10000);
      }else{
        getId("settingscontent").innerHTML="<h2>Save failed. Please try again.</h2>";
      }
    };
    xhr.onerror = function(){
      getId("settingscontent").innerHTML="<h2>Network error while saving Wi-Fi. Please try again.</h2>";
    };
    xhr.ontimeout = function(){
      getId("settingscontent").innerHTML="<h2>Save timeout. Check AP connection and retry.</h2>";
    };
    xhr.send(formData);
    fileuploadinput.value = '';
  }else{
    getId("settingscontent").innerHTML="<h2>Please enter at least one SSID.</h2>";
  }
}
function playItem(target){
  const item = target.attr('attr-id');
  setCurrentItem(item)
  wsSend(`play=${item}`);
}
function hideSpinner(){
  getId("progress").classList.add("hidden");
  getId("content").classList.remove("hidden");
}
function setSourceButtons(mode){
  currentSource = mode=='modesd' ? 'sd' : (mode=='modedlna' ? 'dlna' : 'web');
  ['web','sd','dlna'].forEach(s=>{
    const b = getId('src-'+s);
    if(b) b.classList.toggle('active', s==currentSource);
  });
}
async function switchSource(src){
  const dlnaOk = typeof dlnaSupported !== 'undefined' && dlnaSupported;
  if(src=='sd'){
    if(currentSource!='sd') wsSend("newmode=1");
  }else if(src=='dlna'){
    if(!dlnaOk) return;
    if(currentSource=='dlna'){ window.location.href='dlna.html'; return; }
    let exists = false;
    try{
      const r = await fetch(`/data/playlist_dlna.csv?${Date.now()}`, {cache:'no-store'});
      exists = r.ok && (await r.text()).trim().length > 0;
    }catch(e){}
    if(exists) fetch('/playlist/dlna', {cache:'no-store'});
    else window.location.href='dlna.html';
  }else if(src=='web'){
    if(currentSource=='web') return;
    if(dlnaOk) fetch('/playlist/web', {cache:'no-store'});
    else wsSend("newmode=0");
  }
}
function toggleSnuffle(){
  let el = getId('snuffle');
  el.classList.toggle('active');
  wsSend("snuffle="+el.classList.contains('active'));
}
function previewInfo(text, url='', error=false){
  const previewinfo=getId('previewinfo');
  previewinfo.classList.remove('error');
  if(url!='') previewinfo.innerHTML=`${text} <a href="${url}" target="_blank">${url}</a>`;
  else previewinfo.innerHTML=`${text}`;
  if(error) previewinfo.classList.add('error');
}
const PREVIEW_TIMEOUT = 3000;
function playPreview(root) {
  const streamUrl=root.getElementsByClassName('pleurl')[0].value;
  if(root.hasClass('active')){ root.classList.remove('active'); audiopreview.pause(); previewInfo('Stop playback:', streamUrl); return; }
  classEach('pleitem', function(el){ el.classList.remove('active') });
  if(streamUrl=='' || !audiopreview) { previewInfo("No streams available.", '', true); return; }
  previewInfo('Attempting to play:', streamUrl);
  audiopreview.src = streamUrl;
  audiopreview.load();
  let isTimeout = false;
  const timeout = setTimeout(() => { isTimeout = true; previewInfo("Connection timeout", streamUrl, true); root.classList.remove('active'); audiopreview.pause(); audiopreview.src = ''; return; }, PREVIEW_TIMEOUT);
  const onCanPlay = () => { if (!isTimeout) { clearTimeout(timeout); previewInfo('Playback', streamUrl); root.classList.add('active'); audiopreview.play().catch(err => { previewInfo("Playback error:", streamUrl, true); root.classList.remove('active'); return; }); }  };
  const onError = () => { if (!isTimeout) { clearTimeout(timeout); root.classList.remove('active'); previewInfo("Error loading stream:", streamUrl, true); audiopreview.src = ''; return; } };
  audiopreview.addEventListener("canplay", onCanPlay, { once: true });
  audiopreview.addEventListener("error", onError, { once: true });
}
function continueLoading(mode){
  if(loaded) return;
  if(typeof mode === 'undefined' || !mode) mode = 'ap';
  const pathname = window.location.pathname;
  if(mode=="player" || pathname=='/wifiset'){
    if(['/','/index.html'].includes(pathname)){
      document.title = `${yoTitle} - Player`;
      fetch(`player.html?${fwVersion}`).then(response => response.text()).then(player => { 
        getId('content').classList.add('idx');
        getId('content').innerHTML = player; 
        fetch('logo.svg').then(response => response.text()).then(svg => { 
          getId('logo').innerHTML = svg;
          hideSpinner();
          audiopreview = getId('audiopreview');
          generatePlaylist(`http://${hostname}/data/playlist.csv` + "?" + new Date().getTime());
          populateBankSelect();
		  //DLNA mod
			const dlnaBtn = document.getElementById('src-dlna');
			if (dlnaBtn && typeof dlnaSupported !== 'undefined' && !dlnaSupported) {
			  dlnaBtn.classList.add('hidden');
			}
        });
        getId("version").innerText=` | v${fwVersion}`;
        document.querySelectorAll('input[type="range"]').forEach(sl => { fillSlider(sl); });
        wsSend('getindex=1');
        //generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime());
      });
    }
    if(pathname=='/settings.html'){
      document.title = `${yoTitle} - Settings`;
      fetch(`options.html?${fwVersion}`).then(response => response.text()).then(options => {
        getId('content').innerHTML = options; 
        fetch('logo.svg').then(response => response.text()).then(svg => { 
          getId('logo').innerHTML = svg;
          hideSpinner();
        });
        getId("version").innerText=` | v${fwVersion}`;
        document.querySelectorAll('input[type="range"]').forEach(sl => { fillSlider(sl); });
        wsSend('getsystem=1');
        wsSend('getscreen=1');
        wsSend('gettimezone=1');
        wsSend('getweather=1');
        wsSend('getcontrols=1');
        getWiFi(`http://${hostname}/data/wifi.csv`+"?"+new Date().getTime());
        wsSend('getactive=1');
        const dateApplyBtn = getId('applydateformatbtn');
        if (dateApplyBtn) {
          dateApplyBtn.addEventListener('click', (e) => {
            e.preventDefault();
            e.stopPropagation();
            applyDateFormat();
          });
        }
        const clockFontSel = getId('clockfont');
        if (clockFontSel) {
          clockFontSel.addEventListener('input', () => updateClockFontMonoUi(true));
          clockFontSel.addEventListener('change', () => updateClockFontMonoUi(true));
          updateClockFontMonoUi(false);
        }
        classEach("reset", function(el){ el.innerHTML='<svg viewBox="0 0 16 16" class="fill"><path d="M8 3v5a36.973 36.973 0 0 1-2.324-1.166A44.09 44.09 0 0 1 3.417 5.5a52.149 52.149 0 0 1 2.26-1.32A43.18 43.18 0 0 1 8 3z"/><path d="M7 5v1h4.5C12.894 6 14 7.106 14 8.5S12.894 11 11.5 11H1v1h10.5c1.93 0 3.5-1.57 3.5-3.5S13.43 5 11.5 5h-4z"/></svg>'; });
        const settingsImportFile=getId('settings-import-file');
        if(settingsImportFile){
          settingsImportFile.addEventListener('change',function(){
            if(this.files&&this.files[0]) importSettings(this.files[0]);
            this.value='';
          });
        }
      });
    }
    if(pathname=='/wifiset'){
      document.title = `${yoTitle} - Wi-Fi Settings`;
      fetch(`wifiset.html?${fwVersion}`).then(response => response.text()).then(wifiset => {
        getId('content').innerHTML = wifiset;
        fetch('logo.svg').then(response => response.text()).then(svg => {
          getId('logo').innerHTML = svg;
          hideSpinner();
        });
        getId("version").innerText=` | v${fwVersion}`;
        getWiFi(`http://${hostname}/data/wifi.csv`+"?"+new Date().getTime());
        wsSend('getsystem=1');
        initWiFiSettingsView();
      }).catch(() => {
        hideSpinner();
        const content=getId('content');
        if(content) content.innerHTML='<div style="padding:20px;text-align:center;">Wi-Fi settings page failed to load.</div>';
      });
    }
    if(pathname=='/update.html'){
      document.title = `${yoTitle} - Update`;
      fetch(`updform.html?${fwVersion}`).then(response => response.text()).then(updform => {
        getId('content').classList.add('upd');
        getId('content').innerHTML = updform; 
        fetch('logo.svg').then(response => response.text()).then(svg => { 
          getId('logo').innerHTML = svg;
          hideSpinner();
        });
        getId("version").innerText=` | v${fwVersion}`;
      });
    }
    if(pathname=='/ir.html'){
      document.title = `${yoTitle} - IR Recorder`;
      fetch(`irrecord.html?${fwVersion}`).then(response => response.text()).then(ircontent => {
        loadCSS(`ir.css?${fwVersion}`);
        getId('content').innerHTML = ircontent; 
        loadJS(`ir.js?${fwVersion}`, () => {
          fetch('logo.svg').then(response => response.text()).then(svg => { 
            getId('logo').innerHTML = svg;
            initControls();
            hideSpinner();
          });
        });
        getId("version").innerText=` | v${fwVersion}`;
      });
    }
    if (window.location.pathname === '/dlna.html') {  //DLNA mod
      document.title = `${yoTitle} - DLNA`;

      fetch(`dlna.html?${fwVersion}`)
        .then(r => r.text())
        .then(html => {
          getId('content').innerHTML = html;
          fetch('logo.svg').then(r => r.text()).then(svg => {
            getId('logo').innerHTML = svg;
            hideSpinner();
          });
          getId("version").innerText = ` | v${fwVersion}`;
        });
    }
  }else{ // AP mode
    fetch(`wifiset.html?${fwVersion}`).then(response => {
      if (!response.ok) throw new Error('Wi-Fi settings load failed');
      return response.text();
    }).then(wifiset => {
      getId('content').innerHTML = wifiset;
      fetch('logo.svg').then(response => {
        if (!response.ok) return '';
        return response.text();
      }).then(svg => {
        if (svg) getId('logo').innerHTML = svg;
      }).finally(() => {
        hideSpinner();
      });
      getId("version").innerText=` | v${fwVersion}`;
      getWiFi(`http://${hostname}/data/wifi.csv`+"?"+new Date().getTime());
      wsSend('getsystem=1');
      initWiFiSettingsView();
    }).catch(() => {
      hideSpinner();
      const c = getId('content');
      if (c) c.innerHTML = '<div style="padding:20px;text-align:center;">Wi-Fi settings page failed to load. Open <a href="/webboard">/webboard</a>.</div>';
    });
  }
  document.body.addEventListener('click', (event) => {
    const source = event.target && event.target.nodeType === 1 ? event.target : event.target && event.target.parentElement;
    let target = source ? source.closest('div, span, li') : null;
    if(!target) return; 
    if(target.classList && target.classList.contains("knob") && target.parentElement) target = target.parentElement;
    if(target.classList && target.classList.contains("snfknob") && target.parentElement) target = target.parentElement;
    if(target.parentElement && target.parentElement.classList && target.parentElement.classList.contains("play")){ playItem(target.parentElement); return; }
    if(target.classList.contains("navitem")) { getId(target.dataset.target).scrollIntoView({ behavior: 'smooth' }); return; }
    if(target.classList.contains("reset")) { wsSend("reset="+target.dataset.name); return; }
    if(target.classList.contains("done")) { window.location.href=`http://${hostname}/`; return; }
    let command = target.dataset.command;
    if(!command && source && source.dataset && source.dataset.command){
      command = source.dataset.command;
      target = source;
    }
    if(!command && source){
      const cmdHost = source.closest('[data-command]');
      if(cmdHost){
        command = cmdHost.dataset.command;
        target = cmdHost;
      }
    }
    if (command){
      if (target && (target.tagName === 'INPUT' || target.tagName === 'SELECT' || target.tagName === 'TEXTAREA')) {
        return;
      }
      if(target.classList.contains("local")){
        switch(command){
          case "toggle": toggleTarget(target, target.dataset.target); break;
          case "settings": window.location.href=`http://${hostname}/settings.html`; break;
          case "plimport": break;
          case "plexport": window.open(`http://${hostname}/data/playlist.csv`); break;
          case "pladd": plAdd(); break;
          case "pldel": plRemove(); break;
          case "plselect": {
            const headerCb = target.tagName === 'INPUT' ? target : target.querySelector('input[type="checkbox"]');
            const checked = headerCb ? headerCb.checked : false;
            getId('pleditorcontent').querySelectorAll('li .plecheck input[type="checkbox"]').forEach(cb => { cb.checked = checked; });
            break;
          }
          case "plsubmit": submitPlaylist(); break;
          case "fwupdate": window.location.href=`http://${hostname}/update.html`; break;
          case "webboard": window.location.href=`http://${hostname}/webboard`; break;
          case "themeeditor": window.location.href=`http://${hostname}/theme-editor.html`; break;
          case "setupir": window.location.href=`http://${hostname}/ir.html`; break;
          case "wifiset": window.location.href=`http://${hostname}/wifiset`; break;
          case "wifisetback": window.location.href=`http://${hostname}/settings.html`; break;
          case "applyweather":
            let key=getId("wkey").value;
            if(key!=""){
              wsSend("lat="+getId("wlat").value);
              wsSend("lon="+getId("wlon").value);
              wsSend("key="+key);
            }
            break;
          case "applydateformat": applyDateFormat(); break;
          case "applytz": applyTZ(); break;
          case "wifiexport": window.open(`http://${hostname}/data/wifi.csv`+"?"+new Date().getTime()); break;
          case "wifiupload": submitWiFi(); break;
          case "settingsexport": exportSettings(); break;
          case "settingsimport": getId("settings-import-file").click(); break;
          case "reboot": wsSend("reboot=1"); rebootSystem('Rebooting...'); break;
          case "format": wsSend("format=1"); rebootSystem('Format SPIFFS. Rebooting...'); break;
          case "reset":  wsSend("reset=1");  rebootSystem('Reset settings. Rebooting...'); break;
          case "snuffle": toggleSnuffle(); break;
          case "source": switchSource(target.dataset.source); break;
          case "rebootmdns": wsSend(`mdnsname=${getId('mdns').value}`); wsSend("rebootmdns=1"); break;
          default: break;
        }
      }else{
        if(target.classList.contains("checkbox")) checkboxClick(target, command);
        if (target.classList.contains("cmdbutton")) {

          if (command === "toggle" && modesd) {

            // állapot lekérése — játszik-e?
            const isPlaying = target.classList.contains("active");

            if (isPlaying) {
              // épp játszik → STOP → küldjünk 0-t
              wsSend("toggle=0");

            } else {
              // nem játszik → PLAY → küldjük a pozíciót
              let pos = 0;
              const sl = getId("sdpos");
              if (sl && sl.value) pos = sl.value;

              wsSend(`toggle=${pos}`);
            }

          } else {
            wsSend(`${command}=1`);
          }
        }
        if(target.hasClass("pleplay")) playPreview(target.parentElement);
        if(target.classList.contains("play")){ // A lejátszás indítása.
          const item = target.attr('attr-id');
          setCurrentItem(item)
          wsSend(`${command}=${item}`);
        }
      }
  event.preventDefault();
  event.stopPropagation();
    }
  });
  document.body.addEventListener('input', (event) => {
    const originalTarget = event.target;
    let target = originalTarget;
    let command = target && target.dataset ? target.dataset.command : undefined;
    if (!command && target && target.parentElement && target.parentElement.dataset) {
      command = target.parentElement.dataset.command;
    }
    if (command) {
      if(command==='wifiminrssi') return;
      if(target && target.type==='range') sliderInput(target, command);  //<-- range
      else wsSend(`${command}=${target ? target.value : ''}`);   //<-- other
      event.preventDefault(); event.stopPropagation();
    }
    if (originalTarget && originalTarget.id === 'bankselect') {
      const selectedOption = originalTarget.selectedOptions[0];
      if (selectedOption && selectedOption.value) {
        const url = selectedOption.dataset.url; 
        if (url) {
          loadPlaylistFromBank(url);
        }
        originalTarget.value = '';
      }
    }
    // New custom bankselect is handled by initBankSelectFilter function
  });
  document.body.addEventListener('change', (event) => {
    const target = event.target;
    if(target && target.dataset && target.dataset.command==='wifiminrssi') {
      const value=Number(target.value);
      target.value=Number.isFinite(value) ? Math.min(-50, Math.max(-95, Math.round(value))) : -75;
      wsSend(`wifiminrssi=${target.value}`);
      return;
    }
    if (!target || target.tagName !== 'SELECT') return;
    const command = target.dataset ? target.dataset.command : undefined;
    if (!command) return;
    wsSend(`${command}=${target.value}`);
    event.preventDefault();
    event.stopPropagation();
  });
  document.body.addEventListener('mousewheel', (event) => {
    const target = event.target;
    if(target.type==='range'){
      const command = target.dataset.command;
      target.valueAsNumber += event.deltaY>0?-1:1;
      if (command) {
        sliderInput(target, command);
      }
    }
  });
}
async function loadPlaylistFromBank(url) {
  try {
    const response = await fetch(url);
    if (!response.ok) throw new Error('Failed to load playlist');
    const csvText = await response.text();
    const blob = new Blob([csvText], { type: 'text/csv' });
    const file = new File([blob], 'bankplaylist.csv', { type: 'text/csv' });
    const container = new DataTransfer();
    container.items.add(file);
    const fileInput = getId('file-upload');
    fileInput.files = container.files;
    doPlUpload(fileInput);
    setTimeout(() => {
      wsSend('plsubmit');
    }, 1000); 
  } catch (error) {
    alert('Error loading playlist from bank: ' + error.message);
  }
}
async function populateBankSelect() {
  const bankselectOptions = getId('bankselectoptions');
  const bankselectInput = getId('bankselectinput');
  if (!bankselectOptions || !bankselectInput) return;
  
  const defaultOptions = [
    { value: 'plbank1', text: 'Bank 1 General', url: 'https://raw.githubusercontent.com/mirek76/playlist/refs/heads/main/generalplaylist.csv' },
    { value: 'plbank2', text: 'Bank 2 Jazz', url: 'https://raw.githubusercontent.com/mirek76/playlist/refs/heads/main/jazzplaylist.csv' },
    { value: 'plbank3', text: 'Bank 3 Metal', url: 'https://raw.githubusercontent.com/mirek76/playlist/refs/heads/main/metalplaylist.csv' },
    { value: 'plbank4', text: 'Bank 4 Pop', url: 'https://raw.githubusercontent.com/mirek76/playlist/refs/heads/main/popplaylist.csv' },
    { value: 'plbank5', text: 'Bank 5 Rock', url: 'https://raw.githubusercontent.com/mirek76/playlist/refs/heads/main/rockplaylist.csv' }
  ];

  try {
    const response = await fetch('https://api.github.com/repos/mirek76/playlist/contents/');
    if (!response.ok) throw new Error('GitHub API error');
    const files = await response.json();
    const csvFiles = files.filter(file => file.name.endsWith('.csv') && file.type === 'file');
    if (csvFiles.length === 0) throw new Error('No CSV files found');
    
    bankselectOptions.innerHTML = '<div class="bankselectoption" data-value="">Select a bank...</div>';
    
    csvFiles.forEach((file, index) => {
      const option = document.createElement('div');
      option.className = 'bankselectoption';
      option.setAttribute('data-value', `plbank${index + 1}`);
      const name = file.name.replace('.csv', '').replace('playlist', '').replace(/^\w/, c => c.toUpperCase());
      option.textContent = `Bank ${index + 1} ${name}`;
      option.setAttribute('data-url', file.download_url);
      bankselectOptions.appendChild(option);
    });
  } catch (error) {
    console.log('Error loading bank select from GitHub:', error.message);
    bankselectOptions.innerHTML = '<div class="bankselectoption" data-value="">Select a bank...</div>';
    
    defaultOptions.forEach(opt => {
      const option = document.createElement('div');
      option.className = 'bankselectoption';
      option.setAttribute('data-value', opt.value);
      option.textContent = opt.text;
      option.setAttribute('data-url', opt.url);
      bankselectOptions.appendChild(option);
    });
  }
  
  // Uproszczona inicjalizacja - podstawowe nasłuchiwanie zdarzeń
  bankselectInput.addEventListener('click', () => {
    bankselectOptions.classList.toggle('hidden');
  });
  
  bankselectOptions.addEventListener('click', (e) => {
    if (e.target.classList.contains('bankselectoption')) {
      const value = e.target.getAttribute('data-value');
      const url = e.target.getAttribute('data-url');
      if (value && url) {
        loadPlaylistFromBank(url);
      }
      bankselectOptions.classList.add('hidden');
    }
  });
  
  // Zamykanie dropdown po kliknięciu poza elementem
  document.addEventListener('click', (e) => {
    if (bankselectInput && bankselectOptions && 
        !bankselectInput.contains(e.target) && 
        !bankselectOptions.contains(e.target)) {
      bankselectOptions.classList.add('hidden');
    }
  });
}

/** UPDATE **/
var uploadWithError = false;
function doUpdate(el) {
  let binfile = getId('binfile').files[0];
  if(binfile){
    getId('updateform').attr('class','hidden');
    getId("updateprogress").value = 0;
    getId('updateprogress').hidden=false;
    getId('update_cancel_button').hidden=true;
    var formData = new FormData();
    formData.append("updatetarget", getId('uploadtype1').checked?"firmware":"spiffs");
    formData.append("update", binfile);
    var xhr = new XMLHttpRequest();
    uploadWithError = false;
    xhr.onreadystatechange = function() {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        if(xhr.responseText!="OK"){
          getId("uploadstatus").innerHTML = xhr.responseText;
          uploadWithError=true;
        }
      }
    }
    xhr.upload.addEventListener("progress", progressHandler, false);
    xhr.addEventListener("load", completeHandler, false);
    xhr.addEventListener("error", errorHandler, false);
    xhr.addEventListener("abort", abortHandler, false);
    xhr.open("POST",`http://${hostname}/update`,true);
    xhr.send(formData);
  }else{
    alert('Choose something first');
  }
}
function progressHandler(event) {
  var percent = (event.loaded / event.total) * 100;
  getId("uploadstatus").innerHTML = Math.round(percent) + "%&nbsp;&nbsp;uploaded&nbsp;&nbsp;|&nbsp;&nbsp;please wait...";
  getId("updateprogress").value = Math.round(percent);
  if (percent >= 100) {
    getId("uploadstatus").innerHTML = "Please wait, writing file to filesystem";
  }
}
var tickcount=0;
function rebootingProgress(){
  getId("updateprogress").value = Math.round(tickcount/7);
  tickcount+=14;
  if(tickcount>700){
    location.href=`http://${hostname}/`;
  }else{
    setTimeout(rebootingProgress, 200);
  }
}
function completeHandler(event) {
  if(uploadWithError) return;
  getId("uploadstatus").innerHTML = "Upload Complete, rebooting...";
  rebootingProgress();
}
function errorHandler(event) {
  getId('updateform').attr('class','');
  getId('updateprogress').hidden=true;
  getId("updateprogress").value = 0;
  getId("status").innerHTML = "Upload Failed";
}
function abortHandler(event) {
  getId('updateform').attr('class','');
  getId('updateprogress').hidden=true;
  getId("updateprogress").value = 0;
  getId("status").innerHTML = "inUpload Aborted";
}
/** UPDATE **/
// === CUSTOM CODE START ===
//DLNA functions

// --- DLNA playlist activation via /upload  ---

function openDlna() {
  window.location.href = 'dlna.html';
}

// === CUSTOM CODE END ===
