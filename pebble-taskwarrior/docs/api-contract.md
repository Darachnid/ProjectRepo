# Taskwarrior API adapter contract

This is the adapter contract used by the initial Pebble companion, not a claim that these routes already exist on the user's server. Verify or implement the routes against the actual portal/backend before treating server actions as live.

## Configurable routes

All route paths are relative to the configured API base URL.

| Setting | Method | Default path | Purpose |
|---|---|---|---|
| State / tasks | GET | `/api/v1/state` | Return tasks, contexts, current task, and elapsed session time |
| Start / resume | POST | `/api/v1/focus/start` | Start a task session |
| Mark done | POST | `/api/v1/tasks/{uuid}/done` | Mark a task completed |
| Delete task | DELETE | `/api/v1/tasks/{uuid}` | Delete a task |
| Set context | POST | `/api/v1/context` | Set an existing Taskwarrior context |

The watch sends `{ "uuid": "..." }` to start and done routes and `{ "context": "ExistingContextName" }` to the context route. The delete route receives the UUID in the URL and no body. UUIDs are URL-encoded.

An optional bearer token is sent as `Authorization: Bearer …`.

## GET state response

Example response shape:

```json
{
  "tasks": [
    {
      "uuid": "stable-task-uuid",
      "description": "Write proposal",
      "project": "PhD.PROPOSAL",
      "priority": "H",
      "difficulty": "Medium",
      "estimate": "1h",
      "urgency": 15.8,
      "due": "2026-10-21",
      "tags": ["writing"],
      "annotations": ["Draft Chapter 3 scenarios"]
    }
  ],
  "contexts": ["none", "PhD", "Teaching", "Personal", "Home"],
  "active_context": "PhD",
  "current_uuid": "stable-task-uuid",
  "session_elapsed_seconds": 2537
}
```

The adapter also accepts `pending` instead of `tasks`, and `current_task` instead of `current_uuid`. Task list sorting is by descending numeric urgency, with missing urgency treated as zero.

## Session timer

The timer displayed by the watch is elapsed time for the current local session. If the server provides `session_elapsed_seconds`, that value initializes the timer. The phone bridge caches a session locally as a fallback. A real multi-device authoritative timer would require a server-side session model.

## Required verification before production use

1. Confirm the actual portal/backend API base URL and supported authentication.
2. Confirm GET response field names and context listing format.
3. Confirm that start/resume, done, delete, and set-context operations exist and have the described semantics.
4. Test Tailscale connectivity and authentication from the phone.
5. Confirm delete behavior and server-side permission checks.

Until those checks pass, the phone bridge displays `DEMO • API NOT CONNECTED` and uses sample tasks. Actions taken in demo mode are local previews only.
