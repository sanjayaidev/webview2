## LiveKitManager.gd
## Autoload — add as singleton: LiveKitManager
##
## Usage:
##   LiveKitManager.connect_room(livekit_url, token)
##   LiveKitManager.disconnect_room()
##   LiveKitManager.set_mic_enabled(true/false)
##   LiveKitManager.send_data("hello", "topic")
##   LiveKitManager.open_panel()
##   LiveKitManager.close_panel()
##
## Signals:
##   connected(room_id)
##   disconnected(reason)
##   participant_joined(identity, name)
##   participant_left(identity, name)
##   speakers_changed(speakers: Array)
##   mic_state_changed(enabled: bool)
##   data_received(identity, message, topic)
##   error(message)

extends Node

# ── Signals ───────────────────────────────────────────────────────────────────
signal connected(room_id: String)
signal disconnected(reason: String)
signal participant_joined(identity: String, name: String)
signal participant_left(identity: String, name: String)
signal speakers_changed(speakers: Array)
signal mic_state_changed(enabled: bool)
signal data_received(identity: String, message: String, topic: String)
signal bridge_ready
signal error(message: String)

# ── Config ────────────────────────────────────────────────────────────────────
## Path to the LiveKit HTML page (copied into your project)
const HTML_PATH := "res://addons/livekit_webview/html/livekit.html"

## Default panel size as a fraction of the viewport width
const DEFAULT_WIDTH_FRACTION := 0.5

# ── Internal refs ─────────────────────────────────────────────────────────────
var _webview:       Node   = null   # WebViewWindow GDExtension node
var _panel:         Control = null  # Container panel
var _is_ready:      bool   = false
var _pending_cmd:   Array  = []     # Commands queued before bridge is ready

# ── Init ──────────────────────────────────────────────────────────────────────
func _ready() -> void:
	if Engine.is_editor_hint():
		return
	_build_panel()

# ── Build the split panel (right half of screen) ──────────────────────────────
func _build_panel() -> void:
	var viewport_size := get_viewport().get_visible_rect().size

	# Outer panel — attached to scene root
	_panel = PanelContainer.new()
	_panel.name = "LiveKitPanel"

	var panel_width  := viewport_size.x * DEFAULT_WIDTH_FRACTION
	var panel_height := viewport_size.y

	_panel.set_size(Vector2(panel_width, panel_height))
	_panel.set_position(Vector2(viewport_size.x - panel_width, 0))
	_panel.set_anchors_and_offsets_preset(Control.PRESET_RIGHT_WIDE)

	# Style
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.05, 0.06, 0.1, 1.0)
	style.border_color = Color(0.12, 0.14, 0.25, 1.0)
	style.set_border_width_all(1)
	_panel.add_theme_stylebox_override("panel", style)

	# Hide by default
	_panel.visible = false

	# Add WebViewWindow (GDExtension node)
	if ClassDB.class_exists("WebViewWindow"):
		_webview = ClassDB.instantiate("WebViewWindow")
		_webview.name = "WebViewWindow"
		_webview.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

		# Connect signals
		_webview.connect("webview_ready",    _on_webview_ready)
		_webview.connect("message_received", _on_message)
		_webview.connect("page_loaded",      _on_page_loaded)
		_webview.connect("page_error",       _on_page_error)

		_panel.add_child(_webview)
	else:
		push_error("[LiveKitManager] WebViewWindow class not found. Is the GDExtension loaded?")
		return

	# Add to the scene tree
	get_tree().root.call_deferred("add_child", _panel)

	print("[LiveKitManager] Panel created — %dx%d at x=%d" % [
		int(panel_width), int(panel_height), int(viewport_size.x - panel_width)
	])

# ── WebView Callbacks ─────────────────────────────────────────────────────────
func _on_webview_ready() -> void:
	print("[LiveKitManager] WebView ready — loading LiveKit page")
	_webview.load_file(HTML_PATH)

func _on_page_loaded() -> void:
	print("[LiveKitManager] Page loaded")
	# Bridge readiness is confirmed by the 'bridge_ready' message from JS

func _on_page_error(message: String) -> void:
	push_error("[LiveKitManager] Page error: " + message)
	emit_signal("error", message)

func _on_message(event: String, data: Dictionary) -> void:
	match event:
		"bridge_ready":
			_is_ready = true
			print("[LiveKitManager] JS bridge ready")
			emit_signal("bridge_ready")
			_flush_pending()

		"connected":
			emit_signal("connected", data.get("room_id", ""))

		"disconnected":
			emit_signal("disconnected", data.get("reason", ""))

		"participant_joined":
			emit_signal("participant_joined",
				data.get("identity", ""),
				data.get("name", ""))

		"participant_left":
			emit_signal("participant_left",
				data.get("identity", ""),
				data.get("name", ""))

		"speakers_changed":
			emit_signal("speakers_changed", data.get("speakers", []))

		"mic_state":
			emit_signal("mic_state_changed", data.get("enabled", false))

		"data_received":
			emit_signal("data_received",
				data.get("identity", ""),
				data.get("message", ""),
				data.get("topic", ""))

		"error":
			emit_signal("error", data.get("message", "Unknown error"))

		_:
			pass  # ignore unknown events

# ── Pending command queue (before bridge is ready) ────────────────────────────
func _flush_pending() -> void:
	for cmd in _pending_cmd:
		_webview.send_message(cmd[0], cmd[1])
	_pending_cmd.clear()

func _send(event: String, data: Dictionary = {}) -> void:
	if _is_ready and _webview:
		_webview.send_message(event, data)
	else:
		_pending_cmd.append([event, data])

# ── Public API ────────────────────────────────────────────────────────────────

## Connect to a LiveKit room with a pre-fetched token
func connect_room(livekit_url: String, token: String) -> void:
	_send("connect", { "url": livekit_url, "token": token })

## Disconnect from the current room
func disconnect_room() -> void:
	_send("disconnect")

## Enable or disable the local microphone
func set_mic_enabled(enabled: bool) -> void:
	_send("set_mic", { "enabled": enabled })

## Send a data message to all participants
func send_data(message: String, topic: String = "default") -> void:
	_send("send_data", { "message": message, "topic": topic })

## Show the LiveKit panel
func open_panel() -> void:
	if _panel:
		_panel.visible = true
		if _webview:
			_webview.show_webview()

## Hide the LiveKit panel
func close_panel() -> void:
	if _panel:
		_panel.visible = false
		if _webview:
			_webview.hide_webview()

## Toggle panel visibility
func toggle_panel() -> void:
	if _panel and _panel.visible:
		close_panel()
	else:
		open_panel()

## Resize the panel (width only — height always fills screen)
func set_panel_width(width: float) -> void:
	if not _panel:
		return
	var viewport_size := get_viewport().get_visible_rect().size
	_panel.set_size(Vector2(width, viewport_size.y))
	_panel.set_position(Vector2(viewport_size.x - width, 0))

## Check if the bridge is ready
func is_ready() -> bool:
	return _is_ready
