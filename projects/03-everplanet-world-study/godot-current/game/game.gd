extends Node3D

const REGION_SCENE_PATHS := {
	"region_first_town": "res://regions/first_planet/town/first_town.tscn",
	"region_first_field": "res://regions/first_planet/field/first_field.tscn",
}

@onready var region_root: Node3D = $RegionRoot
@onready var player: CharacterBody3D = $Player
@onready var camera_rig: Node3D = $FixedCameraRig
@onready var interaction_prompt: Label = $UI/InteractionPrompt
@onready var quest_panel: PanelContainer = $UI/QuestPanel
@onready var quest_status: Label = $UI/QuestPanel/Margin/QuestStatus
@onready var system_message: Label = $UI/SystemMessage

var _current_region: Node3D
var _transitioning := false
var _message_generation := 0


func _ready() -> void:
	GameState.region_change_requested.connect(_on_region_change_requested)
	GameState.state_changed.connect(_refresh_quest_status)
	player.interaction_focus_changed.connect(_on_interaction_focus_changed)
	if camera_rig.has_method("set_target"):
		camera_rig.call("set_target", player)
	_refresh_quest_status()
	interaction_prompt.hide()
	system_message.hide()
	call_deferred("_load_initial_region")


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		GameState.save_game()


func _load_initial_region() -> void:
	_change_region(GameState.current_region_id, GameState.current_spawn_id)


func _on_region_change_requested(destination_region_id: String, destination_spawn_id: String) -> void:
	call_deferred("_change_region", destination_region_id, destination_spawn_id)


func _change_region(destination_region_id: String, destination_spawn_id: String) -> void:
	if _transitioning:
		return
	if not REGION_SCENE_PATHS.has(destination_region_id):
		_fail_transition("등록되지 않은 지역 ID: %s" % destination_region_id)
		return
	_transitioning = true
	GameState.input_locked = true
	player.set_controls_enabled(false)
	interaction_prompt.hide()
	var scene_path := String(REGION_SCENE_PATHS[destination_region_id])
	if not ResourceLoader.exists(scene_path, "PackedScene"):
		_fail_transition("지역 씬을 찾을 수 없습니다: %s" % scene_path)
		return
	var packed_variant: Resource = ResourceLoader.load(scene_path, "PackedScene")
	var packed_scene := packed_variant as PackedScene
	if packed_scene == null:
		_fail_transition("지역 씬을 불러올 수 없습니다: %s" % scene_path)
		return
	var next_region := packed_scene.instantiate() as Node3D
	if next_region == null:
		_fail_transition("지역 씬의 루트는 Node3D여야 합니다: %s" % scene_path)
		return
	var authored_region_id := String(next_region.get_meta("region_id", ""))
	if authored_region_id != destination_region_id:
		next_region.free()
		_fail_transition("지역 metadata 불일치: expected=%s actual=%s" % [destination_region_id, authored_region_id])
		return
	if is_instance_valid(_current_region):
		_current_region.queue_free()
		await get_tree().process_frame
	region_root.add_child(next_region)
	_current_region = next_region
	await get_tree().physics_frame
	var spawn_marker := _find_spawn_marker(next_region, destination_spawn_id)
	if spawn_marker == null:
		_fail_transition("스폰 ID를 찾을 수 없습니다: %s / %s" % [destination_region_id, destination_spawn_id])
		return
	var ground_hit := _ground_below_spawn(spawn_marker)
	if ground_hit.is_empty():
		_fail_transition("스폰 아래 WorldSolid 지면이 없습니다: %s / %s" % [destination_region_id, destination_spawn_id])
		return
	var region_data := GameState.get_region_data(destination_region_id)
	var fall_limit := float(region_data.get("fall_limit_y", -12.0))
	player.place_at_spawn(spawn_marker.global_transform, ground_hit["position"], fall_limit)
	if camera_rig.has_method("snap_to_target"):
		camera_rig.call("snap_to_target")
	GameState.confirm_region_change(destination_region_id, destination_spawn_id)
	player.set_controls_enabled(true)
	GameState.input_locked = false
	_transitioning = false
	_show_system_message(String(region_data.get("display_name", destination_region_id)), 1.5)


func _find_spawn_marker(region: Node3D, spawn_id: String) -> Marker3D:
	for node in get_tree().get_nodes_in_group("region_spawn"):
		if node is not Marker3D:
			continue
		if node != region and not region.is_ancestor_of(node):
			continue
		if String(node.get_meta("spawn_id", "")) == spawn_id:
			return node as Marker3D
	return null


func _ground_below_spawn(spawn_marker: Marker3D) -> Dictionary:
	var from := spawn_marker.global_position + Vector3.UP * 3.0
	var to := spawn_marker.global_position + Vector3.DOWN * 8.0
	var query := PhysicsRayQueryParameters3D.create(from, to, 2)
	query.collide_with_areas = false
	query.collide_with_bodies = true
	query.exclude = [player.get_rid()]
	return get_world_3d().direct_space_state.intersect_ray(query)


func _fail_transition(message: String) -> void:
	push_error(message)
	_transitioning = false
	GameState.input_locked = false
	player.set_controls_enabled(true)
	_show_system_message(message, 0.0)


func _on_interaction_focus_changed(prompt_text: String) -> void:
	interaction_prompt.text = "[E / Enter / Space] %s" % prompt_text
	interaction_prompt.visible = not prompt_text.is_empty()


func _refresh_quest_status() -> void:
	var text := GameState.get_quest_status_text("quest_first_errand")
	quest_status.text = text
	quest_panel.visible = not text.is_empty()


func _show_system_message(message: String, duration: float) -> void:
	_message_generation += 1
	var generation := _message_generation
	system_message.text = message
	system_message.show()
	if duration <= 0.0:
		return
	await get_tree().create_timer(duration).timeout
	if generation == _message_generation:
		system_message.hide()
