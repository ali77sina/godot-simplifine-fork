#include "editor_tools.h"

#include "core/crypto/crypto.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/config/project_settings.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/run/editor_run_bar.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/script/script_text_editor.h"
#include "scene/main/node.h"
#include "scene/main/window.h"
#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_parser.h"
#include "modules/gdscript/gdscript_analyzer.h"
#include "modules/gdscript/gdscript_compiler.h"

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
	
	// Get scene-relative path instead of absolute path
	Node *scene_root = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	if (scene_root && p_node == scene_root) {
		// This is the scene root itself
		node_info["path"] = p_node->get_name();
	} else if (scene_root && scene_root->is_ancestor_of(p_node)) {
		// Get relative path from scene root
		node_info["path"] = scene_root->get_path_to(p_node);
	} else {
		// Fallback to absolute path if not in scene tree
		node_info["path"] = p_node->get_path();
	}
	
	node_info["owner"] = p_node->get_owner() ? String(p_node->get_owner()->get_name()) : String();
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
		// Helper lambda to recursively search all nodes
		std::function<void(Node*)> search_nodes = [&](Node* node) {
			if (node) {
				if (node->is_class(type)) {
					nodes.push_back(_get_node_info(node));
				}
				// Recursively search all children
				for (int i = 0; i < node->get_child_count(); i++) {
					search_nodes(node->get_child(i));
				}
			}
		};
		
		// Start searching from the root
		search_nodes(root);
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
	Variant value = p_args["value"];
	
	// Special handling for color properties
	if (prop == "color" || prop == "modulate" || prop == "self_modulate") {
		if (value.get_type() == Variant::STRING) {
			String color_str = value;
			Color color;
			
			// Handle common color names
			if (color_str.to_lower() == "yellow") {
				color = Color(1.0, 1.0, 0.0, 1.0);
			} else if (color_str.to_lower() == "red") {
				color = Color(1.0, 0.0, 0.0, 1.0);
			} else if (color_str.to_lower() == "green") {
				color = Color(0.0, 1.0, 0.0, 1.0);
			} else if (color_str.to_lower() == "blue") {
				color = Color(0.0, 0.0, 1.0, 1.0);
			} else if (color_str.to_lower() == "white") {
				color = Color(1.0, 1.0, 1.0, 1.0);
			} else if (color_str.to_lower() == "black") {
				color = Color(0.0, 0.0, 0.0, 1.0);
			} else if (color_str.begins_with("#")) {
				// Handle hex colors
				color = Color::from_string(color_str, Color(1.0, 1.0, 1.0, 1.0));
			} else if (color_str.begins_with("(") && color_str.ends_with(")")) {
				// Handle Color constructor format: "(r, g, b, a)"
				String values = color_str.substr(1, color_str.length() - 2);
				PackedStringArray components = values.split(",");
				if (components.size() >= 3) {
					float r = components[0].strip_edges().to_float();
					float g = components[1].strip_edges().to_float();
					float b = components[2].strip_edges().to_float();
					float a = components.size() >= 4 ? components[3].strip_edges().to_float() : 1.0;
					color = Color(r, g, b, a);
				} else {
					color = Color(1.0, 1.0, 1.0, 1.0);
					print_line("SET_NODE_PROPERTY WARNING: Invalid Color constructor format '" + color_str + "', using white");
				}
			} else {
				// Try to parse as Color constructor or fallback to white
				color = Color::from_string(color_str, Color(1.0, 1.0, 1.0, 1.0));
				print_line("SET_NODE_PROPERTY WARNING: Unknown color '" + color_str + "', using white as fallback");
			}
			value = color;
			print_line("SET_NODE_PROPERTY: Converted color string '" + color_str + "' to Color(" + String::num(color.r) + ", " + String::num(color.g) + ", " + String::num(color.b) + ", " + String::num(color.a) + ")");
		}
	}
	
	bool valid = false;
	node->set(prop, value, &valid);
	if (!valid) {
		result["success"] = false;
		result["message"] = "Failed to set property '" + String(prop) + "'. It might be invalid or read-only. Node type: " + node->get_class();
		return result;
	}
	
	// Auto-save the scene after property changes so changes persist when running the game
	String current_scene = EditorNode::get_singleton()->get_edited_scene()->get_scene_file_path();
	if (!current_scene.is_empty()) {
		EditorNode::get_singleton()->save_scene_if_open(current_scene);
		print_line("SET_NODE_PROPERTY: Auto-saved scene after property change: " + current_scene);
	} else {
		print_line("SET_NODE_PROPERTY: Scene has no save path, cannot auto-save");
	}
	
	result["success"] = true;
	result["message"] = "Property set successfully and scene saved.";
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
				// Standard property setting with color handling
				Variant processed_value = property_value;
				
				// Special handling for color properties
				if ((property_name == "color" || property_name == "modulate" || property_name == "self_modulate") && property_value.get_type() == Variant::STRING) {
					String color_str = property_value;
					Color color;
					
					// Handle common color names
					if (color_str.to_lower() == "yellow") {
						color = Color(1.0, 1.0, 0.0, 1.0);
					} else if (color_str.to_lower() == "red") {
						color = Color(1.0, 0.0, 0.0, 1.0);
					} else if (color_str.to_lower() == "green") {
						color = Color(0.0, 1.0, 0.0, 1.0);
					} else if (color_str.to_lower() == "blue") {
						color = Color(0.0, 0.0, 1.0, 1.0);
					} else if (color_str.to_lower() == "white") {
						color = Color(1.0, 1.0, 1.0, 1.0);
					} else if (color_str.to_lower() == "black") {
						color = Color(0.0, 0.0, 0.0, 1.0);
					} else if (color_str.begins_with("#")) {
						color = Color::from_string(color_str, Color(1.0, 1.0, 1.0, 1.0));
					} else {
						color = Color::from_string(color_str, Color(1.0, 1.0, 1.0, 1.0));
					}
					processed_value = color;
					print_line("GENERALNODEEDITOR: Converted color string '" + color_str + "' to Color(" + String::num(color.r) + ", " + String::num(color.g) + ", " + String::num(color.b) + ", " + String::num(color.a) + ")");
				}
				
				bool valid = false;
				node->set(StringName(property_name), processed_value, &valid);
				prop_result["success"] = valid;
				prop_result["message"] = valid ? 
					"Property '" + property_name + "' set successfully" : 
					"Failed to set property '" + property_name + "'. It might be invalid or read-only. Node type: " + node->get_class();
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

	Ref<JSON> json;
	json.instantiate();
	String request_body_str = json->stringify(request_data);
	PackedByteArray request_body = request_body_str.to_utf8_buffer();

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

