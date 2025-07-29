#include "editor_tools.h"

#include "core/crypto/crypto.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "scene/main/node.h"
#include "scene/main/window.h"

#include <functional>

void EditorTools::set_api_endpoint(const String &p_endpoint) {
    // This is now handled in AIChatDock
}

Dictionary EditorTools::_get_node_info(Node *p_node) {
	Dictionary node_info;
	if (!p_node) {
		return node_info;
	}
	node_info["name"] = p_node->get_name();
	node_info["type"] = p_node->get_class();
	node_info["path"] = p_node->get_path();
	node_info["owner"] = p_node->get_owner() ? p_node->get_owner()->get_path() : String();
	node_info["child_count"] = p_node->get_child_count();
	return node_info;
}

Node *EditorTools::_get_node_from_path(const String &p_path, Dictionary &r_error_result) {
	Node *root = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	if (!root) {
		r_error_result["success"] = false;
		r_error_result["message"] = "No scene is currently being edited.";
		return nullptr;
	}
	Node *node = root->get_node_or_null(p_path);
	if (!node) {
		r_error_result["success"] = false;
		r_error_result["message"] = "Node not found at path: " + p_path;
	}
	return node;
}

Dictionary EditorTools::get_scene_info(const Dictionary &p_args) {
	Dictionary result;
	Node *root = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	if (!root) {
		result["success"] = false;
		result["message"] = "No scene is currently being edited.";
		return result;
	}
	result["success"] = true;
	result["scene_name"] = root->get_scene_file_path();
	result["root_node"] = _get_node_info(root);
	return result;
}

Dictionary EditorTools::get_all_nodes(const Dictionary &p_args) {
	Dictionary result;
	Node *root = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	if (!root) {
		result["success"] = false;
		result["message"] = "No scene is currently being edited.";
		return result;
	}
	
	Array nodes;
	
	// Helper lambda to recursively collect all nodes
	std::function<void(Node*)> collect_nodes = [&](Node* node) {
		if (node) {
			nodes.push_back(_get_node_info(node));
			// Recursively collect all children
			for (int i = 0; i < node->get_child_count(); i++) {
				collect_nodes(node->get_child(i));
			}
		}
	};
	
	// Start collecting from the root
	collect_nodes(root);
	
	result["success"] = true;
	result["nodes"] = nodes;
	return result;
}

Dictionary EditorTools::search_nodes_by_type(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("type")) {
		result["success"] = false;
		result["message"] = "Missing 'type' argument.";
		return result;
	}
	String type = p_args["type"];
	Array nodes;
	Node *root = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	if (root) {
		List<Node *> node_list;
		root->get_tree()->get_nodes_in_group("_scenetree", &node_list);
		for (Node *node : node_list) {
			if (node->is_class(type)) {
				nodes.push_back(_get_node_info(node));
			}
		}
	}
	result["success"] = true;
	result["nodes"] = nodes;
	return result;
}

Dictionary EditorTools::get_editor_selection(const Dictionary &p_args) {
	Dictionary result;
	Array selection = EditorNode::get_singleton()->get_editor_selection()->get_selected_nodes();
	Array nodes;
	for (int i = 0; i < selection.size(); i++) {
		Node *node = Object::cast_to<Node>(selection[i]);
		if (node) {
			nodes.push_back(_get_node_info(node));
		}
	}
	result["success"] = true;
	result["selected_nodes"] = nodes;
	return result;
}

Dictionary EditorTools::get_node_properties(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("path")) {
		result["success"] = false;
		result["message"] = "Missing 'path' argument.";
		return result;
	}
	Node *node = _get_node_from_path(p_args["path"], result);
	if (!node) {
		return result;
	}

	List<PropertyInfo> properties;
	node->get_property_list(&properties);

	Dictionary props_dict;
	for (const PropertyInfo &prop_info : properties) {
		if (prop_info.usage & PROPERTY_USAGE_EDITOR) {
			props_dict[prop_info.name] = node->get(prop_info.name);
		}
	}

	result["success"] = true;
	result["properties"] = props_dict;
	return result;
}

Dictionary EditorTools::save_scene(const Dictionary &p_args) {
	Dictionary result;
	Node *root = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	if (!root) {
		result["success"] = false;
		result["message"] = "No scene is currently being edited.";
		return result;
	}

	String path = root->get_scene_file_path();
	if (path.is_empty()) {
		result["success"] = false;
		result["message"] = "Scene has no file path. Please save it manually first.";
		return result;
	}

	Error err = EditorInterface::get_singleton()->save_scene();
	if (err != OK) {
		result["success"] = false;
		result["message"] = "Failed to save scene. It might not have a path yet.";
	} else {
		result["success"] = true;
		result["message"] = "Scene saved successfully.";
	}

	return result;
}

