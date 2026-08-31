extends Area3D

@export var npc_id := "npc_first_guide"

var _busy := false


func get_interaction_prompt() -> String:
	var npc := GameState.get_npc_data(npc_id)
	var display_name := String(npc.get("display_name", "NPC"))
	return "%s와 대화" % display_name


func is_interaction_enabled() -> bool:
	return visible and monitoring and not _busy


func interact(_player: Node3D) -> void:
	if _busy or GameState.input_locked:
		return
	_busy = true
	var npc := GameState.get_npc_data(npc_id)
	if npc.is_empty():
		await _present("", ["대화 정보를 찾을 수 없다."])
		_finish_interaction()
		return
	var display_name := String(npc.get("display_name", "NPC"))
	var quest_id := String(npc.get("quest_id", ""))
	var state := GameState.get_quest_state(quest_id)
	var dialogue_variant: Variant = npc.get("dialogue", {})
	var dialogue: Dictionary = dialogue_variant as Dictionary if typeof(dialogue_variant) == TYPE_DICTIONARY else {}
	var lines_variant: Variant = dialogue.get(state, ["..."])
	var lines: Array = lines_variant as Array if typeof(lines_variant) == TYPE_ARRAY else [String(lines_variant)]
	match state:
		GameState.QUEST_NOT_STARTED:
			var result := await _present(display_name, lines, "수락", "accept", "나중에", "decline")
			if result == "accept":
				GameState.accept_quest(quest_id)
		GameState.QUEST_READY:
			var result := await _present(display_name, lines, "완료", "complete")
			if result == "complete":
				GameState.complete_quest(quest_id)
		_:
			await _present(display_name, lines)
	_finish_interaction()


func _present(
		speaker_name: String,
		lines: Array,
		primary_label := "",
		primary_result := "closed",
		secondary_label := "",
		secondary_result := "cancelled"
	) -> String:
	var dialogue_ui := get_tree().get_first_node_in_group("dialogue_ui")
	if dialogue_ui == null or not dialogue_ui.has_method("present"):
		push_error("Dialogue UI is missing from the running game scene.")
		return "missing_ui"
	return await dialogue_ui.call(
		"present",
		speaker_name,
		lines,
		primary_label,
		primary_result,
		secondary_label,
		secondary_result
	)


func _finish_interaction() -> void:
	await get_tree().process_frame
	_busy = false
