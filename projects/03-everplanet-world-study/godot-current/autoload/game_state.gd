extends Node

signal region_change_requested(destination_region_id: String, destination_spawn_id: String)
signal quest_state_changed(quest_id: String, new_state: String)
signal state_changed
signal save_loaded

const CONTENT_PATH := "res://data/first_slice.json"
const SAVE_PATH := "user://save_v1.json"
const SAVE_VERSION := 1

const INITIAL_REGION_ID := "region_first_town"
const INITIAL_SPAWN_ID := "spawn_new_game"

const QUEST_NOT_STARTED := "not_started"
const QUEST_ACTIVE := "active"
const QUEST_READY := "ready_to_turn_in"
const QUEST_COMPLETED := "completed"

var content: Dictionary = {}
var current_region_id := INITIAL_REGION_ID
var current_spawn_id := INITIAL_SPAWN_ID
var unlocked_regions: Dictionary = {}
var quest_states: Dictionary = {}
var input_locked := false


func _ready() -> void:
	_load_content()
	_load_or_create_save()


func get_region_data(region_id: String) -> Dictionary:
	return _dictionary_entry("regions", region_id)


func get_portal_data(portal_id: String) -> Dictionary:
	return _dictionary_entry("portals", portal_id)


func get_npc_data(npc_id: String) -> Dictionary:
	return _dictionary_entry("npcs", npc_id)


func get_target_data(target_id: String) -> Dictionary:
	return _dictionary_entry("targets", target_id)


func get_quest_data(quest_id: String) -> Dictionary:
	return _dictionary_entry("quests", quest_id)


func get_quest_state(quest_id: String) -> String:
	return String(quest_states.get(quest_id, QUEST_NOT_STARTED))


func get_quest_status_text(quest_id: String) -> String:
	var quest := get_quest_data(quest_id)
	if quest.is_empty():
		return ""
	var title := String(quest.get("title", quest_id))
	match get_quest_state(quest_id):
		QUEST_ACTIVE:
			return "%s\n%s" % [title, String(quest.get("active_text", ""))]
		QUEST_READY:
			return "%s\n%s" % [title, String(quest.get("ready_text", ""))]
		QUEST_COMPLETED:
			return "%s\n%s" % [title, String(quest.get("completed_text", ""))]
		_:
			return ""


func is_region_unlocked(region_id: String) -> bool:
	return bool(unlocked_regions.get(region_id, false))


func accept_quest(quest_id: String) -> bool:
	if get_quest_data(quest_id).is_empty():
		push_warning("Unknown quest id: %s" % quest_id)
		return false
	if get_quest_state(quest_id) != QUEST_NOT_STARTED:
		return false
	quest_states[quest_id] = QUEST_ACTIVE
	var quest := get_quest_data(quest_id)
	for region_variant in quest.get("unlock_regions_on_accept", []):
		unlocked_regions[String(region_variant)] = true
	_commit_progress(quest_id)
	return true


func mark_objective_complete(quest_id: String, objective_id: String) -> bool:
	if get_quest_state(quest_id) != QUEST_ACTIVE:
		return false
	var quest := get_quest_data(quest_id)
	if String(quest.get("objective_id", "")) != objective_id:
		push_warning("Objective %s does not belong to quest %s" % [objective_id, quest_id])
		return false
	quest_states[quest_id] = QUEST_READY
	_commit_progress(quest_id)
	return true


func complete_quest(quest_id: String) -> bool:
	if get_quest_state(quest_id) != QUEST_READY:
		return false
	quest_states[quest_id] = QUEST_COMPLETED
	_commit_progress(quest_id)
	return true


func request_portal(portal_id: String) -> Dictionary:
	var portal := get_portal_data(portal_id)
	if portal.is_empty():
		return {"ok": false, "message": "연결 정보를 찾을 수 없다."}
	if String(portal.get("from_region_id", "")) != current_region_id:
		return {"ok": false, "message": "이 포털은 현재 지역과 연결되어 있지 않다."}
	var destination_region_id := String(portal.get("destination_region_id", ""))
	var destination_spawn_id := String(portal.get("destination_spawn_id", ""))
	if not _is_known_spawn(destination_region_id, destination_spawn_id):
		return {"ok": false, "message": "목적지 출구 정보를 찾을 수 없다."}
	if not is_region_unlocked(destination_region_id):
		return {
			"ok": false,
			"message": String(portal.get("locked_text", "아직 갈 수 없는 지역이다.")),
		}
	region_change_requested.emit(destination_region_id, destination_spawn_id)
	return {"ok": true, "message": ""}


