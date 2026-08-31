extends Area3D

@export var target_id := "target_first_field"

var _busy := false


func get_interaction_prompt() -> String:
	var target := GameState.get_target_data(target_id)
	return String(target.get("prompt", "확인"))


func is_interaction_enabled() -> bool:
	return visible and monitoring and not _busy


func interact(_player: Node3D) -> void:
	if _busy or GameState.input_locked:
		return
	_busy = true
	var target := GameState.get_target_data(target_id)
	if target.is_empty():
		await _present(["목표 정보를 찾을 수 없다."])
		_finish_interaction()
		return
	var quest_id := String(target.get("quest_id", ""))
	var objective_id := String(target.get("objective_id", ""))
	var state := GameState.get_quest_state(quest_id)
	match state:
		GameState.QUEST_ACTIVE:
			var result := await _present(_lines_from(target, "active_lines"), "확인", "confirm")
			if result == "confirm":
				GameState.mark_objective_complete(quest_id, objective_id)
		GameState.QUEST_READY, GameState.QUEST_COMPLETED:
			await _present(_lines_from(target, "done_lines"))
		_:
			await _present(_lines_from(target, "inactive_lines"))
	_finish_interaction()


func _lines_from(target: Dictionary, key: String) -> Array:
	var value: Variant = target.get(key, ["..."])
	return value as Array if typeof(value) == TYPE_ARRAY else [String(value)]


func _present(lines: Array, primary_label := "", primary_result := "closed") -> String:
	var dialogue_ui := get_tree().get_first_node_in_group("dialogue_ui")
	if dialogue_ui == null or not dialogue_ui.has_method("present"):
		push_error("Dialogue UI is missing from the running game scene.")
		return "missing_ui"
	return await dialogue_ui.call("present", "", lines, primary_label, primary_result)


func _finish_interaction() -> void:
	await get_tree().process_frame
	_busy = false
