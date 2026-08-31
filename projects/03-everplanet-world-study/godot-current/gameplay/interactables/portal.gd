extends Area3D

@export var portal_id := ""
@export var prompt_text := "이동"

var _busy := false


func get_interaction_prompt() -> String:
	return prompt_text


func is_interaction_enabled() -> bool:
	return visible and monitoring and not _busy


func interact(_player: Node3D) -> void:
	if _busy or GameState.input_locked:
		return
	_busy = true
	var result := GameState.request_portal(portal_id)
	if bool(result.get("ok", false)):
		GameState.input_locked = true
		return
	var dialogue_ui := get_tree().get_first_node_in_group("dialogue_ui")
	if dialogue_ui != null and dialogue_ui.has_method("present"):
		await dialogue_ui.call("present", "", [String(result.get("message", "지금은 이동할 수 없다."))])
	else:
		push_warning(String(result.get("message", "Portal request failed.")))
	await get_tree().process_frame
	_busy = false