func confirm_region_change(region_id: String, spawn_id: String) -> bool:
	if not _is_known_spawn(region_id, spawn_id):
		push_error("Cannot save invalid region spawn: %s / %s" % [region_id, spawn_id])
		return false
	current_region_id = region_id
	current_spawn_id = spawn_id
	save_game()
	state_changed.emit()
	return true


func save_game() -> bool:
	var save_data := {
		"version": SAVE_VERSION,
		"current_region_id": current_region_id,
		"current_spawn_id": current_spawn_id,
		"unlocked_regions": unlocked_regions.keys(),
		"quests": quest_states.duplicate(true),
	}
	var file := FileAccess.open(SAVE_PATH, FileAccess.WRITE)
	if file == null:
		push_error("Could not write save file: %s" % FileAccess.get_open_error())
		return false
	file.store_string(JSON.stringify(save_data, "  "))
	return true


func _load_content() -> void:
	var file := FileAccess.open(CONTENT_PATH, FileAccess.READ)
	if file == null:
		push_error("Could not open content data: %s" % CONTENT_PATH)
		content = {}
		return
	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY:
		push_error("Content data is not a JSON object: %s" % CONTENT_PATH)
		content = {}
		return
	content = parsed as Dictionary


func _load_or_create_save() -> void:
	_reset_to_defaults()
	if not FileAccess.file_exists(SAVE_PATH):
		save_game()
		save_loaded.emit()
		state_changed.emit()
		return
	var file := FileAccess.open(SAVE_PATH, FileAccess.READ)
	if file == null:
		push_warning("Save file could not be opened; starting from defaults.")
		save_loaded.emit()
		state_changed.emit()
		return
	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY:
		push_warning("Save file is invalid; defaults will replace it at the next safe checkpoint.")
		save_loaded.emit()
		state_changed.emit()
		return
	_apply_save_data(parsed as Dictionary)
	save_loaded.emit()
	state_changed.emit()


func _reset_to_defaults() -> void:
	current_region_id = INITIAL_REGION_ID
	current_spawn_id = INITIAL_SPAWN_ID
	unlocked_regions = {INITIAL_REGION_ID: true}
	quest_states = {}
	var quests: Dictionary = content.get("quests", {})
	for quest_variant in quests.keys():
		quest_states[String(quest_variant)] = QUEST_NOT_STARTED


func _apply_save_data(save_data: Dictionary) -> void:
	if int(save_data.get("version", -1)) != SAVE_VERSION:
		push_warning("Unsupported save version; starting from defaults.")
		return
	var loaded_regions: Variant = save_data.get("unlocked_regions", [])
	if typeof(loaded_regions) == TYPE_ARRAY:
		for region_variant in loaded_regions:
			var region_id := String(region_variant)
			if not get_region_data(region_id).is_empty():
				unlocked_regions[region_id] = true
	unlocked_regions[INITIAL_REGION_ID] = true
	var loaded_quests: Variant = save_data.get("quests", {})
	if typeof(loaded_quests) == TYPE_DICTIONARY:
		for quest_variant in (loaded_quests as Dictionary).keys():
			var quest_id := String(quest_variant)
			var state := String((loaded_quests as Dictionary)[quest_variant])
			if not get_quest_data(quest_id).is_empty() and _is_valid_quest_state(state):
				quest_states[quest_id] = state
	var loaded_region_id := String(save_data.get("current_region_id", INITIAL_REGION_ID))
	var loaded_spawn_id := String(save_data.get("current_spawn_id", INITIAL_SPAWN_ID))
	if is_region_unlocked(loaded_region_id) and _is_known_spawn(loaded_region_id, loaded_spawn_id):
		current_region_id = loaded_region_id
		current_spawn_id = loaded_spawn_id
	else:
		push_warning("Saved location is invalid or locked; using the initial Town spawn.")


func _commit_progress(quest_id: String) -> void:
	save_game()
	quest_state_changed.emit(quest_id, get_quest_state(quest_id))
	state_changed.emit()


func _dictionary_entry(section_name: String, entry_id: String) -> Dictionary:
	var section: Dictionary = content.get(section_name, {})
	var entry: Variant = section.get(entry_id, {})
	if typeof(entry) != TYPE_DICTIONARY:
		return {}
	return (entry as Dictionary).duplicate(true)


func _is_known_spawn(region_id: String, spawn_id: String) -> bool:
	var region := get_region_data(region_id)
	if region.is_empty():
		return false
	for known_spawn_variant in region.get("spawn_ids", []):
		if String(known_spawn_variant) == spawn_id:
			return true
	return false


func _is_valid_quest_state(state: String) -> bool:
	return state in [QUEST_NOT_STARTED, QUEST_ACTIVE, QUEST_READY, QUEST_COMPLETED]