Dictionary EditorTools::create_node(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("type") || !p_args.has("name")) {
		result["success"] = false;
		result["message"] = "Missing 'type' or 'name' argument.";
		return result;
	}
	String type = p_args["type"];
	String name = p_args["name"];
	Node *parent = nullptr;

	if (p_args.has("parent")) {
		parent = _get_node_from_path(p_args["parent"], result);
		if (!parent) {
			return result;
		}
	} else {
		parent = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
		if (!parent) {
			result["success"] = false;
			result["message"] = "No scene is currently being edited to add a root node.";
			return result;
		}
	}

	Node *new_node = memnew(Node);
	if (ClassDB::can_instantiate(type)) {
		new_node = (Node *)ClassDB::instantiate(type);
	} else {
		result["success"] = false;
		result["message"] = "Cannot instantiate node of type: " + type;
		return result;
	}

	new_node->set_name(name);
	parent->add_child(new_node);
	new_node->set_owner(parent->get_owner() ? parent->get_owner() : parent);

	result["success"] = true;
	result["node_path"] = new_node->get_path();
	result["message"] = "Node created successfully.";
	
	// Check for configuration warnings
	PackedStringArray warnings = new_node->get_configuration_warnings();
	if (!warnings.is_empty()) {
		String warning_text = "";
		for (int i = 0; i < warnings.size(); i++) {
			warning_text += warnings[i];
			if (i < warnings.size() - 1) warning_text += "; ";
		}
		result["warnings"] = warning_text;
		result["message"] = "Node created successfully, but has warnings: " + warning_text;
	}
	
	return result;
}

Dictionary EditorTools::delete_node(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("path")) {
		result["success"] = false;
		result["message"] = "Missing 'path' argument.";
		return result;
	}
	Node *node = _get_node_from_path(p_args["path"], result);
	if (!node) {
		return result;
	}
	node->queue_free();
	result["success"] = true;
	result["message"] = "Node deleted successfully.";
	return result;
}

Dictionary EditorTools::set_node_property(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("path") || !p_args.has("property") || !p_args.has("value")) {
		result["success"] = false;
		result["message"] = "Missing 'path', 'property', or 'value' argument.";
		return result;
	}
	Node *node = _get_node_from_path(p_args["path"], result);
	if (!node) {
		return result;
	}
	StringName prop = p_args["property"];
	bool valid = false;
	node->set(prop, p_args["value"], &valid);
	if (!valid) {
		result["success"] = false;
		result["message"] = "Failed to set property '" + String(prop) + "'. It might be invalid or read-only.";
		return result;
	}
	result["success"] = true;
	result["message"] = "Property set successfully.";
	return result;
}

Dictionary EditorTools::move_node(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("path") || !p_args.has("new_parent")) {
		result["success"] = false;
		result["message"] = "Missing 'path' or 'new_parent' argument.";
		return result;
	}
	Node *node = _get_node_from_path(p_args["path"], result);
	if (!node) {
		return result;
	}
	Node *new_parent = _get_node_from_path(p_args["new_parent"], result);
	if (!new_parent) {
		return result;
	}
	node->get_parent()->remove_child(node);
	new_parent->add_child(node);
	result["success"] = true;
	result["message"] = "Node moved successfully.";
	return result;
}

Dictionary EditorTools::call_node_method(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("path") || !p_args.has("method")) {
		result["success"] = false;
		result["message"] = "Missing 'path' or 'method' argument.";
		return result;
	}

	Node *node = _get_node_from_path(p_args["path"], result);
	if (!node) {
		return result;
	}

	StringName method = p_args["method"];
	Array args = p_args.has("args") ? (Array)p_args["args"] : Array();

	Variant ret = node->callv(method, args);

	result["success"] = true;
	result["return_value"] = ret;

	return result;
}

Dictionary EditorTools::get_available_classes(const Dictionary &p_args) {
	Dictionary result;
	List<StringName> class_list;
	ClassDB::get_class_list(&class_list);

	Array classes;
	for (const StringName &E : class_list) {
		if (ClassDB::can_instantiate(E) && ClassDB::is_parent_class(E, "Node")) {
			classes.push_back(String(E));
		}
	}

	result["success"] = true;
	result["classes"] = classes;
	return result;
}

