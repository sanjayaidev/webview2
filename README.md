# LiveKit WebView — Godot 4 Addon

Voice chat for Godot 4 on Windows (and Android) using LiveKit.
Uses WebView2 on Windows, your existing WebView plugin on Android.

---

## Project Structure

```
livekit_webview/
├── src/
│   ├── webview_window.h       ← GDExtension header
│   ├── webview_window.cpp     ← GDExtension implementation
│   └── register_types.cpp     ← GDExtension entry point
├── html/
│   └── livekit.html           ← LiveKit headless page (copy to Godot project)
├── godot_addon/
│   └── addons/livekit_webview/
│       ├── plugin.cfg
│       ├── plugin.gd
│       ├── webview_godot.gdextension
│       ├── LiveKitManager.gd  ← Autoload singleton
│       ├── LiveKitAPI.gd      ← Supabase edge function wrapper
│       └── example_usage.gd
├── CMakeLists.txt
├── install_deps.bat
└── build.bat
```

---

## Step 1 — Prerequisites

Install these first:

- [Visual Studio 2022](https://visualstudio.microsoft.com/) with **Desktop development with C++** workload
- [CMake 3.20+](https://cmake.org/download/) — tick "Add to PATH" during install
- [Git](https://git-scm.com/)

---

## Step 2 — Clone godot-cpp

Clone **next to** this folder (same parent directory):

```bash
git clone -b 4.x https://github.com/godotengine/godot-cpp.git
cd godot-cpp
git submodule update --init
```

Your folder structure should look like:
```
parent_folder/
├── godot-cpp/          ← cloned here
└── livekit_webview/    ← this project
```

---

## Step 3 — Install WebView2 + WIL

Double-click `install_deps.bat`

This downloads:
- **Microsoft.Web.WebView2** — the WebView2 SDK headers + static lib
- **Microsoft.Windows.ImplementationLibrary (WIL)** — smart pointers for COM

---

## Step 4 — Build

Double-click `build.bat`

Output: `godot_addon/addons/livekit_webview/bin/webview_godot.dll`

---

## Step 5 — Copy into your Godot project

Copy the entire `godot_addon/addons/livekit_webview/` folder into your project:

```
your_godot_project/
└── addons/
    └── livekit_webview/
        ├── bin/
        │   └── webview_godot.dll   ← compiled DLL
        ├── html/
        │   └── livekit.html        ← copy from html/ folder
        ├── plugin.cfg
        ├── plugin.gd
        ├── webview_godot.gdextension
        ├── LiveKitManager.gd
        └── LiveKitAPI.gd
```

Also copy `html/livekit.html` → `addons/livekit_webview/html/livekit.html`

---

## Step 6 — Enable the plugin

1. Open your Godot project
2. Go to **Project → Project Settings → Plugins**
3. Enable **LiveKit WebView**
4. This auto-adds `LiveKitManager` as an Autoload singleton

---

## Step 7 — Use in GDScript

```gdscript
func _ready():
    # Configure your Supabase edge function
    LiveKitAPI.configure(
        "https://YOUR_PROJECT.supabase.co/functions/v1/livekit",
        "YOUR_SUPABASE_ANON_KEY"
    )

    # Connect signals
    LiveKitManager.connected.connect(func(room_id): print("Connected: ", room_id))
    LiveKitManager.participant_joined.connect(func(id, name): print(name, " joined"))

# Host a room
func host():
    var result = await LiveKitAPI.create_room(device_uid, username, "My Room")
    if not result.has("error"):
        LiveKitManager.open_panel()
        LiveKitManager.connect_room(result.livekit_url, result.token)

# Join a room
func join(room_id: String):
    var result = await LiveKitAPI.join_room(room_id, device_uid, username)
    if not result.has("error"):
        LiveKitManager.open_panel()
        LiveKitManager.connect_room(result.livekit_url, result.token)

# Controls
LiveKitManager.set_mic_enabled(false)   # mute
LiveKitManager.disconnect_room()         # leave
LiveKitManager.toggle_panel()            # show/hide the panel
LiveKitManager.set_panel_width(600)      # resize
```

---

## GDScript API Reference

### LiveKitManager (Autoload)

| Method | Description |
|--------|-------------|
| `connect_room(url, token)` | Connect to a LiveKit room |
| `disconnect_room()` | Leave the current room |
| `set_mic_enabled(bool)` | Mute / unmute microphone |
| `send_data(message, topic)` | Send text data to all participants |
| `open_panel()` | Show the WebView panel |
| `close_panel()` | Hide the WebView panel |
| `toggle_panel()` | Toggle panel visibility |
| `set_panel_width(float)` | Resize the panel |
| `is_ready()` | Returns true when bridge is ready |

| Signal | Args | Description |
|--------|------|-------------|
| `connected` | `room_id` | Joined a room |
| `disconnected` | `reason` | Left a room |
| `participant_joined` | `identity, name` | Someone joined |
| `participant_left` | `identity, name` | Someone left |
| `speakers_changed` | `speakers: Array` | Active speakers list |
| `mic_state_changed` | `enabled: bool` | Mic muted/unmuted |
| `data_received` | `identity, message, topic` | Data message received |
| `error` | `message` | Something went wrong |

### LiveKitAPI (Static)

| Method | Returns |
|--------|---------|
| `configure(edge_url, anon_key)` | void |
| `list_rooms()` | `{ rooms: Array }` |
| `create_room(device_uid, username, name, emoji)` | `{ room_id, token, livekit_url }` |
| `join_room(room_id, device_uid, username)` | `{ room_id, name, token, livekit_url }` |
| `leave_room(room_id, device_uid, is_host)` | `{ ok: true }` |
| `save_credentials(device_uid, url, key, secret)` | `{ ok: true }` |
| `check_credentials(device_uid)` | `{ configured: bool }` |

---

## Panel Layout

```
┌─────────────────────────────────────────────────────┐
│  Your Godot Game (left half)   │  LiveKit Panel      │
│                                │  ┌───────────────┐  │
│                                │  │ ● LiveKit      │  │
│                                │  │ ─────────────  │  │
│                                │  │ 🎤 Player1(you)│  │
│                                │  │ 🎧 Player2    │  │
│                                │  │ ─────────────  │  │
│                                │  │ [10:23] Ready  │  │
│                                │  │ [10:24] Joined │  │
│                                │  │ ─────────────  │  │
│                                │  │ Participants:2 │  │
│                                └──┴───────────────┘  │
└─────────────────────────────────────────────────────┘
```

---

## Requirements

- Windows 10 / 11 (WebView2 is pre-installed)
- Godot 4.2+
- Your Supabase edge function deployed
