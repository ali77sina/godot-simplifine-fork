#pragma once

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/variant/dictionary.h"

class Node;

class EditorTools : public Object {
	GDCLASS(EditorTools, Object);

private:
	static Dictionary _get_node_info(Node *p_node);
	static Node *_get_node_from_path(const String &p_path, Dictionary &r_error_result);

public:
    static Dictionary _predict_code_edit(const String &p_file_content, const String &p_prompt, const String &p_api_endpoint);
    	static Dictionary _call_apply_endpoint(const String &p_file_path, const String &p_file_content, const Dictionary &p_ai_args, const String &p_api_endpoint);
	static String _clean_backend_content(const String &p_content);
	static String _convert_javascript_to_gdscript(const String &p_content);
	static String _fix_malformed_content(const String &p_content);
	static String _generate_unified_diff(const String &p_original, const String &p_modified, const String &p_file_path);
	static Array _check_compilation_errors(const String &p_file_path, const String &p_content);
    static void set_api_endpoint(const String &p_endpoint);

	// Individual Tool Methods (used by universal tools)
	static Dictionary get_scene_info(const Dictionary &p_args);
	static Dictionary get_all_nodes(const Dictionary &p_args);
	static Dictionary search_nodes_by_type(const Dictionary &p_args);
	static Dictionary get_editor_selection(const Dictionary &p_args);
	static Dictionary get_node_properties(const Dictionary &p_args);
	static Dictionary save_scene(const Dictionary &p_args);
	static Dictionary create_node(const Dictionary &p_args);
	static Dictionary delete_node(const Dictionary &p_args);
	static Dictionary set_node_property(const Dictionary &p_args);
	static Dictionary move_node(const Dictionary &p_args);
	static Dictionary call_node_method(const Dictionary &p_args);
	static Dictionary get_available_classes(const Dictionary &p_args);
	static Dictionary get_node_script(const Dictionary &p_args);
	static Dictionary attach_script(const Dictionary &p_args);
	static Dictionary manage_scene(const Dictionary &p_args);
	static Dictionary add_collision_shape(const Dictionary &p_args);
	static Dictionary generalnodeeditor(const Dictionary &p_args);
	static Dictionary list_project_files(const Dictionary &p_args);
	static Dictionary read_file_content(const Dictionary &p_args);
	static Dictionary read_file_advanced(const Dictionary &p_args);
	static Dictionary apply_edit(const Dictionary &p_args);
	static Dictionary check_compilation_errors(const Dictionary &p_args);

	// New Debugging Tools
	static Dictionary run_scene(const Dictionary &p_args);
	static Dictionary get_scene_tree_hierarchy(const Dictionary &p_args);
	static Dictionary inspect_physics_body(const Dictionary &p_args);
	static Dictionary get_camera_info(const Dictionary &p_args);
	static Dictionary take_screenshot(const Dictionary &p_args);
	static Dictionary check_node_in_scene_tree(const Dictionary &p_args);
	static Dictionary inspect_animation_state(const Dictionary &p_args);
	static Dictionary get_layers_and_zindex(const Dictionary &p_args);

	// Universal Tools (New Consolidated API)
	static Dictionary universal_node_manager(const Dictionary &p_args);
	static Dictionary universal_file_manager(const Dictionary &p_args);
	static Dictionary scene_manager(const Dictionary &p_args);
}; 