Dictionary EditorTools::get_node_script(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("path")) {
		result["success"] = false;
		result["message"] = "Missing 'path' argument.";
		return result;
	}

	Node *node = _get_node_from_path(p_args["path"], result);
	if (!node) {
		return result;
	}

	Ref<Script> script = node->get_script();
	if (script.is_null()) {
		result["success"] = false;
		result["message"] = "Node has no script attached.";
	} else {
		result["success"] = true;
		result["script_path"] = script->get_path();
	}

	return result;
}

Dictionary EditorTools::attach_script(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("path") || !p_args.has("script_path")) {
		result["success"] = false;
		result["message"] = "Missing 'path' or 'script_path' argument.";
		return result;
	}

	Node *node = _get_node_from_path(p_args["path"], result);
	if (!node) {
		return result;
	}

	Ref<Script> script = ResourceLoader::load(p_args["script_path"]);
	if (script.is_null()) {
		result["success"] = false;
		result["message"] = "Failed to load script at path: " + String(p_args["script_path"]);
		return result;
	}

	node->set_script(script);
	result["success"] = true;
	result["message"] = "Script attached successfully.";
	return result;
}

Dictionary EditorTools::manage_scene(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("operation")) {
		result["success"] = false;
		result["message"] = "Missing 'operation' argument.";
		return result;
	}

	String operation = p_args["operation"];

	if (operation == "create_new") {
		EditorNode::get_singleton()->new_scene();
		
		// Create a default Node2D root for the new scene
		Node *root_node = nullptr;
		if (ClassDB::can_instantiate("Node2D")) {
			root_node = (Node *)ClassDB::instantiate("Node2D");
			root_node->set_name("Main");
			
			// Properly set the scene root using EditorNode's method
			EditorNode::get_singleton()->set_edited_scene(root_node);
			root_node->set_owner(root_node);
		}
		
		if (root_node) {
			result["success"] = true;
			result["message"] = "New scene created with Node2D root.";
		} else {
			result["success"] = false;
			result["message"] = "Failed to create scene root node.";
		}

	} else if (operation == "save_as") {
		if (!p_args.has("path")) {
			result["success"] = false;
			result["message"] = "Missing 'path' argument for save_as operation.";
			return result;
		}
		String path = p_args["path"];
		EditorInterface::get_singleton()->save_scene_as(path);
		result["success"] = true;
		result["message"] = "Scene saved as " + path;

	} else if (operation == "open") {
		if (!p_args.has("path")) {
			result["success"] = false;
			result["message"] = "Missing 'path' argument for open operation.";
			return result;
		}
		String path = p_args["path"];
		EditorInterface::get_singleton()->open_scene_from_path(path);
		result["success"] = true;
		result["message"] = "Scene opened: " + path;

	} else {
		result["success"] = false;
		result["message"] = "Unknown operation: " + operation + ". Supported: create_new, save_as, open";
	}

	return result;
}

Dictionary EditorTools::add_collision_shape(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("node_path")) {
		result["success"] = false;
		result["message"] = "Missing 'node_path' argument.";
		return result;
	}

	String node_path = p_args["node_path"];
	String shape_type = p_args.get("shape_type", "rectangle");

	Node *node = _get_node_from_path(node_path, result);
	if (!node) {
		return result;
	}

	// Check if it's a physics body that needs collision
	if (!node->is_class("CharacterBody2D") && !node->is_class("RigidBody2D") && !node->is_class("StaticBody2D") && !node->is_class("Area2D")) {
		result["success"] = false;
		result["message"] = "Node is not a physics body that can have collision shapes.";
		return result;
	}

	// Create CollisionShape2D
	Node *collision_shape = nullptr;
	if (ClassDB::can_instantiate("CollisionShape2D")) {
		collision_shape = (Node *)ClassDB::instantiate("CollisionShape2D");
	} else {
		result["success"] = false;
		result["message"] = "Cannot instantiate CollisionShape2D.";
		return result;
	}

	// Create the actual shape resource based on type
	Variant shape_resource;
	if (shape_type == "rectangle") {
		if (ClassDB::can_instantiate("RectangleShape2D")) {
			shape_resource = ClassDB::instantiate("RectangleShape2D");
		}
	} else if (shape_type == "circle") {
		if (ClassDB::can_instantiate("CircleShape2D")) {
			shape_resource = ClassDB::instantiate("CircleShape2D");
		}
	} else if (shape_type == "capsule") {
		if (ClassDB::can_instantiate("CapsuleShape2D")) {
			shape_resource = ClassDB::instantiate("CapsuleShape2D");
		}
	}

	if (shape_resource.get_type() == Variant::NIL) {
		collision_shape->queue_free();
		result["success"] = false;
		result["message"] = "Failed to create shape resource of type: " + shape_type;
		return result;
	}

	// Set the shape on the collision node
	collision_shape->set("shape", shape_resource);
	
	// Add as child safely
	if (node && collision_shape) {
		node->add_child(collision_shape);
		collision_shape->set_owner(node->get_owner() ? node->get_owner() : node);
	} else {
		if (collision_shape) collision_shape->queue_free();
		result["success"] = false;
		result["message"] = "Failed to add collision shape - invalid nodes.";
		return result;
	}

	result["success"] = true;
	result["message"] = "CollisionShape2D with " + shape_type + " shape added to " + node_path;
	return result;
}