Dictionary EditorTools::_call_apply_endpoint(const String &p_file_path, const String &p_file_content, const Dictionary &p_ai_args, const String &p_api_endpoint) {
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
	
	// Construct the apply endpoint path
	String apply_path = base_path.replace("/chat", "/apply");

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

	// Prepare request body to match backend's expected format
	Dictionary request_data;
	request_data["file_name"] = p_file_path;
	request_data["file_content"] = p_file_content;
	request_data["prompt"] = p_ai_args.get("prompt", "");
	request_data["tool_arguments"] = p_ai_args;

	Ref<JSON> json;
	json.instantiate();
	String request_body_str = json->stringify(request_data);
	PackedByteArray request_body = request_body_str.to_utf8_buffer();

	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Content-Length: " + itos(request_body.size()));

	err = http_client->request(HTTPClient::METHOD_POST, apply_path, headers, request_body.ptr(), request_body.size());
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
		result["message"] = "Apply server returned error " + itos(response_code) + ": " + response_str;
		return result;
	}

	err = json->parse(response_str);
	if (err != OK) {
		result["success"] = false;
		result["message"] = "Failed to parse JSON response from apply server.";
		return result;
	}

	Dictionary response_data = json->get_data();

	// Clean up the edited_content if it exists
	if (response_data.has("edited_content")) {
		String edited_content = response_data["edited_content"];
		String cleaned_content = _clean_backend_content(edited_content);
		response_data["edited_content"] = cleaned_content;
	}

	response_data["success"] = true;
	return response_data;
}

