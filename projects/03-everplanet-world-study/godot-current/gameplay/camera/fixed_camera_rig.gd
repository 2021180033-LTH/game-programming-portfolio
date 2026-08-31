extends Node3D

@export var target_path: NodePath
@export var follow_offset := Vector3(0.0, 0.9, 0.0)
@export_range(0.1, 30.0, 0.1) var follow_sharpness := 7.0

var _target: Node3D


func _ready() -> void:
	if not target_path.is_empty():
		_target = get_node_or_null(target_path) as Node3D
	call_deferred("snap_to_target")


func _process(delta: float) -> void:
	if not is_instance_valid(_target):
		return
	var desired_position := _target.global_position + follow_offset
	var weight := 1.0 - exp(-follow_sharpness * delta)
	global_position = global_position.lerp(desired_position, weight)


func set_target(target: Node3D) -> void:
	_target = target
	snap_to_target()


func snap_to_target() -> void:
	if not is_instance_valid(_target):
		return
	global_position = _target.global_position + follow_offset
	reset_physics_interpolation()