Dictionary EditorTools::generalnodeeditor(const Dictionary &p_args) {
	Dictionary result;
	
	// Validate required arguments
	if (!p_args.has("node_path")) {
		result["success"] = false;
		result["message"] = "Missing 'node_path' argument.";
		return result;
	}
	
	String node_path = p_args["node_path"];
	Array node_paths;
	
	// Support both single node and array of nodes
	if (node_path.begins_with("[") && node_path.ends_with("]")) {
		// Parse array of node paths
		String paths_str = node_path.substr(1, node_path.length() - 2);
		PackedStringArray paths = paths_str.split(",");
		for (int i = 0; i < paths.size(); i++) {
			node_paths.push_back(paths[i].strip_edges());
		}
	} else {
		node_paths.push_back(node_path);
	}
	
	Dictionary properties = p_args.get("properties", Dictionary());
	String texture_path = p_args.get("texture_path", "");
	bool batch_operation = node_paths.size() > 1;
	
	Array operation_results;
	int success_count = 0;
	int failure_count = 0;
	
	// Process each node
	for (int i = 0; i < node_paths.size(); i++) {
		String current_node_path = node_paths[i];
		Dictionary node_result;
		node_result["node_path"] = current_node_path;
		
		Dictionary temp_result;
		Node *node = _get_node_from_path(current_node_path, temp_result);
		if (!node) {
			node_result["success"] = false;
			node_result["message"] = temp_result["message"];
			operation_results.push_back(node_result);
			failure_count++;
			continue;
		}
		
		Array property_results;
		bool node_success = true;
		String node_message = "";
		
		// Handle texture assignment
		if (!texture_path.is_empty()) {
			bool texture_applied = false;
			String texture_error = "";
			
			// Check if node supports texture
			bool has_texture_property = false;
			bool valid = false;
			node->get("texture", &valid);
			has_texture_property = valid;
			
			if (node->has_method("set_texture") || has_texture_property) {
				Ref<Texture2D> texture = ResourceLoader::load(texture_path);
				if (texture.is_valid()) {
					if (node->has_method("set_texture")) {
						Array args;
						args.push_back(texture);
						node->callv("set_texture", args);
						texture_applied = true;
					} else {
						bool valid = false;
						node->set("texture", texture, &valid);
						texture_applied = valid;
					}
					
					if (!texture_applied) {
						texture_error = "Failed to apply texture to node";
					}
				} else {
					texture_error = "Failed to load texture from: " + texture_path;
				}
			} else {
				texture_error = "Node type '" + node->get_class() + "' does not support texture assignment";
			}
			
			Dictionary texture_result;
			texture_result["operation"] = "texture_assignment";
			texture_result["success"] = texture_applied;
			texture_result["message"] = texture_applied ? "Texture applied successfully" : texture_error;
			property_results.push_back(texture_result);
			
			if (!texture_applied) {
				node_success = false;
			}
		}
		
		// Handle property modifications
		Array property_keys = properties.keys();
		for (int j = 0; j < property_keys.size(); j++) {
			String property_name = property_keys[j];
			Variant property_value = properties[property_name];
			
			Dictionary prop_result;
			prop_result["operation"] = "property_modification";
			prop_result["property"] = property_name;
			prop_result["value"] = property_value;
			
			// Special handling for common properties
			if (property_name == "position" && property_value.get_type() == Variant::ARRAY) {
				Array pos_array = property_value;
				if (pos_array.size() >= 2) {
					Vector2 position(pos_array[0], pos_array[1]);
					bool valid = false;
					node->set("position", position, &valid);
					prop_result["success"] = valid;
					prop_result["message"] = valid ? "Position set successfully" : "Failed to set position";
				} else {
					prop_result["success"] = false;
					prop_result["message"] = "Position array must have at least 2 elements [x, y]";
				}
			} else if (property_name == "scale" && property_value.get_type() == Variant::ARRAY) {
				Array scale_array = property_value;
				if (scale_array.size() >= 2) {
					Vector2 scale(scale_array[0], scale_array[1]);
					bool valid = false;
					node->set("scale", scale, &valid);
					prop_result["success"] = valid;
					prop_result["message"] = valid ? "Scale set successfully" : "Failed to set scale";
				} else {
					prop_result["success"] = false;
					prop_result["message"] = "Scale array must have at least 2 elements [x, y]";
				}
			} else {
				// Standard property setting
				bool valid = false;
				node->set(StringName(property_name), property_value, &valid);
				prop_result["success"] = valid;
				prop_result["message"] = valid ? 
					"Property '" + property_name + "' set successfully" : 
					"Failed to set property '" + property_name + "'. It might be invalid or read-only";
			}
			
			property_results.push_back(prop_result);
			
			if (!prop_result["success"]) {
				node_success = false;
			}
		}
		
		// Compile node result
		node_result["success"] = node_success;
		node_result["property_results"] = property_results;
		
		if (node_success) {
			success_count++;
			node_result["message"] = "All operations completed successfully on " + current_node_path;
		} else {
			failure_count++;
			node_result["message"] = "Some operations failed on " + current_node_path;
		}
		
		operation_results.push_back(node_result);
	}
	
	// Compile final result
	result["operation_results"] = operation_results;
	result["batch_operation"] = batch_operation;
	result["total_nodes"] = node_paths.size();
	result["success_count"] = success_count;
	result["failure_count"] = failure_count;
	
	if (failure_count == 0) {
		result["success"] = true;
		result["message"] = String("Successfully processed all ") + String::num_int64(success_count) + " node(s)";
	} else if (success_count == 0) {
		result["success"] = false;
		result["message"] = String("Failed to process all ") + String::num_int64(failure_count) + " node(s)";
	} else {
		result["success"] = true; // Partial success
		result["message"] = String("Processed ") + String::num_int64(success_count) + " successfully, " + 
							String::num_int64(failure_count) + " failed";
	}
	
	return result;
}

