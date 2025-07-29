/**************************************************************************/
/*  ai_chat_dock.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "ai_chat_dock.h"

#include "core/io/config_file.h"
#include "core/io/json.h"
#include "core/os/time.h"
#include "core/io/file_access.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"
#include "scene/main/http_request.h"
#include "scene/resources/style_box_flat.h"

#include "../ai/editor_tools.h"
#include "diff_viewer.h"

void AIChatDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_send_button_pressed"), &AIChatDock::_on_send_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_input_text_changed"), &AIChatDock::_on_input_text_changed);
	ClassDB::bind_method(D_METHOD("_on_input_field_gui_input"), &AIChatDock::_on_input_field_gui_input);
	ClassDB::bind_method(D_METHOD("_on_model_selected"), &AIChatDock::_on_model_selected);
	ClassDB::bind_method(D_METHOD("_on_tool_output_toggled"), &AIChatDock::_on_tool_output_toggled);
	ClassDB::bind_method(D_METHOD("_on_tool_file_link_pressed", "path"), &AIChatDock::_on_tool_file_link_pressed);
}

void AIChatDock::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_POST_ENTER_TREE: {
			// UI setup moved from constructor to here to ensure theme is available.
			// Model selection at the top - following Godot dock patterns
			HBoxContainer *model_container = memnew(HBoxContainer);
			add_child(model_container);

			Label *model_label = memnew(Label);
			model_label->set_text("Model:");
			model_container->add_child(model_label);

			model_dropdown = memnew(OptionButton);
			model_dropdown->add_item("gpt-4o");
			model_dropdown->add_item("gpt-4-turbo");
			model_dropdown->add_item("gpt-3.5-turbo");
			model_dropdown->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			model_dropdown->connect("item_selected", callable_mp(this, &AIChatDock::_on_model_selected));
			model_container->add_child(model_dropdown);

			// Chat history area - expand to fill available space
			chat_scroll = memnew(ScrollContainer);
			chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			chat_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
			add_child(chat_scroll);

			chat_container = memnew(VBoxContainer);
			chat_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			chat_scroll->add_child(chat_container);

			// Input area at the bottom
			HBoxContainer *input_container = memnew(HBoxContainer);
			add_child(input_container);

			input_field = memnew(TextEdit);
			Ref<StyleBoxFlat> input_style = memnew(StyleBoxFlat);
			input_style->set_bg_color(get_theme_color(SNAME("dark_color_1"), SNAME("Editor")));
			input_style->set_border_width_all(1);
			input_style->set_border_color(get_theme_color(SNAME("dark_color_3"), SNAME("Editor")));
			input_field->add_theme_style_override("normal", input_style);
			input_field->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			input_field->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
			input_field->set_placeholder("Ask me anything about Godot...");
			input_field->set_custom_minimum_size(Size2(0, 100)); // Minimum height for the input box
			input_field->connect("text_changed", callable_mp(this, &AIChatDock::_on_input_text_changed));
			input_field->connect("gui_input", callable_mp(this, &AIChatDock::_on_input_field_gui_input));
			input_container->add_child(input_field);

			send_button = memnew(Button);
			send_button->set_text("Send");
			send_button->set_disabled(true);
			send_button->connect("pressed", callable_mp(this, &AIChatDock::_on_send_button_pressed));
			input_container->add_child(send_button);

			// Load saved model from settings, now that UI is ready.
			if (EditorSettings::get_singleton()->has_setting("ai_chat/model")) {
				String saved_model = EditorSettings::get_singleton()->get_setting("ai_chat/model");
				model = saved_model;
				// Set the dropdown to the saved model
				for (int i = 0; i < model_dropdown->get_item_count(); i++) {
					if (model_dropdown->get_item_text(i) == saved_model) {
						model_dropdown->select(i);
						break;
					}
				}
			}

			// Add welcome message after the UI is fully initialized.
			_add_message_to_chat("assistant", "# Hello! I'm your AI assistant\n\nI can help you with **Godot development**, *scripting questions*, `debugging`, and more.\n\n## What I can help with:\n- Scene and node management\n- Code analysis and suggestions\n- Project file operations\n- And much more!\n\n```gdscript\n# Example: Creating a simple node\nfunc _ready():\n    var new_node = Node2D.new()\n    add_child(new_node)\n```\n\nHow can I assist you today?");
		} break;
		case NOTIFICATION_PROCESS: {
			if (http_client.is_valid()) {
				http_client->poll();
				if (http_status == STATUS_CONNECTING && http_client->get_status() == HTTPClient::STATUS_CONNECTED) {
					http_status = STATUS_REQUESTING;
					Error err = http_client->request(HTTPClient::METHOD_POST, pending_request_path, pending_request_headers, pending_request_body.ptr(), pending_request_body.size());
					if (err != OK) {
						_add_message_to_chat("system", "Failed to send request to backend.");
						is_waiting_for_response = false;
						_update_ui_state();
						http_status = STATUS_DONE;
						return;
					}
				} else if (http_status == STATUS_REQUESTING && http_client->get_status() == HTTPClient::STATUS_BODY) {
					http_status = STATUS_BODY;
				}

				if (http_client->get_status() == HTTPClient::STATUS_BODY) {
					PackedByteArray chunk = http_client->read_response_body_chunk();
					if (chunk.size() > 0) {
						_handle_response_chunk(chunk);
					}
				}

				if (http_client->get_status() == HTTPClient::STATUS_DISCONNECTED || http_client->get_status() == HTTPClient::STATUS_CONNECTION_ERROR || http_client->get_status() == HTTPClient::STATUS_CANT_CONNECT) {
					is_waiting_for_response = false;
					_update_ui_state();
					http_status = STATUS_DONE;
					current_assistant_message_label = nullptr;
					set_process(false);
				}
			}
		} break;
		case NOTIFICATION_ENTER_TREE: {
			// Load API key from editor settings
			if (EditorSettings::get_singleton()->has_setting("ai_chat/api_key")) {
				api_key = EditorSettings::get_singleton()->get_setting("ai_chat/api_key");
			}
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			// Theme updates handled automatically
		} break;
	}
}

void AIChatDock::_on_send_button_pressed() {
	String message = input_field->get_text().strip_edges();
	if (message.is_empty() || is_waiting_for_response) {
		return;
	}

	input_field->set_text("");
	_add_message_to_chat("user", message);
	_send_chat_request();
	input_field->grab_focus();
}

void AIChatDock::_on_input_text_changed() {
	send_button->set_disabled(input_field->get_text().strip_edges().is_empty() || is_waiting_for_response);
}

void AIChatDock::_on_input_field_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid() && key_event->is_pressed() && !key_event->is_echo()) {
		if (key_event->get_keycode() == Key::ENTER && !key_event->is_shift_pressed()) {
			_on_send_button_pressed();
			get_viewport()->set_input_as_handled(); // Consume the event to prevent the default newline behavior
		}
	}
}

void AIChatDock::_on_model_selected(int p_index) {
	if (model_dropdown) {
		String selected_model = model_dropdown->get_item_text(p_index);
		model = selected_model;
		// Save the selected model to editor settings
		EditorSettings::get_singleton()->set_setting("ai_chat/model", model);
	}
}

void AIChatDock::_handle_response_chunk(const PackedByteArray &p_chunk) {
	response_buffer += String::utf8((const char *)p_chunk.ptr(), p_chunk.size());

	int newline_pos;
	while ((newline_pos = response_buffer.find("\n")) != -1) {
		String line = response_buffer.substr(0, newline_pos);
		response_buffer = response_buffer.substr(newline_pos + 1);
		if (line.strip_edges().is_empty()) {
			continue;
		}
		_process_ndjson_line(line);
	}
}

void AIChatDock::_process_ndjson_line(const String &p_line) {
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(p_line);
	if (err != OK) {
		_add_message_to_chat("system", "Error parsing streaming response: " + p_line);
		return;
	}

	Dictionary data = json->get_data();

	if (data.has("error")) {
		_add_message_to_chat("system", "Backend error: " + String(data["error"]));
		return;
	}

	if (data.has("status") && data["status"] == "executing_tools") {
		if (data.has("assistant_message")) {
			Dictionary assistant_message = data["assistant_message"];
			// This status means a new assistant turn is starting with tool calls.
			// We must create a new message for it, not update a previous one.
			_add_message_to_chat("assistant", assistant_message.get("content", ""), assistant_message.get("tool_calls", Array()));

			if (assistant_message.has("tool_calls")) {
				_execute_tool_calls(assistant_message.get("tool_calls", Array()));
			}
		}
		return; // Stop further processing for this line
	}

	// This handles streaming content deltas
	if (data.has("content_delta")) {
		RichTextLabel *label = _get_or_create_current_assistant_message_label();
		String delta = data["content_delta"];
		if (!chat_history.is_empty()) {
			ChatMessage &last_msg = chat_history.write[chat_history.size() - 1];
			if (last_msg.role == "assistant") {
				last_msg.content += delta;
				// Convert the accumulated content to BBCode and set it
				label->set_text(_markdown_to_bbcode(last_msg.content));
			}
		}
		call_deferred("_scroll_to_bottom");
	}

	// This handles the final message of a stream.
	if (data.has("assistant_message")) {
		Dictionary assistant_message = data["assistant_message"];

		// If this final message has tool calls, we shouldn't be processing it here.
		// It should have been handled by the "executing_tools" status.
		// But as a fallback, we can add it to the chat.
		if (assistant_message.has("tool_calls")) {
			_add_message_to_chat("assistant", assistant_message.get("content", ""), assistant_message.get("tool_calls", Array()));
			return;
		}

		RichTextLabel *label = _get_or_create_current_assistant_message_label();
		String final_content = assistant_message.get("content", "");
		if (final_content == "<null>") {
			final_content = "";
		}
		
		if (!chat_history.is_empty()) {
			ChatMessage &last_msg = chat_history.write[chat_history.size() - 1];
			if (last_msg.role == "assistant") {
				// Update the content in history and then render from history.
				last_msg.content = final_content;
				label->set_text(_markdown_to_bbcode(last_msg.content));
			}
		} else {
			// This case should ideally not happen if the stream started correctly.
			label->set_text(_markdown_to_bbcode(final_content));
		}
	}
}

RichTextLabel *AIChatDock::_get_or_create_current_assistant_message_label() {
	// If a label is already assigned, use it.
	if (current_assistant_message_label != nullptr) {
		// Make sure its parent is visible.
		Control *bubble_panel = Object::cast_to<Control>(current_assistant_message_label->get_parent()->get_parent());
		if (bubble_panel && !bubble_panel->is_visible()) {
			bubble_panel->set_visible(true);
		}
		return current_assistant_message_label;
	}

	// If the last message was an assistant message (likely with tool calls but no content yet),
	// find its label and reuse it.
	if (!chat_history.is_empty()) {
		const ChatMessage &last_msg = chat_history[chat_history.size() - 1];
		if (last_msg.role == "assistant") {
			// Find the last message bubble created.
			for (int i = chat_container->get_child_count() - 1; i >= 0; i--) {
				PanelContainer *panel = Object::cast_to<PanelContainer>(chat_container->get_child(i));
				if (panel) {
					RichTextLabel *rt = find_rich_text_label_in_children(panel);
					if (rt) {
						current_assistant_message_label = rt;
						panel->set_visible(true); // Ensure it's visible.
						return current_assistant_message_label;
					}
				}
			}
		}
	}

	// Otherwise, create a new message bubble.
	_add_message_to_chat("assistant", "");
	return current_assistant_message_label; // _add_message_to_chat sets this.
}

void AIChatDock::_execute_tool_calls(const Array &p_tool_calls) {
	for (int i = 0; i < p_tool_calls.size(); i++) {
		Dictionary tool_call = p_tool_calls[i];
		String tool_call_id = tool_call.get("id", "");
		Dictionary function_dict = tool_call.get("function", Dictionary());
		String function_name = function_dict.get("name", "");
		String arguments_str = function_dict.get("arguments", "{}");

		// Don't add system messages - they break OpenAI format!
		// _add_message_to_chat("system", "🔧 Executing: " + function_name);

		Ref<JSON> json;
		json.instantiate();
		Error err = json->parse(arguments_str);
		Dictionary args;
		if (err == OK) {
			args = json->get_data();
		}

		Dictionary result;

		// Execute the tool
		if (function_name == "get_scene_info") {
			result = EditorTools::get_scene_info(args);
		} else if (function_name == "get_all_nodes") {
			result = EditorTools::get_all_nodes(args);
		} else if (function_name == "search_nodes_by_type") {
			result = EditorTools::search_nodes_by_type(args);
		} else if (function_name == "get_editor_selection") {
			result = EditorTools::get_editor_selection(args);
		} else if (function_name == "get_node_properties") {
			result = EditorTools::get_node_properties(args);
		} else if (function_name == "save_scene") {
			result = EditorTools::save_scene(args);
		} else if (function_name == "get_available_classes") {
			result = EditorTools::get_available_classes(args);
		} else if (function_name == "create_node") {
			result = EditorTools::create_node(args);
		} else if (function_name == "delete_node") {
			result = EditorTools::delete_node(args);
		} else if (function_name == "set_node_property") {
			result = EditorTools::set_node_property(args);
		} else if (function_name == "move_node") {
			result = EditorTools::move_node(args);
		} else if (function_name == "call_node_method") {
			result = EditorTools::call_node_method(args);
		} else if (function_name == "get_node_script") {
			result = EditorTools::get_node_script(args);
		} else if (function_name == "attach_script") {
			result = EditorTools::attach_script(args);
		} else if (function_name == "manage_scene") {
			result = EditorTools::manage_scene(args);
		} else if (function_name == "add_collision_shape") {
			result = EditorTools::add_collision_shape(args);
		} else if (function_name == "generalnodeeditor") {
			result = EditorTools::generalnodeeditor(args);
		} else if (function_name == "list_project_files") {
			result = EditorTools::list_project_files(args);
		} else if (function_name == "read_file_content") {
			result = EditorTools::read_file_content(args);
		} else if (function_name == "read_file_advanced") {
			result = EditorTools::read_file_advanced(args);
		} else if (function_name == "apply_edit") {
			String path = args["path"];
			String prompt = args["prompt"];

			Error err;
			String file_content = FileAccess::get_file_as_string(path, &err);
			if (err != OK) {
				result["success"] = false;
				result["message"] = "Failed to read file for editing: " + path;
			} else {
				Dictionary prediction = EditorTools::_predict_code_edit(file_content, prompt, api_endpoint);
				if (!prediction.get("success", false)) {
					result = prediction;
				} else {
					String new_content = prediction["new_content"];
					diff_viewer->set_diff(path, file_content, new_content);
					
					// Check if the script is open in the editor
					if (diff_viewer->has_script_open(path)) {
						// Auto-apply to script editor for immediate preview
						diff_viewer->apply_to_script_editor();
						result["success"] = true;
						result["message"] = "Applied changes to script editor for " + path + ". You can save when ready.";
					} else {
						// Show popup for files not currently open
						diff_viewer->popup_centered();
						result["success"] = true;
						result["message"] = "Showing diff for " + path;
					}
				}
			}
		} else {
			result["success"] = false;
			result["message"] = "Unknown tool: " + function_name;
		}

		// Add a proper, separate tool bubble for the output.
		_add_tool_response_to_chat(tool_call_id, function_name, args, result);
		
		// Don't add system messages - they break OpenAI format!
		// bool success = result.get("success", false);
		// String message = result.get("message", "");
		// String status_icon = success ? "✅" : "❌";
		// _add_message_to_chat("system", status_icon + " " + function_name + ": " + message);
	}

	// We've finished with this turn's tool calls. Clear the current label
	// so the next content_delta or assistant_message creates a new bubble
	// or finds the correct existing one.
	current_assistant_message_label = nullptr;
	_send_chat_request();
}

void AIChatDock::_add_message_to_chat(const String &p_role, const String &p_content, const Array &p_tool_calls) {
	ChatMessage msg;
	msg.role = p_role;
	msg.content = p_content;
	if (msg.content == "<null>") {
		msg.content = "";
	}
	msg.timestamp = _get_timestamp();
	msg.tool_calls = p_tool_calls;

	chat_history.push_back(msg);
	_create_message_bubble(msg);
	// Show tool call placeholders immediately for better UX
	if (p_role == "assistant") {
		_create_tool_call_bubbles(p_tool_calls);
	}

	// Auto-scroll to bottom
	call_deferred("_scroll_to_bottom");
}

void AIChatDock::_add_tool_response_to_chat(const String &p_tool_call_id, const String &p_name, const Dictionary &p_args, const Dictionary &p_result) {
	Ref<JSON> json;
	json.instantiate();

	ChatMessage msg;
	msg.role = "tool";
	msg.tool_call_id = p_tool_call_id;
	msg.name = p_name;
	msg.content = json->stringify(p_result);
	msg.timestamp = _get_timestamp();

	chat_history.push_back(msg);

	// Find the placeholder for this tool and replace its content.
	PanelContainer *placeholder = Object::cast_to<PanelContainer>(chat_container->find_child("tool_placeholder_" + p_tool_call_id, true, false));
	if (!placeholder) {
		return;
	}

	// Clear the "loading" text.
	for (int i = 0; i < placeholder->get_child_count(); i++) {
		placeholder->get_child(i)->queue_free();
	}

	// Create the replacement UI.
	VBoxContainer *tool_container = memnew(VBoxContainer);
	placeholder->add_child(tool_container);

	Button *toggle_button = memnew(Button);
	
	// Show success/failure status in the button
	Dictionary data = p_result;
	bool success = data.get("success", false);
	String message = data.get("message", "");
	String status_icon = success ? "✅" : "❌";
	toggle_button->set_text(status_icon + " " + p_name + ": " + message);
	
	toggle_button->set_flat(false);
	toggle_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toggle_button->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	toggle_button->add_theme_icon_override("icon", get_theme_icon(success ? SNAME("StatusSuccess") : SNAME("StatusError"), SNAME("EditorIcons")));
	toggle_button->add_theme_color_override("font_color", success ? get_theme_color(SNAME("success_color"), SNAME("Editor")) : get_theme_color(SNAME("error_color"), SNAME("Editor")));
	tool_container->add_child(toggle_button);

				PanelContainer *content_panel = memnew(PanelContainer);
	content_panel->set_visible(false); // Collapsed by default.
				tool_container->add_child(content_panel);
				toggle_button->connect("pressed", callable_mp(this, &AIChatDock::_on_tool_output_toggled).bind(content_panel));

				Ref<StyleBoxFlat> content_style = memnew(StyleBoxFlat);
				content_style->set_bg_color(get_theme_color(SNAME("dark_color_1"), SNAME("Editor")));
	content_style->set_border_width_all(1);
	content_style->set_border_color(get_theme_color(SNAME("dark_color_2"), SNAME("Editor")));
				content_style->set_content_margin_all(10);
				content_panel->add_theme_style_override("panel", content_style);

	VBoxContainer *content_vbox = memnew(VBoxContainer);
	content_panel->add_child(content_vbox);

	HBoxContainer *header_hbox = memnew(HBoxContainer);
	content_vbox->add_child(header_hbox);

	// Dictionary data = p_result; // Already declared above
	// bool success = data.get("success", false); // Already declared above

	Label *status_label = memnew(Label);
	status_label->set_text(success ? "Tool Succeeded" : "Tool Failed");
	status_label->add_theme_color_override("font_color", success ? get_theme_color(SNAME("success_color"), SNAME("Editor")) : get_theme_color(SNAME("error_color"), SNAME("Editor")));
	status_label->add_theme_icon_override("icon", get_theme_icon(success ? SNAME("StatusSuccess") : SNAME("StatusError"), SNAME("EditorIcons")));
	header_hbox->add_child(status_label);

	content_vbox->add_child(memnew(HSeparator));

	// Create specific UI based on the tool that was called.
	if (p_name == "list_project_files" && success) {
		Tree *file_tree = memnew(Tree);
		file_tree->set_hide_root(true);
		file_tree->set_custom_minimum_size(Size2(0, 300));
		TreeItem *root = file_tree->create_item();

		Dictionary tree_items;

		Array files = p_result.get("files", Array());
		for (int i = 0; i < files.size(); i++) {
			String file_path = files[i];
			Vector<String> parts = file_path.split("/");
			TreeItem *current_item = root;
			String current_path = "";

			for (int j = 0; j < parts.size(); j++) {
				current_path += parts[j];
				if (tree_items.has(current_path)) {
					current_item = (TreeItem *)((Object *)tree_items[current_path]);
				} else {
					TreeItem *new_item = file_tree->create_item(current_item);
					new_item->set_text(0, parts[j]);
					bool is_dir = j < parts.size() - 1;
					new_item->set_icon(0, is_dir ? get_theme_icon(SNAME("Folder"), SNAME("EditorIcons")) : get_theme_icon(SNAME("File"), SNAME("EditorIcons")));
					tree_items[current_path] = new_item;
					current_item = new_item;
				}
				current_path += "/";
			}
		}
		content_vbox->add_child(file_tree);

	} else if (p_name == "read_file_content" && success) {
		VBoxContainer *file_content_vbox = memnew(VBoxContainer);
		content_vbox->add_child(file_content_vbox);

		Button *file_link = memnew(Button);
		file_link->set_text(p_args["path"]);
		file_link->set_flat(true);
		file_link->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
		file_link->add_theme_icon_override("icon", get_theme_icon(SNAME("File"), SNAME("EditorIcons")));
		file_link->connect("pressed", callable_mp(this, &AIChatDock::_on_tool_file_link_pressed).bind(p_args["path"]));
		file_content_vbox->add_child(file_link);

		TextEdit *file_content = memnew(TextEdit);
		file_content->set_text(p_result["content"]);
		file_content->set_editable(false);
		file_content->set_custom_minimum_size(Size2(0, 300));
		file_content_vbox->add_child(file_content);

	} else {
		RichTextLabel *content_label = memnew(RichTextLabel);
		content_label->add_theme_font_override("normal_font", get_theme_font(SNAME("source"), SNAME("EditorFonts")));
		content_label->set_text(json->stringify(p_result, "  "));
		content_label->set_fit_content(true);
		content_label->set_selection_enabled(true);
		content_vbox->add_child(content_label);
	}

	call_deferred("_scroll_to_bottom");
}

void AIChatDock::_scroll_to_bottom() {
	if (chat_scroll) {
		chat_scroll->ensure_control_visible(chat_container);
		VScrollBar *vbar = chat_scroll->get_v_scroll_bar();
		if (vbar) {
			vbar->set_value(vbar->get_max());
		}
	}
}

void AIChatDock::_on_tool_file_link_pressed(const String &p_path) {
	EditorNode::get_singleton()->load_scene(p_path);
}

void AIChatDock::_create_message_bubble(const ChatMessage &p_message) {
	PanelContainer *message_panel = memnew(PanelContainer);
	chat_container->add_child(message_panel);

	// Default to invisible. We'll show it only if it has content.
	message_panel->set_visible(false);

	// --- Style Configuration ---
	Ref<StyleBoxFlat> panel_style = memnew(StyleBoxFlat);
	panel_style->set_content_margin_all(10);
	Color role_color;

	if (p_message.role == "user") {
		panel_style->set_bg_color(get_theme_color(SNAME("accent_color"), SNAME("Editor")) * Color(1, 1, 1, 0.1));
		panel_style->set_border_width(SIDE_LEFT, 4);
		panel_style->set_border_color(get_theme_color(SNAME("accent_color"), SNAME("Editor")));
		role_color = get_theme_color(SNAME("accent_color"), SNAME("Editor"));
	} else { // Assistant and System
		panel_style->set_bg_color(get_theme_color(SNAME("dark_color_3"), SNAME("Editor")));
		panel_style->set_border_width(SIDE_LEFT, 4);
		panel_style->set_border_color(get_theme_color(SNAME("dark_color_1"), SNAME("Editor")));
		role_color = (p_message.role == "system") ? get_theme_color(SNAME("warning_color"), SNAME("Editor")) : get_theme_color(SNAME("font_color"), SNAME("Editor"));
	}
	message_panel->add_theme_style_override("panel", panel_style);

	// --- Content ---
	VBoxContainer *message_vbox = memnew(VBoxContainer);
	message_panel->add_child(message_vbox);

	Label *role_label = memnew(Label);
	role_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
	role_label->set_text(p_message.role.capitalize());
	role_label->add_theme_color_override("font_color", role_color);
	message_vbox->add_child(role_label);

	// Always create a content label for assistant to allow streaming.
	RichTextLabel *content_label = memnew(RichTextLabel);
	content_label->set_fit_content(true);
	content_label->set_selection_enabled(true);
	content_label->set_use_bbcode(true);
	content_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	message_vbox->add_child(content_label);

	if (p_message.role == "assistant") {
		current_assistant_message_label = content_label;
	}

	if (!p_message.content.strip_edges().is_empty()) {
		content_label->set_text(_markdown_to_bbcode(p_message.content));
		message_panel->set_visible(true);
	}

	if (!p_message.tool_calls.is_empty()) {
		message_panel->set_visible(true);
	}

	// Add spacing after each message panel
	Control *spacer = memnew(Control);
	spacer->set_custom_minimum_size(Size2(0, 10));
	chat_container->add_child(spacer);
}

void AIChatDock::_create_tool_call_bubbles(const Array &p_tool_calls) {
	if (!current_assistant_message_label) {
		return;
	}

	Control *bubble_panel = Object::cast_to<Control>(current_assistant_message_label->get_parent()->get_parent());
	VBoxContainer *message_vbox = Object::cast_to<VBoxContainer>(bubble_panel->get_child(0));

	if (!message_vbox) {
		return;
	}

	for (int i = 0; i < p_tool_calls.size(); i++) {
		Dictionary tool_call = p_tool_calls[i];
		String tool_call_id = tool_call.get("id", "");
		Dictionary function_dict = tool_call.get("function", Dictionary());
		String func_name = function_dict.get("name", "unknown_tool");

		// Create a container that will hold either the "loading" label or the final tool output.
		// This avoids replacing nodes, which was causing errors.
		PanelContainer *placeholder = memnew(PanelContainer);
		placeholder->set_name("tool_placeholder_" + tool_call_id);
		message_vbox->add_child(placeholder);

		Ref<StyleBoxFlat> placeholder_style = memnew(StyleBoxFlat);
		placeholder_style->set_bg_color(get_theme_color(SNAME("dark_color_1"), SNAME("Editor")));
		placeholder_style->set_content_margin_all(10);
		placeholder_style->set_border_width_all(1);
		placeholder_style->set_border_color(get_theme_color(SNAME("dark_color_2"), SNAME("Editor")));
		placeholder_style->set_corner_radius_all(5);
		placeholder->add_theme_style_override("panel", placeholder_style);

		HBoxContainer *tool_hbox = memnew(HBoxContainer);
		placeholder->add_child(tool_hbox);

		Label *tool_label = memnew(Label);
		tool_label->set_text("Calling tool: " + func_name + "...");
		tool_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.6));
		tool_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Tools"), SNAME("EditorIcons")));
		tool_hbox->add_child(tool_label);
	}
}

void AIChatDock::_on_tool_output_toggled(Control *p_content) {
	p_content->set_visible(!p_content->is_visible());
}

void AIChatDock::_send_chat_request() {
	is_waiting_for_response = true;
	_update_ui_state();
	response_buffer.clear();
	http_status = STATUS_IDLE;

	// Build the messages array for the API
	Array messages;
	for (int i = 0; i < chat_history.size(); i++) {
		const ChatMessage &msg = chat_history[i];
		Dictionary api_msg;
		api_msg["role"] = msg.role;
		api_msg["content"] = msg.content;
		if (msg.role == "assistant" && !msg.tool_calls.is_empty()) {
			api_msg["tool_calls"] = msg.tool_calls;
		}
		if (msg.role == "tool") {
			api_msg["tool_call_id"] = msg.tool_call_id;
			api_msg["name"] = msg.name;
		}
		messages.push_back(api_msg);
	}

	// Build request data
	Dictionary request_data;
	request_data["messages"] = messages;

	Ref<JSON> json;
	json.instantiate();
	String request_body = json->stringify(request_data);

	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");

	// Setup HTTPClient
	http_client->set_read_chunk_size(4096); // 4kb chunk size

	// Extract host and port from endpoint
	String host = api_endpoint;
	int port = 80;
	bool use_ssl = false;

	if (host.begins_with("https://")) {
		host = host.trim_prefix("https://");
		use_ssl = true;
		port = 443;
	} else if (host.begins_with("http://")) {
		host = host.trim_prefix("http://");
	}

	String path = "/";
	if (host.find("/") != -1) {
		path = host.substr(host.find("/"), -1);
		host = host.substr(0, host.find("/"));
	}

	if (host.find(":") != -1) {
		port = host.substr(host.find(":") + 1, -1).to_int();
		host = host.substr(0, host.find(":"));
	}

	PackedByteArray request_body_data = request_body.to_utf8_buffer();
	Error err = http_client->connect_to_host(host, port, (use_ssl ? TLSOptions::client_unsafe(Ref<X509Certificate>()) : Ref<TLSOptions>()));
	if (err != OK) {
		_add_message_to_chat("system", "Failed to connect to backend.");
		is_waiting_for_response = false;
		_update_ui_state();
		return;
	}

	http_status = STATUS_CONNECTING;
	pending_request_path = path;
	pending_request_headers = headers;
	pending_request_body = request_body_data;

	set_process(true);
}

void AIChatDock::_update_ui_state() {
	send_button->set_disabled(input_field->get_text().strip_edges().is_empty() || is_waiting_for_response);
	input_field->set_editable(!is_waiting_for_response);

	if (is_waiting_for_response) {
		send_button->set_text("...");
	} else {
		send_button->set_text("Send");
	}
}

String AIChatDock::_get_timestamp() {
	Dictionary time_dict = Time::get_singleton()->get_datetime_dict_from_system();
	return String::num_int64(time_dict["hour"]).pad_zeros(2) + ":" +
		   String::num_int64(time_dict["minute"]).pad_zeros(2);
}

String AIChatDock::_process_inline_markdown(String p_line) {
	String line = p_line;

	// Bold (**text** or __text__)
	while (true) {
		int start = line.find("**");
		if (start == -1) {
			start = line.find("__");
			if (start == -1) {
				break;
			}
		}

		String marker = line.substr(start, 2);
		int end = line.find(marker, start + 2);
		if (end == -1) {
			break;
		}

		String before = line.substr(0, start);
		String bold_text = line.substr(start + 2, end - start - 2);
		String after = line.substr(end + 2);

		line = before + "[b]" + bold_text + "[/b]" + after;
	}

	// Italic (*text* or _text_) - but not inside ** or __
	int pos = 0;
	while (pos < line.length()) {
		int star_pos = line.find("*", pos);
		int underscore_pos = line.find("_", pos);

		int start = -1;
		String marker;

		if (star_pos != -1 && (underscore_pos == -1 || star_pos < underscore_pos)) {
			// Check it's not part of **
			if ((star_pos > 0 && line[star_pos - 1] == '*') || (star_pos < line.length() - 1 && line[star_pos + 1] == '*')) {
				pos = star_pos + 1;
				continue;
			}
			start = star_pos;
			marker = "*";
		} else if (underscore_pos != -1) {
			// Check it's not part of __
			if ((underscore_pos > 0 && line[underscore_pos - 1] == '_') || (underscore_pos < line.length() - 1 && line[underscore_pos + 1] == '_')) {
				pos = underscore_pos + 1;
				continue;
			}
			start = underscore_pos;
			marker = "_";
		}

		if (start == -1) {
			break;
		}

		int end = line.find(marker, start + 1);
		if (end == -1) {
			pos = start + 1;
			continue;
		}

		String before = line.substr(0, start);
		String italic_text = line.substr(start + 1, end - start - 1);
		String after = line.substr(end + 1);

		line = before + "[i]" + italic_text + "[/i]" + after;
		pos = before.length() + 3 + italic_text.length() + 4; // Skip past [i]...[/i]
	}

	// Inline code (`text`)
	while (true) {
		int start = line.find("`");
		if (start == -1) {
			break;
		}

		int end = line.find("`", start + 1);
		if (end == -1) {
			break;
		}

		String before = line.substr(0, start);
		String code_text = line.substr(start + 1, end - start - 1);
		String after = line.substr(end + 1);

		line = before + "[code]" + code_text + "[/code]" + after;
	}

	return line;
}

String AIChatDock::_markdown_to_bbcode(const String &p_markdown) {
	if (p_markdown.is_empty()) {
		return String();
	}

	Vector<String> lines = p_markdown.split("\n");
	String result = "";
	bool in_code_block = false;

	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];

		if (line.strip_edges().begins_with("```")) {
			in_code_block = !in_code_block;
			if (in_code_block) {
				result += "[code]";
			} else {
				result += "[/code]";
			}
		} else if (in_code_block) {
			result += line.xml_escape(); // Escape to prevent BBCode parsing inside code blocks.
		} else {
			if (line.strip_edges().is_empty()) {
				// Preserve blank lines between paragraphs.
				result += "";
			} else {
				String processed_line = line;
				String trimmed_line = processed_line.lstrip(" \t");

				// Headers
				if (trimmed_line.begins_with("#")) {
					int header_level = 0;
					while (header_level < trimmed_line.length() && trimmed_line[header_level] == '#') {
						header_level++;
					}
					String header_content = trimmed_line.substr(header_level).strip_edges();
					if (!header_content.is_empty()) {
						int font_size = 22 - (header_level * 2);
						processed_line = "[font_size=" + String::num_int64(font_size) + "][b]" + _process_inline_markdown(header_content) + "[/b][/font_size]";
					}
					// Lists
				} else if (trimmed_line.begins_with("- ") || trimmed_line.begins_with("* ")) {
					String item_content = trimmed_line.substr(trimmed_line.find(" ") + 1);
					processed_line = "[indent]" + String::chr(0x2022) + " " + _process_inline_markdown(item_content) + "[/indent]";
				} else {
					// Regular paragraph
					processed_line = _process_inline_markdown(processed_line);
				}
				result += processed_line;
			}
		}

		if (i < lines.size() - 1) {
			result += "\n";
		}
	}

	return result;
}

void AIChatDock::clear_chat_history() {
	chat_history.clear();

	// Clear UI
	for (int i = chat_container->get_child_count() - 1; i >= 0; i--) {
		Node *child = chat_container->get_child(i);
		child->queue_free();
	}
}

void AIChatDock::set_api_key(const String &p_api_key) {
	api_key = p_api_key;
	EditorSettings::get_singleton()->set_setting("ai_chat/api_key", p_api_key);
}

void AIChatDock::set_api_endpoint(const String &p_endpoint) {
	api_endpoint = p_endpoint;
	EditorTools::set_api_endpoint(p_endpoint);
}

void AIChatDock::set_model(const String &p_model) {
	model = p_model;
}

void AIChatDock::_save_layout_to_config(Ref<ConfigFile> p_layout, const String &p_section) const {
	p_layout->set_value(p_section, "api_endpoint", api_endpoint);
	p_layout->set_value(p_section, "model", model);
}

void AIChatDock::_load_layout_from_config(Ref<ConfigFile> p_layout, const String &p_section) {
	if (p_layout->has_section_key(p_section, "api_endpoint")) {
		api_endpoint = p_layout->get_value(p_section, "api_endpoint");
	}
	if (p_layout->has_section_key(p_section, "model")) {
		model = p_layout->get_value(p_section, "model");
	}
}

AIChatDock::AIChatDock() {
	set_name("AI Chat");

	// HTTP request for API calls
	http_client = HTTPClient::create();

	diff_viewer = memnew(DiffViewer);
	add_child(diff_viewer);
	diff_viewer->connect("diff_accepted", callable_mp(this, &AIChatDock::_on_diff_accepted));
}

void AIChatDock::_on_diff_accepted(const String &p_path, const String &p_content) {
    // Changes are now applied directly to script editor
    // No need to write to disk immediately - user can save when ready
    print_line("Diff accepted for: " + p_path);
} 