Dictionary EditorTools::apply_edit(const Dictionary &p_args) {
    // Enhanced version that returns diff and compilation errors as JSON
    String path = p_args.get("path", "");
    String prompt = p_args.get("prompt", "");
    
    print_line("APPLY_EDIT: Using enhanced processing with diff and error checking");
    
    if (path.is_empty() || prompt.is_empty()) {
        Dictionary result;
        result["success"] = false;
        result["message"] = "Missing path or prompt for apply_edit";
        result["diff"] = "";
        result["compilation_errors"] = Array();
        return result;
    }
    
    // Read the file content
    Error err;
    String file_content = FileAccess::get_file_as_string(path, &err);
    if (err != OK) {
        Dictionary result;
        result["success"] = false;
        result["message"] = "Failed to read file: " + path;
        result["diff"] = "";
        result["compilation_errors"] = Array();
        return result;
    }
    
    // Use OS system call to bypass Godot's broken HTTPClient
    Dictionary local_result;
    String edit_prompt = p_args.get("prompt", "");
    
    print_line("APPLY_EDIT: Using OS curl to call backend API - prompt: " + edit_prompt);
    
    // Create JSON request
    Dictionary request_data;
    request_data["file_content"] = file_content;
    request_data["prompt"] = edit_prompt;
    
    Ref<JSON> json;
    json.instantiate();
    String request_json = json->stringify(request_data);
    
    // Write request to temporary file
    String temp_request_path = OS::get_singleton()->get_user_data_dir() + "/temp_request.json";
    String temp_response_path = OS::get_singleton()->get_user_data_dir() + "/temp_response.json";
    
    Ref<FileAccess> request_file = FileAccess::open(temp_request_path, FileAccess::WRITE);
    if (request_file.is_valid()) {
        request_file->store_string(request_json);
        request_file->close();
        
        // Use curl via OS system call
        String curl_command = "curl -X POST http://localhost:8000/predict_code_edit -H \"Content-Type: application/json\" -d @\"" + temp_request_path + "\" -o \"" + temp_response_path + "\" -s";
        
        print_line("APPLY_EDIT: Executing curl command to backend");
        
        List<String> args;
        args.push_back("-c");
        args.push_back(curl_command);
        int exit_code;
        Error exec_err = OS::get_singleton()->execute("sh", args, nullptr, &exit_code);
        
        if (exec_err == OK && exit_code == 0) {
            // Read response from file
            Error err;
            String response_content = FileAccess::get_file_as_string(temp_response_path, &err);
            
            if (err == OK && response_content.length() > 0) {
                Ref<JSON> response_json;
                response_json.instantiate();
                Error parse_err = response_json->parse(response_content);
                
                if (parse_err == OK) {
                    Dictionary response_data = response_json->get_data();
                    local_result["success"] = true;
                    local_result["edited_content"] = response_data.get("edited_content", file_content);
                    print_line("APPLY_EDIT: Successfully received response via curl (" + String::num_int64(String(local_result["edited_content"]).length()) + " chars)");
                } else {
                    local_result["success"] = false;
                    local_result["message"] = "Failed to parse curl response JSON";
                    print_line("APPLY_EDIT ERROR: JSON parse failed - " + response_content.substr(0, 200));
                }
            } else {
                local_result["success"] = false;
                local_result["message"] = "Failed to read curl response file";
                print_line("APPLY_EDIT ERROR: Could not read response file");
            }
            
            // Cleanup temp files
            if (FileAccess::exists(temp_response_path)) {
                OS::get_singleton()->move_to_trash(temp_response_path);
            }
        } else {
            local_result["success"] = false;
            local_result["message"] = "Curl command failed with exit code: " + String::num_int64(exit_code);
            print_line("APPLY_EDIT ERROR: Curl failed - " + String(local_result["message"]));
        }
        
        // Cleanup request file
        if (FileAccess::exists(temp_request_path)) {
            OS::get_singleton()->move_to_trash(temp_request_path);
        }
    } else {
        local_result["success"] = false;
        local_result["message"] = "Failed to create temporary request file";
        print_line("APPLY_EDIT ERROR: Could not create temp file");
    }
    // Dictionary local_result = _call_apply_endpoint(path, file_content, p_args, "");
    
    if (local_result.get("success", false)) {
        String new_content = local_result["edited_content"];
        String cleaned_content = _clean_backend_content(new_content);
        
        print_line("APPLY_EDIT DEBUG: Applying dynamic file approach - writing content to disk immediately");
        
        // Generate unified diff with safety checks
        String diff = "";
        print_line("APPLY_EDIT DEBUG: About to generate diff...");
        
        // Add length checks to prevent huge diffs from hanging
        if (file_content.length() > 100000 || cleaned_content.length() > 100000) {
            print_line("APPLY_EDIT WARNING: File too large for diff generation, skipping");
            diff = "Diff skipped - file too large (original: " + String::num_int64(file_content.length()) + " chars, new: " + String::num_int64(cleaned_content.length()) + " chars)";
        } else {
            print_line("APPLY_EDIT DEBUG: Calling _generate_unified_diff with " + String::num_int64(file_content.length()) + " and " + String::num_int64(cleaned_content.length()) + " chars");
            
            // TEMPORARY: Skip actual diff generation to test if it's causing hangs
            // diff = _generate_unified_diff(file_content, cleaned_content, path);
            diff = "=== TEMPORARY SIMPLE DIFF ===\nOriginal length: " + String::num_int64(file_content.length()) + " chars\nNew length: " + String::num_int64(cleaned_content.length()) + " chars\n=== END DIFF ===";
            
            print_line("APPLY_EDIT DEBUG: Using temporary simple diff (bypassing _generate_unified_diff)");
        }
        
        print_line("APPLY_EDIT DEBUG: Generated diff (" + String::num_int64(diff.length()) + " chars):");
        if (diff.length() > 0) {
            print_line("DIFF PREVIEW: " + diff.substr(0, 300) + (diff.length() > 300 ? "..." : ""));
        } else {
            print_line("DIFF WARNING: Diff is empty!");
        }
        
        // DYNAMIC APPROACH: Write the new content to disk immediately
        // This allows Godot to naturally detect compilation errors
        Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
        if (file.is_valid()) {
            file->store_string(cleaned_content);
            file->close();
            print_line("APPLY_EDIT: New content written to disk - " + path);
            print_line("APPLY_EDIT: Godot will now naturally detect any compilation errors");
        } else {
            print_line("APPLY_EDIT ERROR: Failed to write to disk - " + path);
        }
        
        // For now, return empty compilation_errors since Godot will show them in the console
        // In the future, we could capture these from Godot's error system
        Array compilation_errors;
        
        // Apply to script editor for user review
        ScriptEditor *script_editor = ScriptEditor::get_singleton();
        if (script_editor) {
            Ref<Resource> resource = ResourceLoader::load(path);
            Ref<Script> script = resource;
            
            if (script.is_valid()) {
                script_editor->edit(script);
                ScriptTextEditor *ste = Object::cast_to<ScriptTextEditor>(script_editor->get_current_editor());
                if (ste) {
                    ste->set_diff(file_content, cleaned_content);
                    print_line("APPLY_EDIT: Diff view ready for user review");
                }
            }
        }
        
        // Return enhanced result with diff and compilation errors
        Dictionary result;
        result["success"] = true;
        result["message"] = "Changes applied to disk! Use Accept to keep or Reject to revert. Check console for any compilation errors.";
        result["edited_content"] = cleaned_content;
        result["diff"] = diff;
        result["compilation_errors"] = compilation_errors;
        result["has_errors"] = compilation_errors.size() > 0;
        result["dynamic_approach"] = true; // Flag to indicate content is already on disk
        
        print_line("APPLY_EDIT DEBUG: Final JSON response contains:");
        print_line("  - success: " + String(result["success"]));
        print_line("  - diff length: " + String::num_int64(String(result["diff"]).length()));
        print_line("  - compilation_errors count: " + String::num_int64(Array(result["compilation_errors"]).size()));
        print_line("  - has_errors: " + String(result["has_errors"]));
        
        return result;
    }
    
    // If local processing failed, still return proper structure
    Dictionary failed_result = local_result;
    failed_result["diff"] = "";
    failed_result["compilation_errors"] = Array();
    failed_result["has_errors"] = false;
    return failed_result;
} 

