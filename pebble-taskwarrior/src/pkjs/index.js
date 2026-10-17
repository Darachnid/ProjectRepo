var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var SETTINGS_KEY = 'taskwarrior.settings.v1';
var SESSION_KEY = 'taskwarrior.session.v1';
var state = { connected: false, tasks: [], contexts: [], activeContext: '', current: null, status: 'DEMO DATA • API NOT CONNECTED' };
var refreshTimer = null;
var settings = {};

function defaults() {
  return {
    TailnetHost: 'tasks.tail.ac1rv.gay',
    APIBaseURL: 'https://tasks.tail.ac1rv.gay',
    APIToken: '',
    StatePath: '/api/v1/state',
    StartPath: '/api/v1/focus/start',
    DonePath: '/api/v1/tasks/{uuid}/done',
    DeletePath: '/api/v1/tasks/{uuid}',
    ContextPath: '/api/v1/context',
    RefreshSeconds: 60,
    Theme: 0, Accent: 0,
    ShowUrgency: true, ShowTags: true, ShowAnnotations: true, ShowDue: true
  };
}
function loadSettings() {
  try { settings = Object.assign(defaults(), JSON.parse(localStorage.getItem(SETTINGS_KEY) || '{}')); }
  catch (e) { settings = defaults(); }
}
function saveSettings(s) {
  settings = Object.assign(defaults(), s || {});
  try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch (e) {}
}
function baseUrl() {
  var u = String(settings.APIBaseURL || '').trim();
  if (!u && settings.TailnetHost) u = 'https://' + String(settings.TailnetHost).replace(/^https?:\/\//, '');
  return u.replace(/\/$/, '');
}
function route(path) {
  return baseUrl() + '/' + String(path || '').replace(/^\/+/, '');
}
function request(path, method, body) {
  return new Promise(function(resolve, reject) {
    var xhr = new XMLHttpRequest();
    xhr.open(method || 'GET', route(path), true);
    xhr.timeout = 9000;
    xhr.setRequestHeader('Accept', 'application/json');
    if (body !== undefined) xhr.setRequestHeader('Content-Type', 'application/json');
    if (settings.APIToken) xhr.setRequestHeader('Authorization', 'Bearer ' + settings.APIToken);
    xhr.onreadystatechange = function() {
      if (xhr.readyState !== 4) return;
      if (xhr.status >= 200 && xhr.status < 300) {
        try { resolve(xhr.responseText ? JSON.parse(xhr.responseText) : {}); }
        catch (e) { resolve({}); }
      } else reject(new Error('HTTP ' + xhr.status));
    };
    xhr.onerror = function() { reject(new Error('Network error')); };
    xhr.ontimeout = function() { reject(new Error('Request timed out')); };
    xhr.send(body === undefined ? null : JSON.stringify(body));
  });
}
function sampleState() {
  return {
    tasks: [
      { uuid: 'demo-1', description: 'Write proposal', project: 'PhD.PROPOSAL', priority: 'H', difficulty: 'Medium', estimate: '1h', urgency: 15.8, tags: ['writing'], annotations: ['Draft Chapter 3 scenarios'] },
      { uuid: 'demo-2', description: 'Revise introduction', project: 'PhD.PROPOSAL', priority: 'H', estimate: '45m', urgency: 12.4 },
      { uuid: 'demo-3', description: 'Read sustainability article', project: 'RESEARCH', priority: 'M', estimate: '30m', urgency: 8.2 },
      { uuid: 'demo-4', description: 'Email committee', project: 'ADMIN', priority: 'M', due: '2026-10-21', urgency: 5.6 }
    ],
    contexts: ['none', 'PhD', 'Teaching', 'Personal', 'Home'],
    active_context: 'PhD',
    current_uuid: 'demo-1',
    session_elapsed_seconds: 2537,
    session_running: true
  };
}
function normalizeState(data) {
  data = data || {};
  var tasks = Array.isArray(data.tasks) ? data.tasks.slice() : (Array.isArray(data.pending) ? data.pending.slice() : []);
  tasks.sort(function(a, b) { return Number(b.urgency || 0) - Number(a.urgency || 0); });
  state.tasks = tasks;
  var ctx = data.contexts || data.available_contexts || [];
  state.contexts = Array.isArray(ctx) ? ctx.map(function(c) { return typeof c === 'string' ? c : (c.name || c.context || ''); }).filter(Boolean) : [];
  state.activeContext = data.active_context || data.activeContext || data.context || '';
  var currentUuid = data.current_uuid || data.currentUuid || (data.current_task && data.current_task.uuid);
  state.current = data.current_task || data.currentTask || tasks.find(function(t) { return t.uuid === currentUuid; }) || null;
  if (!state.current && tasks.length) state.current = tasks[0];
  state.connected = true;
  state.status = 'SYNCED';
  var elapsed = Number(data.session_elapsed_seconds || data.elapsed_seconds || 0);
  var saved = getSession();
  state.timerRunning = data.session_running !== undefined ? !!data.session_running : !!(saved && saved.running);
  if (state.current && saved && saved.uuid === state.current.uuid && elapsed === 0) {
    elapsed = Number(saved.elapsed || 0) + (saved.running ? Math.max(0, Math.floor((Date.now() - saved.updatedAt) / 1000)) : 0);
  }
  if (state.current) saveSession(state.current.uuid, elapsed, state.timerRunning);
  state.elapsed = elapsed;
}
function getSession() {
  try { return JSON.parse(localStorage.getItem(SESSION_KEY) || 'null'); } catch (e) { return null; }
}
function saveSession(uuid, elapsed, running) {
  try { localStorage.setItem(SESSION_KEY, JSON.stringify({ uuid: uuid, elapsed: Number(elapsed || 0), running: !!running, updatedAt: Date.now() })); } catch (e) {}
}
function field(obj, keys) {
  for (var i = 0; i < keys.length; i++) if (obj && obj[keys[i]] !== undefined && obj[keys[i]] !== null && obj[keys[i]] !== '') return obj[keys[i]];
  return '';
}
function listLine(t) {
  var values = [t.uuid || t.id || '', t.description || t.title || '(untitled task)', t.project || '', t.urgency === undefined ? '' : t.urgency];
  var limits = [40, 54, 32, 8];
  return values.map(function(v, i) { return String(v).replace(/[~\r\n]/g, ' ').slice(0, limits[i]); }).join('~');
}
function taskDetails(t) {
  if (!t) return '';
  var out = [];
  function add(label, value) { if (value !== '' && value !== null && value !== undefined) out.push(label + ': ' + (Array.isArray(value) ? value.join(', ') : String(value))); }
  add('Priority', t.priority);
  add('Difficulty', t.difficulty);
  add('Estimate', field(t, ['estimate', 'estimated_time', 'duration']));
  if (settings.ShowDue) add('Due', t.due);
  if (settings.ShowUrgency) add('Urgency', t.urgency);
  if (settings.ShowTags) add('Tags', t.tags);
  if (settings.ShowAnnotations) {
    var anns = Array.isArray(t.annotations) ? t.annotations : [];
    anns.forEach(function(a) { add('Note', typeof a === 'string' ? a : (a.description || a.annotation || JSON.stringify(a))); });
  }
  var reserved = { uuid:1, id:1, description:1, title:1, project:1, status:1, priority:1, difficulty:1, estimate:1, estimated_time:1, duration:1, due:1, urgency:1, tags:1, annotations:1, depends:1, entry:1, modified:1, start:1, end:1, wait:1, scheduled:1, until:1, current:1 };
  Object.keys(t).sort().forEach(function(k) {
    if (!reserved[k] && t[k] !== '' && t[k] !== null && t[k] !== undefined && typeof t[k] !== 'object') add(k, t[k]);
  });
  return out.join('\n');
}
function sendState() {
  var t = state.current || {};
  var elapsed = Number(state.elapsed || 0);
  var payload = {
    TaskCount: Math.min(state.tasks.length, 12),
    TaskLines: state.tasks.slice(0, 12).map(listLine).join('\n'),
    ContextCount: Math.min(state.contexts.length, 12),
    ContextLines: state.contexts.slice(0, 12).join('\n'),
    CurrentTitle: String(t.description || t.title || 'No current task').slice(0, 120),
    CurrentProject: String(t.project || '').slice(0, 80),
    CurrentPriority: String(t.priority || '').slice(0, 20),
    CurrentDifficulty: String(t.difficulty || '').slice(0, 32),
    CurrentEstimate: String(field(t, ['estimate', 'estimated_time', 'duration'])).slice(0, 32),
    CurrentDue: String(t.due || '').slice(0, 40),
    CurrentTags: String(Array.isArray(t.tags) ? t.tags.join(', ') : (t.tags || '')).slice(0, 120),
    CurrentAnnotations: String(Array.isArray(t.annotations) ? t.annotations.map(function(a) { return typeof a === 'string' ? a : (a.description || a.annotation || ''); }).join(' • ') : (t.annotations || '')).slice(0, 180),
    CurrentDetails: taskDetails(t).slice(0, 600),
    CurrentUuid: String(t.uuid || t.id || '').slice(0, 64),
    Elapsed: elapsed,
    TimerRunning: state.timerRunning ? 1 : 0,
    ActiveContext: String(state.activeContext || '').slice(0, 80),
    StatusText: String(state.status || '').slice(0, 80)
  };
  Pebble.sendAppMessage(payload, function() {}, function(e) { console.log('AppMessage send failed: ' + JSON.stringify(e)); });
}
function refresh() {
  var path = settings.StatePath || '/api/v1/state';
  return request(path, 'GET').then(function(data) {
    normalizeState(data);
    sendState();
  }).catch(function(err) {
    state = { connected: false, status: 'DEMO • API NOT CONNECTED' };
    normalizeState(sampleState());
    state.connected = false;
    state.status = 'DEMO • API NOT CONNECTED';
    var saved = getSession();
    if (saved && state.current && saved.uuid === state.current.uuid) state.elapsed = saved.elapsed + (saved.running ? Math.floor((Date.now() - saved.updatedAt) / 1000) : 0);
    sendState();
    console.log('Taskwarrior API unavailable: ' + err.message);
  });
}
function localAction(action, uuid) {
  var task = state.tasks.find(function(t) { return String(t.uuid || t.id) === String(uuid); });
  if (action === 'start' && task) {
    state.current = task;
    var prev = getSession();
    var elapsed = prev && prev.uuid === uuid ? prev.elapsed : 0;
    saveSession(uuid, elapsed, true);
    state.elapsed = elapsed;
    state.timerRunning = true;
    state.status = state.connected ? 'SYNCED' : 'DEMO MODE';
  } else if ((action === 'done' || action === 'delete') && task) {
    state.tasks = state.tasks.filter(function(t) { return t !== task; });
    if (state.current === task) {
      state.current = state.tasks[0] || null;
      state.timerRunning = false;
      saveSession(state.current ? String(state.current.uuid || state.current.id) : '', 0, false);
    }
    state.status = state.connected ? 'SYNCED' : 'DEMO MODE';
  }
  sendState();
}
function handleAction(action, uuid) {
  if (!state.connected) { localAction(action, uuid); return; }
  var routePath, method = 'POST', body = { uuid: uuid };
  if (action === 'start') routePath = settings.StartPath;
  else if (action === 'done') routePath = settings.DonePath;
  else if (action === 'delete') { routePath = settings.DeletePath; method = 'DELETE'; }
  else return;
  routePath = String(routePath || '').replace('{uuid}', encodeURIComponent(uuid));
  request(routePath, method, method === 'DELETE' ? undefined : body).then(function() { refresh(); })
    .catch(function(e) { state.status = 'ACTION FAILED • ' + e.message; sendState(); });
}
function handleContext(name) {
  if (!state.connected) { state.activeContext = name; state.status = 'DEMO MODE'; sendState(); return; }
  request(settings.ContextPath || '/api/v1/context', 'POST', { context: name }).then(function() { refresh(); })
    .catch(function(e) { state.status = 'CONTEXT FAILED • ' + e.message; sendState(); });
}
function onMessage(e) {
  var p = e.payload || {};
  if (p.Command === 'refresh') refresh();
  else if (p.Command === 'action') handleAction(String(p.SelectedAction || ''), String(p.SelectedUuid || ''));
  else if (p.Command === 'set_context') handleContext(String(p.Context || ''));
}
function sendUiSettings(s) {
  var payload = {};
  ['Theme','Accent','ShowUrgency','ShowTags','ShowAnnotations','ShowDue'].forEach(function(k) {
    if (s[k] !== undefined) payload[k] = (k.indexOf('Show') === 0) ? (s[k] ? 1 : 0) : Number(s[k] || 0);
  });
  Pebble.sendAppMessage(payload, function() {}, function() {});
}
loadSettings();
Pebble.addEventListener('ready', function() {
  Pebble.addEventListener('appmessage', onMessage);
  sendUiSettings(settings);
  refresh();
  if (refreshTimer) clearInterval(refreshTimer);
  refreshTimer = setInterval(refresh, Math.max(15, Number(settings.RefreshSeconds || 60)) * 1000);
});
Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;
  var incoming;
  try { incoming = clay.getSettings(e.response); } catch (err) { return; }
  saveSettings(incoming);
  sendUiSettings(settings);
  refresh();
  if (refreshTimer) clearInterval(refreshTimer);
  refreshTimer = setInterval(refresh, Math.max(15, Number(settings.RefreshSeconds || 60)) * 1000);
});