Dictionary EditorTools::list_project_files(const Dictionary &p_args) {
	Dictionary result;
	String path = p_args.has("dir") ? p_args["dir"] : "res://";
	String filter = p_args.has("filter") ? p_args["filter"] : "";

	Array files;
	Array dirs;
	Ref<DirAccess> dir = DirAccess::open(path);
	if (dir.is_valid()) {
		dir->list_dir_begin();
		String file_name = dir->get_next();
		while (file_name != "") {
			if (dir->current_is_dir()) {
				if (file_name != "." && file_name != "..") {
					dirs.push_back(file_name);
				}
			} else {
				if (filter.is_empty() || file_name.match(filter)) {
					files.push_back(file_name);
				}
			}
			file_name = dir->get_next();
		}
	} else {
		result["success"] = false;
		result["message"] = "Could not open directory: " + path;
		return result;
	}
	result["success"] = true;
	result["files"] = files;
	result["directories"] = dirs;
	return result;
}

Dictionary EditorTools::read_file_content(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("path")) {
		result["success"] = false;
		result["message"] = "Missing 'path' argument.";
		return result;
	}
	String path = p_args["path"];
	Error err;
	String content = FileAccess::get_file_as_string(path, &err);
	if (err != OK) {
		result["success"] = false;
		result["message"] = "Failed to read file: " + path;
		return result;
	}
	result["success"] = true;
	result["content"] = content;
	return result;
}

Dictionary EditorTools::read_file_advanced(const Dictionary &p_args) {
	Dictionary result;
	if (!p_args.has("path")) {
		result["success"] = false;
		result["message"] = "Missing 'path' argument.";
		return result;
	}
	String path = p_args["path"];
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		result["success"] = false;
		result["message"] = "Failed to open file: " + path;
		return result;
	}

	int start_line = p_args.has("start_line") ? (int)p_args["start_line"] : 1;
	int end_line = p_args.has("end_line") ? (int)p_args["end_line"] : -1;
	String content;
	int current_line = 1;

	while (!file->eof_reached() && (end_line == -1 || current_line <= end_line)) {
		String line = file->get_line();
		if (current_line >= start_line) {
			content += line + "\n";
		}
		current_line++;
	}

	result["success"] = true;
	result["content"] = content;
	return result;
}