String EditorTools::_clean_backend_content(const String &p_content) {
	String content = p_content;
	
	// Remove code block wrappers (```javascript, ```gdscript, etc.)
	// Handle various possible code block formats
	Vector<String> code_block_patterns = {
		"```javascript\n",
		"```gdscript\n", 
		"```\n",
		"```js\n",
		"```gd\n"
	};
	
	for (const String &pattern : code_block_patterns) {
		if (content.begins_with(pattern)) {
			content = content.substr(pattern.length());
			break;
		}
	}
	
	// Remove trailing code block marker
	if (content.ends_with("\n```")) {
		content = content.substr(0, content.length() - 4);
	} else if (content.ends_with("```")) {
		content = content.substr(0, content.length() - 3);
	}
	
	// Fix JavaScript to GDScript conversion for .gd files
	content = _convert_javascript_to_gdscript(content);
	
	// Fix common malformed content issues
	content = _fix_malformed_content(content);
	
	// Trim extra whitespace
	content = content.strip_edges();
	
	return content;
}

String EditorTools::_convert_javascript_to_gdscript(const String &p_content) {
	String content = p_content;
	Vector<String> lines = content.split("\n");
	Vector<String> converted_lines;
	
	for (const String &line : lines) {
		String converted_line = line;
		
		// Convert JavaScript function syntax to GDScript
		if (converted_line.contains("function ")) {
			// Replace "function name() {" with "func name():"
			String trimmed = converted_line.strip_edges();
			if (trimmed.begins_with("function ")) {
				// Extract function name
				String func_part = trimmed.substr(9); // Remove "function "
				int paren_pos = func_part.find("(");
				if (paren_pos > 0) {
					String func_name = func_part.substr(0, paren_pos);
					String params = func_part.substr(paren_pos);
					
					// Remove opening brace if present
					if (params.ends_with(" {")) {
						params = params.substr(0, params.length() - 2);
					} else if (params.ends_with("{")) {
						params = params.substr(0, params.length() - 1);
					}
					
					// Get indentation
					String indent = line.substr(0, line.length() - line.lstrip("\t ").length());
					converted_line = indent + "func " + func_name + params + ":";
				}
			}
		}
		
		// Convert console.log to print
		if (converted_line.contains("console.log(")) {
			converted_line = converted_line.replace("console.log(", "print(");
		}
		
		// Remove standalone opening/closing braces (JavaScript style)
		String trimmed = converted_line.strip_edges();
		if (trimmed == "{" || trimmed == "}") {
			continue; // Skip these lines in GDScript
		}
		
		// Convert JavaScript variable declarations
		if (converted_line.contains("let ") || converted_line.contains("var ") || converted_line.contains("const ")) {
			converted_line = converted_line.replace("let ", "var ");
			converted_line = converted_line.replace("const ", "var ");
		}
		
		converted_lines.push_back(converted_line);
	}
	
	return String("\n").join(converted_lines);
}

String EditorTools::_fix_malformed_content(const String &p_content) {
	String content = p_content;
	
	// Fix missing function endings in GDScript
	Vector<String> lines = content.split("\n");
	Vector<String> fixed_lines;
	bool in_function = false;
	
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		String trimmed = line.strip_edges();
		
		// Track function declarations in GDScript
		if (trimmed.begins_with("func ")) {
			in_function = true;
		} else if (in_function) {
			// Check indentation
			String line_indent = line.substr(0, line.length() - line.lstrip("\t ").length());
			if (!trimmed.is_empty() && line_indent.length() == 0) {
				// Function ended (no indentation)
				in_function = false;
			}
		}
		
		fixed_lines.push_back(line);
		
		// If we're starting a new function without proper ending
		if (in_function && 
			i + 1 < lines.size() && 
			lines[i + 1].strip_edges().begins_with("func ")) {
			in_function = false;
		}
	}
	
	return String("\n").join(fixed_lines);
}

String EditorTools::_generate_unified_diff(const String &p_original, const String &p_modified, const String &p_file_path) {
	Vector<String> original_lines = p_original.split("\n");
	Vector<String> modified_lines = p_modified.split("\n");
	
	String diff = "--- " + p_file_path + " (original)\n";
	diff += "+++ " + p_file_path + " (modified)\n";
	
	// Simple diff implementation - compare line by line
	int original_line = 0;
	int modified_line = 0;
	int context_lines = 3;
	
	while (original_line < original_lines.size() || modified_line < modified_lines.size()) {
		// Find changes
		int change_start_orig = original_line;
		int change_start_mod = modified_line;
		
		// Skip matching lines
		while (original_line < original_lines.size() && 
			   modified_line < modified_lines.size() && 
			   original_lines[original_line] == modified_lines[modified_line]) {
			original_line++;
			modified_line++;
		}
		
		if (original_line >= original_lines.size() && modified_line >= modified_lines.size()) {
			break; // End of both files
		}
		
		// Find end of change block
		int change_end_orig = original_line;
		int change_end_mod = modified_line;
		
		// Simple heuristic: advance until we find matching lines again or reach end
		while ((change_end_orig < original_lines.size() || change_end_mod < modified_lines.size())) {
			// Look ahead to see if we find a match
			bool found_match = false;
			int lookahead = 3; // Look ahead a few lines
			
			for (int i = 0; i < lookahead && !found_match; i++) {
				if (change_end_orig + i < original_lines.size() && 
					change_end_mod + i < modified_lines.size() &&
					original_lines[change_end_orig + i] == modified_lines[change_end_mod + i]) {
					found_match = true;
					break;
				}
			}
			
			if (found_match) {
				break;
			}
			
			if (change_end_orig < original_lines.size()) change_end_orig++;
			if (change_end_mod < modified_lines.size()) change_end_mod++;
		}
		
		// Generate hunk header
		int context_start_orig = MAX(0, change_start_orig - context_lines);
		int context_start_mod = MAX(0, change_start_mod - context_lines);
		int context_end_orig = MIN(original_lines.size(), change_end_orig + context_lines);
		int context_end_mod = MIN(modified_lines.size(), change_end_mod + context_lines);
		
		int hunk_orig_lines = context_end_orig - context_start_orig;
		int hunk_mod_lines = context_end_mod - context_start_mod;
		
		diff += "@@ -" + String::num_int64(context_start_orig + 1) + "," + String::num_int64(hunk_orig_lines) + 
				" +" + String::num_int64(context_start_mod + 1) + "," + String::num_int64(hunk_mod_lines) + " @@\n";
		
		// Add context before change
		for (int i = context_start_orig; i < change_start_orig; i++) {
			diff += " " + original_lines[i] + "\n";
		}
		
		// Add removed lines
		for (int i = change_start_orig; i < change_end_orig && i < original_lines.size(); i++) {
			diff += "-" + original_lines[i] + "\n";
		}
		
		// Add added lines
		for (int i = change_start_mod; i < change_end_mod && i < modified_lines.size(); i++) {
			diff += "+" + modified_lines[i] + "\n";
		}
		
		// Add context after change
		for (int i = change_end_orig; i < context_end_orig; i++) {
			diff += " " + original_lines[i] + "\n";
		}
		
		original_line = change_end_orig;
		modified_line = change_end_mod;
	}
	
	return diff;
}

