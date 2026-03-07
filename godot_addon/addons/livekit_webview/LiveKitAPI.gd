## LiveKitAPI.gd
## Helper for calling your Supabase edge function
## Works standalone — no autoload needed, just call statically
##
## Usage:
##   var result = await LiveKitAPI.create_room(device_uid, username, room_name)
##   var result = await LiveKitAPI.join_room(room_id, device_uid, username)
##   LiveKitManager.connect_room(result.livekit_url, result.token)

extends RefCounted

## Set these once at game start (or load from your config)
static var edge_url:  String = ""   # https://xxxx.supabase.co/functions/v1/livekit
static var anon_key:  String = ""   # your Supabase anon key

# ── Internal HTTP call ────────────────────────────────────────────────────────
static func _post(body: Dictionary) -> Dictionary:
	if edge_url.is_empty() or anon_key.is_empty():
		push_error("[LiveKitAPI] edge_url and anon_key must be set before calling API")
		return { "error": "Not configured" }

	var http := HTTPRequest.new()
	Engine.get_main_loop().root.add_child(http)

	var headers := [
		"Content-Type: application/json",
		"apikey: " + anon_key,
	]
	var json_body := JSON.stringify(body)

	var err := http.request(edge_url, headers, HTTPClient.METHOD_POST, json_body)
	if err != OK:
		http.queue_free()
		return { "error": "HTTPRequest failed: " + str(err) }

	var result = await http.request_completed
	http.queue_free()

	# result = [result_code, response_code, headers, body]
	var response_code: int   = result[1]
	var body_bytes:    PackedByteArray = result[3]
	var body_str:      String = body_bytes.get_string_from_utf8()

	var parsed = JSON.parse_string(body_str)
	if parsed == null:
		return { "error": "Invalid JSON response" }

	if response_code != 200:
		return { "error": parsed.get("error", "HTTP " + str(response_code)) }

	return parsed

# ── Public API ────────────────────────────────────────────────────────────────

## Configure the API (call once at start)
static func configure(p_edge_url: String, p_anon_key: String) -> void:
	edge_url = p_edge_url
	anon_key = p_anon_key

## List all active rooms
## Returns: { rooms: Array }
static func list_rooms() -> Dictionary:
	return await _post({ "action": "list_rooms" })

## Create a new room (host must have LiveKit credentials saved)
## Returns: { room_id, token, livekit_url } or { error }
static func create_room(device_uid: String, username: String, room_name: String, emoji: String = "🎙️") -> Dictionary:
	return await _post({
		"action":     "create_room",
		"device_uid": device_uid,
		"username":   username,
		"name":       room_name,
		"emoji":      emoji,
	})

## Join an existing room
## Returns: { room_id, name, emoji, host, token, livekit_url } or { error }
static func join_room(room_id: String, device_uid: String, username: String) -> Dictionary:
	return await _post({
		"action":     "join_room",
		"room_id":    room_id,
		"device_uid": device_uid,
		"username":   username,
	})

## Leave a room
## Returns: { ok: true } or { error }
static func leave_room(room_id: String, device_uid: String, is_host: bool) -> Dictionary:
	return await _post({
		"action":     "leave_room",
		"room_id":    room_id,
		"device_uid": device_uid,
		"is_host":    is_host,
	})

## Save LiveKit credentials for a device (host setup)
static func save_credentials(device_uid: String, livekit_url: String, api_key: String, api_secret: String) -> Dictionary:
	return await _post({
		"action":      "save_credentials",
		"device_uid":  device_uid,
		"livekit_url": livekit_url,
		"api_key":     api_key,
		"api_secret":  api_secret,
	})

## Check if a device has credentials configured
static func check_credentials(device_uid: String) -> Dictionary:
	return await _post({
		"action":     "check_credentials",
		"device_uid": device_uid,
	})