Dictionary EditorTools::_predict_code_edit(const String &p_file_content, const String &p_prompt, const String &p_api_endpoint) {
	Dictionary result;
	HTTPClient *http_client = HTTPClient::create();

	// Prepare request
	String host = p_api_endpoint;
	int port = 80;
	bool use_ssl = false;

	if (host.begins_with("https://")) {
		host = host.trim_prefix("https://");
		use_ssl = true;
		port = 443;
	} else if (host.begins_with("http://")) {
		host = host.trim_prefix("http://");
	}

	String base_path = "/";
	if (host.find("/") != -1) {
		base_path = host.substr(host.find("/"), -1);
		host = host.substr(0, host.find("/"));
	}

	if (host.find(":") != -1) {
		port = host.substr(host.find(":") + 1, -1).to_int();
		host = host.substr(0, host.find(":"));
	}
	
	// Construct the full path by replacing /chat with /predict_code_edit
	String predict_path = base_path.replace("/chat", "/predict_code_edit");

	Error err = http_client->connect_to_host(host, port, use_ssl ? Ref<TLSOptions>() : Ref<TLSOptions>());
	if (err != OK) {
		result["success"] = false;
		result["message"] = "Failed to connect to host: " + host;
		memdelete(http_client);
		return result;
	}

	// Wait for connection
	while (http_client->get_status() == HTTPClient::STATUS_CONNECTING || http_client->get_status() == HTTPClient::STATUS_RESOLVING) {
		http_client->poll();
		OS::get_singleton()->delay_usec(1000);
	}

	if (http_client->get_status() != HTTPClient::STATUS_CONNECTED) {
		result["success"] = false;
		result["message"] = "Failed to connect to host after polling.";
		memdelete(http_client);
		return result;
	}

	// Prepare request body
	Dictionary request_data;
	request_data["file_content"] = p_file_content;
	request_data["prompt"] = p_prompt;

	print_line("DEBUG: Sending to backend - file_content length: " + itos(p_file_content.length()));
	print_line("DEBUG: Sending to backend - prompt: " + p_prompt);

	Ref<JSON> json;
	json.instantiate();
	String request_body_str = json->stringify(request_data);
	PackedByteArray request_body = request_body_str.to_utf8_buffer();

	print_line("DEBUG: Request body: " + request_body_str);

	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Content-Length: " + itos(request_body.size()));

	err = http_client->request(HTTPClient::METHOD_POST, predict_path, headers, request_body.ptr(), request_body.size());
	if (err != OK) {
		result["success"] = false;
		result["message"] = "HTTPClient->request failed.";
		memdelete(http_client);
		return result;
	}

	// Wait for response
	while (http_client->get_status() == HTTPClient::STATUS_REQUESTING) {
		http_client->poll();
		OS::get_singleton()->delay_usec(1000);
	}

	if (http_client->get_status() != HTTPClient::STATUS_BODY && http_client->get_status() != HTTPClient::STATUS_CONNECTED) {
		result["success"] = false;
		result["message"] = "Request failed after sending.";
		memdelete(http_client);
		return result;
	}

	if (!http_client->has_response()) {
		result["success"] = false;
		result["message"] = "Request completed, but no response received.";
		memdelete(http_client);
		return result;
	}

	int response_code = http_client->get_response_code();
	PackedByteArray body;

	while (http_client->get_status() == HTTPClient::STATUS_BODY) {
		http_client->poll();
		PackedByteArray chunk = http_client->read_response_body_chunk();
		if (chunk.size() == 0) {
			OS::get_singleton()->delay_usec(1000);
		} else {
			body.append_array(chunk);
		}
	}

	String response_str = String::utf8((const char *)body.ptr(), body.size());

	memdelete(http_client);

	if (response_code != 200) {
		result["success"] = false;
		result["message"] = "Prediction server returned error " + itos(response_code) + ": " + response_str;
		return result;
	}

	err = json->parse(response_str);
	if (err != OK) {
		result["success"] = false;
		result["message"] = "Failed to parse JSON response from prediction server.";
		return result;
	}

	Dictionary response_data = json->get_data();
	response_data["success"] = true;
	return response_data;
}

Dictionary EditorTools::apply_edit(const Dictionary &p_args) {
    Dictionary result;
    // This tool is now handled in AIChatDock, this is just a placeholder
    result["success"] = true;
    result["message"] = "This tool is handled in the AI Chat Dock.";
    return result;
} 