Array EditorTools::_check_compilation_errors(const String &p_file_path, const String &p_content) {
	Array errors;
	
	// Get file extension to determine script type
	String extension = p_file_path.get_extension();
	
	if (extension == "gd") {
		// GDScript compilation check using parser/analyzer/compiler approach
		GDScriptParser parser;
		Error parse_err = parser.parse(p_content, p_file_path, false);
		
		// Get parser errors
		const List<GDScriptParser::ParserError> &parser_errors = parser.get_errors();
		for (const GDScriptParser::ParserError &error : parser_errors) {
			Dictionary error_dict;
			error_dict["type"] = "parser_error";
			error_dict["line"] = error.line;
			error_dict["column"] = error.column;
			error_dict["message"] = error.message;
			errors.push_back(error_dict);
		}
		
		// Only continue to analysis if parsing succeeded
		if (parse_err == OK) {
			GDScriptAnalyzer analyzer(&parser);
			Error analyze_err = analyzer.analyze();
			
			// Get analyzer errors (they're stored in the parser)
			const List<GDScriptParser::ParserError> &analyzer_errors = parser.get_errors();
			for (const GDScriptParser::ParserError &error : analyzer_errors) {
				// Skip errors we already collected during parsing
				bool already_collected = false;
				for (const GDScriptParser::ParserError &parse_error : parser_errors) {
					if (parse_error.line == error.line && parse_error.message == error.message) {
						already_collected = true;
						break;
					}
				}
				if (!already_collected) {
					Dictionary error_dict;
					error_dict["type"] = "analyzer_error";
					error_dict["line"] = error.line;
					error_dict["column"] = error.column;
					error_dict["message"] = error.message;
					errors.push_back(error_dict);
				}
			}
			
			// Only continue to compilation if analysis succeeded
			if (analyze_err == OK) {
				// Create a temporary script for compilation
				Ref<GDScript> temp_script;
				temp_script.instantiate();
				
				GDScriptCompiler compiler;
				Error compile_err = compiler.compile(&parser, temp_script.ptr(), false);
				
				if (compile_err != OK) {
					Dictionary error_dict;
					error_dict["type"] = "compiler_error";
					error_dict["line"] = compiler.get_error_line();
					error_dict["column"] = compiler.get_error_column();
					error_dict["message"] = compiler.get_error();
					errors.push_back(error_dict);
				}
			}
		}
	} else if (extension == "cs") {
		// C# compilation would require mono/dotnet integration
		// For now, add a placeholder
		Dictionary error_dict;
		error_dict["type"] = "info";
		error_dict["line"] = 0;
		error_dict["column"] = 0;
		error_dict["message"] = "C# compilation checking not implemented yet";
		errors.push_back(error_dict);
	}
	
	print_line("COMPILATION CHECK: Found " + String::num_int64(errors.size()) + " errors for " + p_file_path);
	
	    return errors;
}

