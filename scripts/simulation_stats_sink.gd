extends RefCounted
class_name SimulationStatsSink

var _file: FileAccess = null
var path: String = ""
var _header: Dictionary = {}
var _records_in_chunk: int = 0
var _chunk_index: int = 0
var _max_records_per_chunk: int = 0


func open(new_path: String, header: Dictionary = {}, max_records_per_chunk: int = 0) -> bool:
	path = new_path
	_header = header.duplicate(true)
	_records_in_chunk = 0
	_chunk_index = 0
	_max_records_per_chunk = max_records_per_chunk
	return _open_chunk()


func write_record(record: Dictionary) -> void:
	if _file == null:
		return
	if _max_records_per_chunk > 0 and _records_in_chunk >= _max_records_per_chunk:
		if not _rotate_chunk():
			return
	_file.store_line(JSON.stringify(record))
	_records_in_chunk += 1


func flush() -> void:
	if _file != null:
		_file.flush()


func close() -> void:
	if _file == null:
		return
	_file.flush()
	_file.close()
	_file = null


func _rotate_chunk() -> bool:
	if _file != null:
		_file.flush()
		_file.close()
		_file = null
	_chunk_index += 1
	_records_in_chunk = 0
	return _open_chunk()


func _open_chunk() -> bool:
	var chunk_path := _chunk_path_for_index(_chunk_index)
	var absolute_path := _resolve_absolute_path(chunk_path)
	var base_dir := absolute_path.get_base_dir()
	if not base_dir.is_empty():
		DirAccess.make_dir_recursive_absolute(base_dir)
	_file = FileAccess.open(absolute_path, FileAccess.WRITE)
	if _file == null:
		return false
	if not _header.is_empty():
		var header_record := _header.duplicate(true)
		header_record["chunk_index"] = _chunk_index
		header_record["chunk_path"] = chunk_path
		_file.store_line(JSON.stringify(header_record))
		_records_in_chunk = 1
	return true


func _resolve_absolute_path(input_path: String) -> String:
	if input_path.begins_with("user://"):
		var relative_path := input_path.trim_prefix("user://")
		return "%s/%s" % [OS.get_user_data_dir(), relative_path]
	var global_path := ProjectSettings.globalize_path(input_path)
	if not global_path.is_empty():
		return global_path
	return input_path


func _chunk_path_for_index(index: int) -> String:
	if index <= 0:
		return path
	var base_dir := path.get_base_dir()
	var file_name := path.get_file()
	var extension := path.get_extension()
	var stem := file_name
	if not extension.is_empty():
		stem = file_name.trim_suffix(".%s" % extension)
	var chunk_file := "%s_part%03d" % [stem, index]
	if not extension.is_empty():
		chunk_file = "%s.%s" % [chunk_file, extension]
	if base_dir.is_empty():
		return chunk_file
	return "%s/%s" % [base_dir, chunk_file]
