module.exports = [
  { "type": "heading", "defaultValue": "TASKWARRIOR / PEBBLE TIME 2" },
  { "type": "text", "defaultValue": "Retro Pixel is the starting theme. API routes below are configurable because the existing Taskwarrior portal and TaskChampion sync service do not yet have a verified REST contract." },

  { "type": "section", "items": [
    { "type": "heading", "defaultValue": "Connection / Tailscale" },
    { "type": "input", "messageKey": "TailnetHost", "defaultValue": "tasks.tail.ac1rv.gay", "label": "Tailscale / Tailnet hostname", "attributes": { "type": "text", "maxlength": 160 } },
    { "type": "input", "messageKey": "APIBaseURL", "defaultValue": "https://tasks.tail.ac1rv.gay", "label": "API base URL", "attributes": { "type": "url", "maxlength": 240 } },
    { "type": "input", "messageKey": "APIToken", "defaultValue": "", "label": "Bearer API token (if required)", "attributes": { "type": "password", "maxlength": 240 } },
    { "type": "text", "defaultValue": "Use the hostname reachable from your phone while Tailscale is connected. Do not use the TaskChampion sync URL as though it were a REST API." }
  ]},

  { "type": "section", "items": [
    { "type": "heading", "defaultValue": "API Routes" },
    { "type": "input", "messageKey": "StatePath", "defaultValue": "/api/v1/state", "label": "State / tasks GET path", "attributes": { "maxlength": 160 } },
    { "type": "input", "messageKey": "StartPath", "defaultValue": "/api/v1/focus/start", "label": "Start / resume POST path", "attributes": { "maxlength": 160 } },
    { "type": "input", "messageKey": "DonePath", "defaultValue": "/api/v1/tasks/{uuid}/done", "label": "Mark done POST path", "attributes": { "maxlength": 160 } },
    { "type": "input", "messageKey": "DeletePath", "defaultValue": "/api/v1/tasks/{uuid}", "label": "Delete task DELETE path", "attributes": { "maxlength": 160 } },
    { "type": "input", "messageKey": "ContextPath", "defaultValue": "/api/v1/context", "label": "Set context POST path", "attributes": { "maxlength": 160 } },
    { "type": "slider", "messageKey": "RefreshSeconds", "label": "Refresh interval (seconds)", "defaultValue": 60, "min": 15, "max": 300, "step": 15 }
  ]},

  { "type": "section", "items": [
    { "type": "heading", "defaultValue": "Appearance" },
    { "type": "select", "messageKey": "Theme", "label": "Theme", "defaultValue": "0", "serializeValueAs": "integer", "options": [
      { "label": "Retro Pixel (default)", "value": "0" },
      { "label": "Modern Dark", "value": "1" },
      { "label": "Minimal Light", "value": "2" },
      { "label": "High Contrast", "value": "3" }
    ]},
    { "type": "select", "messageKey": "Accent", "label": "Accent color", "defaultValue": "0", "serializeValueAs": "integer", "options": [
      { "label": "Yellow", "value": "0" },
      { "label": "Red", "value": "1" },
      { "label": "Green", "value": "2" },
      { "label": "Blue", "value": "3" },
      { "label": "Purple", "value": "4" },
      { "label": "Orange", "value": "5" }
    ]}
  ]},

  { "type": "section", "items": [
    { "type": "heading", "defaultValue": "Task Details" },
    { "type": "toggle", "messageKey": "ShowUrgency", "label": "Show urgency when present", "defaultValue": true },
    { "type": "toggle", "messageKey": "ShowTags", "label": "Show tags when present", "defaultValue": true },
    { "type": "toggle", "messageKey": "ShowAnnotations", "label": "Show annotations when present", "defaultValue": true },
    { "type": "toggle", "messageKey": "ShowDue", "label": "Show due date when present", "defaultValue": true },
    { "type": "text", "defaultValue": "Task fields are displayed only when the API actually supplies them. Annotations remain attached to their task and are not treated as new tasks." }
  ]},

  { "type": "submit", "defaultValue": "Save Taskwarrior Settings" }
];