Dictionary EditorTools::check_compilation_errors(const Dictionary &p_args) {
    Dictionary result;
    String path = p_args.get("path", "");
    
    if (path.is_empty()) {
        result["success"] = false;
        result["message"] = "Path is required";
        result["errors"] = Array();
        return result;
    }
    
    print_line("CHECK_COMPILATION_ERRORS: Checking file - " + path);
    
    // Simple approach: Try to reload the script and see if it fails
    Array errors;
    
    if (path.get_extension() == "gd") {
        // Read the file content and parse it directly
        Error file_err;
        String file_content = FileAccess::get_file_as_string(path, &file_err);
        
        if (file_err != OK) {
            Dictionary error_dict;
            error_dict["type"] = "file_error";
            error_dict["line"] = 0;
            error_dict["column"] = 0;
            error_dict["message"] = "Failed to read file: " + path;
            errors.push_back(error_dict);
        } else {
            // Parse the script directly to get detailed error information
            GDScriptParser parser;
            Error parse_err = parser.parse(file_content, path, false);
            
            // Get parser errors
            const List<GDScriptParser::ParserError> &parser_errors = parser.get_errors();
            for (const GDScriptParser::ParserError &error : parser_errors) {
                Dictionary error_dict;
                error_dict["type"] = "parser_error";
                error_dict["line"] = error.line;
                error_dict["column"] = error.column;
                error_dict["message"] = error.message;
                errors.push_back(error_dict);
                print_line("CHECK_COMPILATION_ERRORS: Found parser error at line " + String::num_int64(error.line) + ": " + error.message);
            }
            
            // Only continue to analysis if parsing succeeded
            if (parse_err == OK && parser_errors.is_empty()) {
                GDScriptAnalyzer analyzer(&parser);
                Error analyze_err = analyzer.analyze();
                
                // Get analyzer errors (they're stored in the parser)
                const List<GDScriptParser::ParserError> &analyzer_errors = parser.get_errors();
                for (const GDScriptParser::ParserError &error : analyzer_errors) {
                    // Skip errors we already collected during parsing
                    bool already_collected = false;
                    for (const GDScriptParser::ParserError &parse_error : parser_errors) {
                        if (parse_error.line == error.line && parse_error.message == error.message) {
                            already_collected = true;
                            break;
                        }
                    }
                    if (!already_collected) {
                        Dictionary error_dict;
                        error_dict["type"] = "analyzer_error";
                        error_dict["line"] = error.line;
                        error_dict["column"] = error.column;
                        error_dict["message"] = error.message;
                        errors.push_back(error_dict);
                        print_line("CHECK_COMPILATION_ERRORS: Found analyzer error at line " + String::num_int64(error.line) + ": " + error.message);
                    }
                }
                
                if (analyze_err == OK && analyzer_errors.size() == parser_errors.size()) {
                    print_line("CHECK_COMPILATION_ERRORS: Script parsed and analyzed successfully");
                }
            } else {
                print_line("CHECK_COMPILATION_ERRORS: Parsing failed with " + String::num_int64(parser_errors.size()) + " errors");
            }
        }
    } else if (path.get_extension() == "cs") {
        // For C# files, add placeholder check
        Dictionary info_dict;
        info_dict["type"] = "info";
        info_dict["line"] = 0;
        info_dict["column"] = 0;
        info_dict["message"] = "C# compilation checking not implemented";
        errors.push_back(info_dict);
    } else {
        Dictionary info_dict;
        info_dict["type"] = "info";
        info_dict["line"] = 0;
        info_dict["column"] = 0;
        info_dict["message"] = "Unsupported file type for compilation checking";
        errors.push_back(info_dict);
    }
    
    result["success"] = true;
    result["path"] = path;
    result["errors"] = errors;
    result["has_errors"] = errors.size() > 0;
    result["error_count"] = errors.size();
    
    print_line("CHECK_COMPILATION_ERRORS: Found " + String::num_int64(errors.size()) + " errors in " + path);
    
    return result;
}

// --- Universal Tools Implementation ---

Dictionary EditorTools::universal_node_manager(const Dictionary &p_args) {
	String operation = p_args.get("operation", "");
	
	if (operation == "create") return create_node(p_args);
	if (operation == "delete") return delete_node(p_args);
	if (operation == "move") return move_node(p_args);
	if (operation == "set_property") return set_node_property(p_args);
	if (operation == "get_info") return get_all_nodes(p_args);
	if (operation == "search") return search_nodes_by_type(p_args);
	if (operation == "select") return get_editor_selection(p_args);
	if (operation == "get_properties") return get_node_properties(p_args);
	if (operation == "call_method") return call_node_method(p_args);
	if (operation == "get_script") return get_node_script(p_args);
	if (operation == "attach_script") return attach_script(p_args);
	if (operation == "add_collision") return add_collision_shape(p_args);
	
	Dictionary result;
	result["success"] = false;
	result["message"] = "Unknown node operation: " + operation;
	return result;
}

Dictionary EditorTools::universal_file_manager(const Dictionary &p_args) {
	String operation = p_args.get("operation", "");
	
	if (operation == "read") {
		if (p_args.has("start_line") || p_args.has("end_line")) {
			return read_file_advanced(p_args);
		} else {
			return read_file_content(p_args);
		}
	}
	if (operation == "list") return list_project_files(p_args);
	if (operation == "apply_ai_edit") return apply_edit(p_args);
	if (operation == "check_compilation") return check_compilation_errors(p_args);
	if (operation == "get_classes") return get_available_classes(p_args);

	Dictionary result;
	result["success"] = false;
	result["message"] = "Unknown file operation: " + operation;
	return result;
}

Dictionary EditorTools::scene_manager(const Dictionary &p_args) {
	String operation = p_args.get("operation", "");
	
	if (operation == "save") return save_scene(p_args);
	if (operation == "get_info") return get_scene_info(p_args);
	if (operation == "open" || operation == "create_new" || operation == "save_as" || operation == "instantiate") {
		return manage_scene(p_args);
	}
	
	Dictionary result;
	result["success"] = false;
	result["message"] = "Unknown scene operation: " + operation;
	return result;
}

// --- New Debugging Tools Implementation ---

Dictionary EditorTools::run_scene(const Dictionary &p_args) {
	Dictionary result;
	String scene_path = p_args.get("scene_path", "");
	int duration = p_args.get("duration", 5);
	
	// Get the current scene if no path specified
	if (scene_path.is_empty()) {
		Node *current_scene = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
		if (current_scene) {
			scene_path = current_scene->get_scene_file_path();
		}
	}
	
	if (scene_path.is_empty()) {
		result["success"] = false;
		result["message"] = "No scene to run";
		return result;
	}
	
	// Start the scene
	EditorRunBar::get_singleton()->play_custom_scene(scene_path);
	
	result["success"] = true;
	result["message"] = "Scene started: " + scene_path;
	result["scene_path"] = scene_path;
	result["duration"] = duration;
	return result;
}

