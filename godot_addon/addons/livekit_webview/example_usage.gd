## example_usage.gd
## Drop this on any node to test LiveKit in your scene
extends Node

# ── Config — set these in your game's config/settings ──
const EDGE_URL  := "https://YOUR_PROJECT.supabase.co/functions/v1/livekit"
const ANON_KEY  := "YOUR_SUPABASE_ANON_KEY"
const DEVICE_UID := "player-device-001"   # use your actual device UID
const USERNAME   := "Player1"

func _ready() -> void:
	# 1. Configure the API
	LiveKitAPI.configure(EDGE_URL, ANON_KEY)

	# 2. Connect LiveKitManager signals
	LiveKitManager.connected.connect(_on_connected)
	LiveKitManager.disconnected.connect(_on_disconnected)
	LiveKitManager.participant_joined.connect(_on_participant_joined)
	LiveKitManager.participant_left.connect(_on_participant_left)
	LiveKitManager.speakers_changed.connect(_on_speakers_changed)
	LiveKitManager.mic_state_changed.connect(_on_mic_changed)
	LiveKitManager.data_received.connect(_on_data_received)
	LiveKitManager.error.connect(_on_error)

	print("[Example] LiveKit ready. Press H to host, J to join, M to mute, L to leave, P to toggle panel")

func _input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed:
		return

	match event.keycode:
		KEY_H: _host_room()
		KEY_J: _join_room()
		KEY_M: _toggle_mute()
		KEY_L: _leave()
		KEY_P: LiveKitManager.toggle_panel()

# ── Host a room ───────────────────────────────────────────────────────────────
func _host_room() -> void:
	print("[Example] Creating room...")
	var result = await LiveKitAPI.create_room(DEVICE_UID, USERNAME, "My Game Room", "🎮")

	if result.has("error"):
		print("[Example] Error: ", result.error)
		return

	print("[Example] Room created: ", result.room_id)
	LiveKitManager.open_panel()
	LiveKitManager.connect_room(result.livekit_url, result.token)

# ── Join a room ───────────────────────────────────────────────────────────────
func _join_room() -> void:
	# In your game, get this room_id from your matchmaking / room list UI
	var room_id := "ROOM-1234"

	print("[Example] Joining room: ", room_id)
	var result = await LiveKitAPI.join_room(room_id, DEVICE_UID, USERNAME)

	if result.has("error"):
		print("[Example] Error: ", result.error)
		return

	print("[Example] Joined: ", result.name)
	LiveKitManager.open_panel()
	LiveKitManager.connect_room(result.livekit_url, result.token)

# ── Mute toggle ───────────────────────────────────────────────────────────────
var _muted := false
func _toggle_mute() -> void:
	_muted = !_muted
	LiveKitManager.set_mic_enabled(!_muted)

# ── Leave ─────────────────────────────────────────────────────────────────────
func _leave() -> void:
	LiveKitManager.disconnect_room()
	LiveKitManager.close_panel()

# ── Signal handlers ───────────────────────────────────────────────────────────
func _on_connected(room_id: String) -> void:
	print("[LiveKit] ✅ Connected to: ", room_id)

func _on_disconnected(reason: String) -> void:
	print("[LiveKit] Disconnected: ", reason)

func _on_participant_joined(identity: String, name: String) -> void:
	print("[LiveKit] 👋 Joined: ", name, " (", identity, ")")

func _on_participant_left(identity: String, name: String) -> void:
	print("[LiveKit] 👋 Left: ", name)

func _on_speakers_changed(speakers: Array) -> void:
	print("[LiveKit] Speaking: ", speakers)

func _on_mic_changed(enabled: bool) -> void:
	print("[LiveKit] Mic: ", "ON" if enabled else "OFF")

func _on_data_received(identity: String, message: String, topic: String) -> void:
	print("[LiveKit] 💬 ", identity, ": ", message)

func _on_error(message: String) -> void:
	push_error("[LiveKit] Error: " + message)
