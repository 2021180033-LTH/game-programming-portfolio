extends Control

signal dialogue_closed(result: String)

@onready var panel: PanelContainer = $Panel
@onready var speaker_label: Label = $Panel/Margin/VBox/Speaker
@onready var body_label: Label = $Panel/Margin/VBox/Body
@onready var secondary_button: Button = $Panel/Margin/VBox/Buttons/Secondary
@onready var primary_button: Button = $Panel/Margin/VBox/Buttons/Primary

var _lines: Array[String] = []
var _line_index := 0
var _final_primary_label := ""
var _final_primary_result := "closed"
var _final_secondary_label := ""
var _final_secondary_result := "cancelled"


func _ready() -> void:
	hide()


func present(
		speaker_name: String,
		lines: Array,
		primary_label := "",
		primary_result := "closed",
		secondary_label := "",
		secondary_result := "cancelled"
	) -> String:
	if visible:
		return "busy"
	_lines.clear()
	for line_variant in lines:
		_lines.append(String(line_variant))
	if _lines.is_empty():
		_lines.append("...")
	_line_index = 0
	_final_primary_label = primary_label
	_final_primary_result = primary_result
	_final_secondary_label = secondary_label
	_final_secondary_result = secondary_result
	speaker_label.text = speaker_name
	speaker_label.visible = not speaker_name.is_empty()
	GameState.input_locked = true
	show()
	_show_current_line()
	primary_button.grab_focus()
	var result: String = await dialogue_closed
	return result


func _unhandled_input(event: InputEvent) -> void:
	if not visible:
		return
	if event.is_action_pressed("interact"):
		get_viewport().set_input_as_handled()
		_on_primary_pressed()
	elif event.is_action_pressed("ui_cancel"):
		get_viewport().set_input_as_handled()
		if secondary_button.visible:
			_on_secondary_pressed()
		else:
			_finish("closed")


func _show_current_line() -> void:
	body_label.text = _lines[_line_index]
	var on_final_line := _line_index >= _lines.size() - 1
	if on_final_line:
		primary_button.text = _final_primary_label if not _final_primary_label.is_empty() else "닫기"
		secondary_button.text = _final_secondary_label
		secondary_button.visible = not _final_secondary_label.is_empty()
	else:
		primary_button.text = "다음"
		secondary_button.hide()


func _on_primary_pressed() -> void:
	if not visible:
		return
	if _line_index < _lines.size() - 1:
		_line_index += 1
		_show_current_line()
		return
	_finish(_final_primary_result)


func _on_secondary_pressed() -> void:
	if visible:
		_finish(_final_secondary_result)


func _finish(result: String) -> void:
	hide()
	GameState.input_locked = false
	dialogue_closed.emit(result)