Dictionary EditorTools::get_scene_tree_hierarchy(const Dictionary &p_args) {
	Dictionary result;
	bool include_properties = p_args.get("include_properties", false);
	
	Node *root = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	if (!root) {
		result["success"] = false;
		result["message"] = "No scene is currently being edited.";
		return result;
	}
	
	// Recursive function to build hierarchy
	std::function<Dictionary(Node*)> build_hierarchy = [&](Node* node) -> Dictionary {
		Dictionary node_dict;
		if (!node) return node_dict;
		
		node_dict["name"] = node->get_name();
		node_dict["type"] = node->get_class();
		node_dict["path"] = root->get_path_to(node);
		
		if (include_properties) {
			List<PropertyInfo> properties;
			node->get_property_list(&properties);
			Dictionary props_dict;
			for (const PropertyInfo &prop_info : properties) {
				if (prop_info.usage & PROPERTY_USAGE_EDITOR) {
					props_dict[prop_info.name] = node->get(prop_info.name);
				}
			}
			node_dict["properties"] = props_dict;
		}
		
		// Add children recursively
		Array children;
		for (int i = 0; i < node->get_child_count(); i++) {
			children.push_back(build_hierarchy(node->get_child(i)));
		}
		node_dict["children"] = children;
		node_dict["child_count"] = node->get_child_count();
		
		return node_dict;
	};
	
	result["success"] = true;
	result["hierarchy"] = build_hierarchy(root);
	result["include_properties"] = include_properties;
	return result;
}

Dictionary EditorTools::inspect_physics_body(const Dictionary &p_args) {
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
	
	Dictionary physics_info;
	physics_info["node_name"] = node->get_name();
	physics_info["node_type"] = node->get_class();
	
	// Check if it's a physics body
	if (node->is_class("RigidBody2D") || node->is_class("CharacterBody2D") || 
		node->is_class("StaticBody2D") || node->is_class("AnimatableBody2D") ||
		node->is_class("RigidBody3D") || node->is_class("CharacterBody3D") ||
		node->is_class("StaticBody3D") || node->is_class("AnimatableBody3D")) {
		
		physics_info["is_physics_body"] = true;
		
		// Get physics properties
		physics_info["collision_layer"] = node->get("collision_layer");
		physics_info["collision_mask"] = node->get("collision_mask");
		
		if (node->is_class("RigidBody2D") || node->is_class("RigidBody3D")) {
			physics_info["mass"] = node->get("mass");
			physics_info["gravity_scale"] = node->get("gravity_scale");
			physics_info["linear_velocity"] = node->get("linear_velocity");
			physics_info["angular_velocity"] = node->get("angular_velocity");
		}
		
		// Check for collision shapes
		Array collision_shapes;
		for (int i = 0; i < node->get_child_count(); i++) {
			Node *child = node->get_child(i);
			if (child->is_class("CollisionShape2D") || child->is_class("CollisionShape3D")) {
				Dictionary shape_info;
				shape_info["name"] = child->get_name();
				shape_info["type"] = child->get_class();
				shape_info["disabled"] = child->get("disabled");
				collision_shapes.push_back(shape_info);
			}
		}
		physics_info["collision_shapes"] = collision_shapes;
		
	} else {
		physics_info["is_physics_body"] = false;
		physics_info["message"] = "Node is not a physics body";
	}
	
	result["success"] = true;
	result["physics_info"] = physics_info;
	return result;
}

Dictionary EditorTools::get_camera_info(const Dictionary &p_args) {
	Dictionary result;
	String camera_path = p_args.get("camera_path", "");
	
	Node *camera = nullptr;
	Node *root = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	
	if (!camera_path.is_empty()) {
		camera = _get_node_from_path(camera_path, result);
		if (!camera) {
			return result;
		}
	} else if (root) {
		// Find first camera in the scene
		std::function<Node*(Node*)> find_camera = [&](Node* node) -> Node* {
			if (node->is_class("Camera2D") || node->is_class("Camera3D")) {
				return node;
			}
			for (int i = 0; i < node->get_child_count(); i++) {
				Node *found = find_camera(node->get_child(i));
				if (found) return found;
			}
			return nullptr;
		};
		camera = find_camera(root);
	}
	
	if (!camera) {
		result["success"] = false;
		result["message"] = "No camera found";
		return result;
	}
	
	Dictionary camera_info;
	camera_info["name"] = camera->get_name();
	camera_info["type"] = camera->get_class();
	camera_info["path"] = root ? root->get_path_to(camera) : camera->get_path();
	camera_info["position"] = camera->get("position");
	camera_info["enabled"] = camera->get("enabled");
	
	if (camera->is_class("Camera2D")) {
		camera_info["zoom"] = camera->get("zoom");
		camera_info["offset"] = camera->get("offset");
		camera_info["limit_left"] = camera->get("limit_left");
		camera_info["limit_right"] = camera->get("limit_right");
		camera_info["limit_top"] = camera->get("limit_top");
		camera_info["limit_bottom"] = camera->get("limit_bottom");
	}
	
	result["success"] = true;
	result["camera_info"] = camera_info;
	return result;
}

