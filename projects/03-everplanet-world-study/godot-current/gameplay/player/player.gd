extends CharacterBody3D

signal interaction_focus_changed(prompt_text: String)

@export var move_speed := 4.2
@export var acceleration := 24.0
@export var deceleration := 30.0
@export var gravity_strength := 24.0
@export var turn_speed := 12.0

@onready var interaction_area: Area3D = $InteractionArea

var controls_enabled := true
var fall_limit_y := -12.0
var _focused_interactable: Area3D
var _last_safe_transform := Transform3D.IDENTITY
var _spawn_transform := Transform3D.IDENTITY
var _has_safe_transform := false
var _safe_transform_timer := 0.0
var _last_prompt := ""


func _ready() -> void:
	motion_mode = CharacterBody3D.MOTION_MODE_GROUNDED
	up_direction = Vector3.UP
	floor_snap_length = 0.35
	floor_max_angle = deg_to_rad(46.0)
	floor_stop_on_slope = true


func _physics_process(delta: float) -> void:
	var can_move := controls_enabled and not GameState.input_locked
	var desired_direction := Vector3.ZERO
	if can_move:
		desired_direction = _screen_relative_direction()
	var target_velocity := desired_direction * move_speed
	var horizontal := Vector3(velocity.x, 0.0, velocity.z)
	var rate := acceleration if not desired_direction.is_zero_approx() else deceleration
	horizontal = horizontal.move_toward(target_velocity, rate * delta)
	velocity.x = horizontal.x
	velocity.z = horizontal.z
	if is_on_floor():
		if velocity.y < 0.0:
			velocity.y = -0.5
	else:
		velocity.y -= gravity_strength * delta
	if not desired_direction.is_zero_approx():
		var desired_yaw := atan2(-desired_direction.x, -desired_direction.z)
		rotation.y = lerp_angle(rotation.y, desired_yaw, 1.0 - exp(-turn_speed * delta))
	move_and_slide()
	_update_safe_transform(delta)
	if global_position.y < fall_limit_y:
		_recover_from_fall()
	_update_interaction_focus()


func _unhandled_input(event: InputEvent) -> void:
	if not controls_enabled or GameState.input_locked:
		return
	if not event.is_action_pressed("interact"):
		return
	if not is_instance_valid(_focused_interactable):
		return
	get_viewport().set_input_as_handled()
	_focused_interactable.call_deferred("interact", self)


func place_at_spawn(spawn_transform: Transform3D, ground_position: Vector3, region_fall_limit_y: float) -> void:
	var grounded_transform := spawn_transform
	var spawn_forward := -spawn_transform.basis.z
	spawn_forward.y = 0.0
	if spawn_forward.is_zero_approx():
		spawn_forward = Vector3.FORWARD
	spawn_forward = spawn_forward.normalized()
	var spawn_yaw := atan2(-spawn_forward.x, -spawn_forward.z)
	grounded_transform.basis = Basis(Vector3.UP, spawn_yaw)
	grounded_transform.origin = ground_position + Vector3.UP * 0.03
	global_transform = grounded_transform
	velocity = Vector3.ZERO
	fall_limit_y = region_fall_limit_y
	_spawn_transform = grounded_transform
	_last_safe_transform = grounded_transform
	_has_safe_transform = true
	_safe_transform_timer = 0.35
	reset_physics_interpolation()


func set_controls_enabled(enabled: bool) -> void:
	controls_enabled = enabled
	if not enabled:
		velocity.x = 0.0
		velocity.z = 0.0
		_set_focused_interactable(null)


func _screen_relative_direction() -> Vector3:
	var input_vector := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	if input_vector.is_zero_approx():
		return Vector3.ZERO
	var camera := get_viewport().get_camera_3d()
	if camera == null:
		return Vector3(input_vector.x, 0.0, input_vector.y).normalized()
	var camera_right := camera.global_transform.basis.x
	var camera_forward := -camera.global_transform.basis.z
	camera_right.y = 0.0
	camera_forward.y = 0.0
	camera_right = camera_right.normalized()
	camera_forward = camera_forward.normalized()
	return (camera_right * input_vector.x + camera_forward * -input_vector.y).normalized()


func _update_safe_transform(delta: float) -> void:
	_safe_transform_timer -= delta
	if is_on_floor() and _safe_transform_timer <= 0.0:
		_last_safe_transform = global_transform
		_has_safe_transform = true
		_safe_transform_timer = 0.35


func _recover_from_fall() -> void:
	global_transform = _last_safe_transform if _has_safe_transform else _spawn_transform
	velocity = Vector3.ZERO
	reset_physics_interpolation()
	push_warning("Player crossed the region fall limit and was returned to safe ground.")


func _update_interaction_focus() -> void:
	if GameState.input_locked or not controls_enabled:
		_set_focused_interactable(null)
		return
	var nearest: Area3D
	var nearest_distance_squared := INF
	for area in interaction_area.get_overlapping_areas():
		if not area.has_method("interact"):
			continue
		if area.has_method("is_interaction_enabled") and not bool(area.call("is_interaction_enabled")):
			continue
		var distance_squared := global_position.distance_squared_to(area.global_position)
		if distance_squared < nearest_distance_squared:
			nearest = area
			nearest_distance_squared = distance_squared
	_set_focused_interactable(nearest)


func _set_focused_interactable(next_interactable: Area3D) -> void:
	_focused_interactable = next_interactable
	var prompt := ""
	if is_instance_valid(_focused_interactable):
		if _focused_interactable.has_method("get_interaction_prompt"):
			prompt = String(_focused_interactable.call("get_interaction_prompt"))
		if prompt.is_empty():
			prompt = "상호작용"
	if prompt == _last_prompt:
		return
	_last_prompt = prompt
	interaction_focus_changed.emit(prompt)
