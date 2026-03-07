@tool
extends EditorPlugin

func _enter_tree() -> void:
	add_autoload_singleton("LiveKitManager", "res://addons/livekit_webview/LiveKitManager.gd")
	print("[LiveKit WebView] Plugin enabled")

func _exit_tree() -> void:
	remove_autoload_singleton("LiveKitManager")
