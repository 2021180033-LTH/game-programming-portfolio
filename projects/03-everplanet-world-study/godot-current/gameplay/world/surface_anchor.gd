@tool
class_name SurfaceAnchor
extends Node3D

@export var sphere_center := Vector3(0.0, -79.5, 0.0)
@export_range(1.0, 10000.0, 0.1) var sphere_radius := 80.0
@export_range(-2.0, 2.0, 0.001) var surface_offset := 0.0
@export var align_up_to_surface := false
@export var preview_in_editor := true


func _ready() -> void:
	snap_to_surface()


func _process(_delta: float) -> void:
	if Engine.is_editor_hint() and preview_in_editor:
		snap_to_surface()


func snap_to_surface() -> void:
	if not is_inside_tree():
		return
	var parent_node := get_parent_node_3d()
	var center_global := sphere_center
	if parent_node != null:
		center_global = parent_node.to_global(sphere_center)
	var current := global_position
	var horizontal := Vector2(current.x - center_global.x, current.z - center_global.z)
	var maximum_horizontal := maxf(sphere_radius - 0.001, 0.001)
	if horizontal.length() > maximum_horizontal:
		horizontal = horizontal.normalized() * maximum_horizontal
	var vertical := sqrt(maxf(
		sphere_radius * sphere_radius - horizontal.length_squared(),
		0.0
	))
	var surface_position := Vector3(
		center_global.x + horizontal.x,
		center_global.y + vertical,
		center_global.z + horizontal.y
	)
	var surface_up := (surface_position - center_global).normalized()
	var desired_position := surface_position + surface_up * surface_offset
	if align_up_to_surface:
		var authored_scale := global_basis.get_scale()
		var forward := -global_basis.z
		forward -= surface_up * forward.dot(surface_up)
		if forward.length_squared() < 0.000001:
			forward = Vector3.FORWARD - surface_up * Vector3.FORWARD.dot(surface_up)
		forward = forward.normalized()
		var right := forward.cross(surface_up).normalized()
		var aligned_basis := Basis(
			right * authored_scale.x,
			surface_up * authored_scale.y,
			-forward * authored_scale.z
		)
		global_transform = Transform3D(aligned_basis, desired_position)
	else:
		global_position = desired_position