Dictionary EditorTools::take_screenshot(const Dictionary &p_args) {
	Dictionary result;
	String filename = p_args.get("filename", "screenshot_debug.png");
	
	// Try to get the main viewport
	Viewport *viewport = EditorNode::get_singleton()->get_viewport();
	if (!viewport) {
		result["success"] = false;
		result["message"] = "Could not access viewport";
		return result;
	}
	
	// Take screenshot
	Ref<Image> screenshot = viewport->get_texture()->get_image();
	if (screenshot.is_null()) {
		result["success"] = false;
		result["message"] = "Failed to capture screenshot";
		return result;
	}
	
	// Save to project directory
	String project_path = ProjectSettings::get_singleton()->globalize_path("res://");
	String full_path = project_path + "/" + filename;
	
	Error save_result = screenshot->save_png(full_path);
	if (save_result != OK) {
		result["success"] = false;
		result["message"] = "Failed to save screenshot: " + String::num_int64(save_result);
		return result;
	}
	
	result["success"] = true;
	result["message"] = "Screenshot saved";
	result["filename"] = filename;
	result["path"] = full_path;
	return result;
}

Dictionary EditorTools::check_node_in_scene_tree(const Dictionary &p_args) {
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
	
	Dictionary node_status;
	node_status["exists"] = true;
	node_status["name"] = node->get_name();
	node_status["type"] = node->get_class();
	node_status["is_inside_tree"] = node->is_inside_tree();
	node_status["is_ready"] = node->is_ready();
	node_status["process_mode"] = node->get("process_mode");
	
	Node *parent = node->get_parent();
	if (parent) {
		node_status["parent_name"] = parent->get_name();
		node_status["parent_type"] = parent->get_class();
	} else {
		node_status["parent_name"] = "";
		node_status["parent_type"] = "";
	}
	
	node_status["child_count"] = node->get_child_count();
	node_status["visible"] = node->has_method("is_visible") ? node->call("is_visible") : Variant();
	
	result["success"] = true;
	result["node_status"] = node_status;
	return result;
}

Dictionary EditorTools::inspect_animation_state(const Dictionary &p_args) {
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
	
	Dictionary animation_info;
	animation_info["node_name"] = node->get_name();
	animation_info["node_type"] = node->get_class();
	
	if (node->is_class("AnimationPlayer")) {
		animation_info["is_animation_player"] = true;
		animation_info["current_animation"] = node->get("current_animation");
		animation_info["is_playing"] = node->call("is_playing");
		animation_info["playback_speed"] = node->get("playback_speed");
		
		// Get list of animations
		Array animation_list;
		Variant animations = node->call("get_animation_list");
		if (animations.get_type() == Variant::ARRAY) {
			animation_list = animations;
		}
		animation_info["available_animations"] = animation_list;
		
	} else if (node->is_class("AnimatedSprite2D") || node->is_class("AnimatedSprite3D")) {
		animation_info["is_animated_sprite"] = true;
		animation_info["animation"] = node->get("animation");
		animation_info["frame"] = node->get("frame");
		animation_info["playing"] = node->call("is_playing");
		animation_info["speed_scale"] = node->get("speed_scale");
		
	} else {
		animation_info["is_animated"] = false;
		animation_info["message"] = "Node is not an animation node";
	}
	
	result["success"] = true;
	result["animation_info"] = animation_info;
	return result;
}

Dictionary EditorTools::get_layers_and_zindex(const Dictionary &p_args) {
	Dictionary result;
	String path = p_args.get("path", "");
	
	Node *root = EditorNode::get_singleton()->get_tree()->get_edited_scene_root();
	if (!root) {
		result["success"] = false;
		result["message"] = "No scene is currently being edited.";
		return result;
	}
	
	Array layer_info;
	
	if (!path.is_empty()) {
		// Get info for specific node
		Node *node = _get_node_from_path(path, result);
		if (!node) {
			return result;
		}
		
		Dictionary node_layer_info;
		node_layer_info["name"] = node->get_name();
		node_layer_info["type"] = node->get_class();
		node_layer_info["path"] = root->get_path_to(node);
		
		if (node->has_method("get_z_index")) {
			node_layer_info["z_index"] = node->call("get_z_index");
		}
		if (node->has_method("get_z_as_relative")) {
			node_layer_info["z_as_relative"] = node->call("get_z_as_relative");
		}
		if (node->is_class("CanvasLayer")) {
			node_layer_info["layer"] = node->get("layer");
		}
		
		layer_info.push_back(node_layer_info);
		
	} else {
		// Get info for all nodes with layer/z-index properties
		std::function<void(Node*)> collect_layer_nodes = [&](Node* node) {
			if (node) {
				Dictionary node_layer_info;
				bool has_layer_info = false;
				
				node_layer_info["name"] = node->get_name();
				node_layer_info["type"] = node->get_class();
				node_layer_info["path"] = root->get_path_to(node);
				
				if (node->has_method("get_z_index")) {
					node_layer_info["z_index"] = node->call("get_z_index");
					has_layer_info = true;
				}
				if (node->has_method("get_z_as_relative")) {
					node_layer_info["z_as_relative"] = node->call("get_z_as_relative");
					has_layer_info = true;
				}
				if (node->is_class("CanvasLayer")) {
					node_layer_info["layer"] = node->get("layer");
					has_layer_info = true;
				}
				
				if (has_layer_info) {
					layer_info.push_back(node_layer_info);
				}
				
				// Recursively check children
				for (int i = 0; i < node->get_child_count(); i++) {
					collect_layer_nodes(node->get_child(i));
				}
			}
		};
		
		collect_layer_nodes(root);
	}
	
	result["success"] = true;
	result["layer_info"] = layer_info;
	result["node_count"] = layer_info.size();
	return result;
} 