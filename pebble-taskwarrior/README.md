# Taskwarrior — Pebble Time 2

A Pebble Time 2 (Emery) companion with a restrained retro-pixel theme: black screen, white typography, yellow selection, and configurable accent colors. Pixel styling is deliberately subtle rather than an all-pixel-art interface.

## Implemented UI

- **Current Task:** title, project/context, accumulated session timer, and fields supplied by the task payload. Long details can scroll beneath the pinned timer.
- **Task List:** up/down button scrolling, sorted by urgency in the phone bridge, and select to open the task action menu.
- **Task Actions:** Start / Resume (default highlighted), Mark as Done, and Delete Task. Delete has a confirmation screen.
- **Context Selector:** selects one of the predefined contexts returned by the API. It does not create or edit contexts.
- **Phone-app settings (Clay):** Tailnet hostname, API base URL, optional bearer token, configurable REST paths, refresh interval, theme, accent color, and switches for optional task fields.

The UI displays task properties only when they exist in the payload. The annotation section displays annotations attached to the selected task; it does not invent extra Taskwarrior fields.

## Build

Use a Rebble/Pebble SDK 3 environment with Emery target support:

```sh
cd pebble-taskwarrior
pebble package install @rebble/clay
pebble build
```

## Important integration status

The known services are:

- Taskwarrior web portal: `https://tasks.tail.ac1rv.gay`
- TaskChampion sync: `https://tasksync.tail.ac1rv.gay`

TaskChampion sync is not a REST API. The exact REST API contract for the portal has not yet been verified. The phone bridge therefore exposes configurable routes and uses a proposed JSON contract documented in [docs/api-contract.md](docs/api-contract.md). Until the real API routes and response schema are confirmed, the app falls back to clearly labelled demo data. Actions in demo mode are local previews only and do not change the server.

Do not put a token in source control. Enter it only in the Pebble phone-app Settings form. Use a Tailnet hostname reachable from the phone while Tailscale is connected.

## State response expected by the adapter

The configurable state route should return JSON with a `tasks` array and optionally `contexts`, `active_context`, `current_task` or `current_uuid`, and `session_elapsed_seconds`. The adapter also recognizes `pending` as an alternate task array. Task rows should have a stable `uuid` and a `description`; other fields are displayed only if present.

## Button behavior

- Current Task: **Up** opens task list; **Down** opens context selector; **Select** opens actions; **Back** exits.
- Task List: **Up/Down** scroll; **Select** opens actions; **Back** returns.
- Context Selector: **Up/Down** scroll; **Select** sets the selected context; **Back** returns.
- Action menu: **Start / Resume** is the first and initially highlighted item, followed by **Mark as Done** and **Delete Task**.
