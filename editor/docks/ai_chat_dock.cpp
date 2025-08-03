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
#include "core/io/resource_loader.h"
#include "core/io/image.h"
#include "core/string/ustring.h"
#include "core/io/marshalls.h"
#include "core/core_bind.h"
#include "scene/resources/image_texture.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_settings.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/script/script_text_editor.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/flow_container.h"
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
#include "scene/gui/texture_rect.h"
#include "scene/gui/dialogs.h"
#include "scene/main/http_request.h"
#include "scene/resources/style_box_flat.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"
#include "core/string/string_name.h"

#include "../ai/editor_tools.h"
#include "diff_viewer.h"

void AIChatDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_send_button_pressed"), &AIChatDock::_on_send_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_input_text_changed"), &AIChatDock::_on_input_text_changed);
	ClassDB::bind_method(D_METHOD("_on_input_field_gui_input"), &AIChatDock::_on_input_field_gui_input);
	ClassDB::bind_method(D_METHOD("_on_model_selected"), &AIChatDock::_on_model_selected);
	ClassDB::bind_method(D_METHOD("_on_tool_output_toggled"), &AIChatDock::_on_tool_output_toggled);
	ClassDB::bind_method(D_METHOD("_on_tool_file_link_pressed", "path"), &AIChatDock::_on_tool_file_link_pressed);
	ClassDB::bind_method(D_METHOD("_on_attach_button_pressed"), &AIChatDock::_on_attach_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_files_selected"), &AIChatDock::_on_files_selected);
	ClassDB::bind_method(D_METHOD("_on_remove_attachment", "path"), &AIChatDock::_on_remove_attachment);
	ClassDB::bind_method(D_METHOD("_on_conversation_selected"), &AIChatDock::_on_conversation_selected);
	ClassDB::bind_method(D_METHOD("_on_new_conversation_pressed"), &AIChatDock::_on_new_conversation_pressed);
	ClassDB::bind_method(D_METHOD("_on_save_image_pressed", "base64_data", "format"), &AIChatDock::_on_save_image_pressed);
	ClassDB::bind_method(D_METHOD("_on_save_image_location_selected"), &AIChatDock::_on_save_image_location_selected);
	ClassDB::bind_method(D_METHOD("_display_generated_image_deferred", "base64_data", "id"), &AIChatDock::_display_generated_image_deferred);
}

void AIChatDock::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_POST_ENTER_TREE: {
			// UI setup moved from constructor to here to ensure theme is available.
			
			// Conversation history row
			HBoxContainer *history_container = memnew(HBoxContainer);
			add_child(history_container);

			Label *history_label = memnew(Label);
			history_label->set_text("Conversation:");
			history_container->add_child(history_label);

			conversation_history_dropdown = memnew(OptionButton);
			conversation_history_dropdown->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			conversation_history_dropdown->connect("item_selected", callable_mp(this, &AIChatDock::_on_conversation_selected));
			history_container->add_child(conversation_history_dropdown);

			new_conversation_button = memnew(Button);
			new_conversation_button->set_text("New");
			new_conversation_button->set_tooltip_text("Start a new conversation");
			new_conversation_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Add"), SNAME("EditorIcons")));
			new_conversation_button->connect("pressed", callable_mp(this, &AIChatDock::_on_new_conversation_pressed));
			history_container->add_child(new_conversation_button);

			// Model selection row
			HBoxContainer *top_container = memnew(HBoxContainer);
			add_child(top_container);

			Label *model_label = memnew(Label);
			model_label->set_text("Model:");
			top_container->add_child(model_label);

			model_dropdown = memnew(OptionButton);
			model_dropdown->add_item("gpt-4o");
			model_dropdown->add_item("gpt-4-turbo");
			model_dropdown->add_item("gpt-3.5-turbo");
			model_dropdown->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			model_dropdown->connect("item_selected", callable_mp(this, &AIChatDock::_on_model_selected));
			top_container->add_child(model_dropdown);

			// Container for attached files (initially hidden)
			attached_files_container = memnew(HFlowContainer);
			attached_files_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			attached_files_container->add_theme_constant_override("h_separation", 6); // Horizontal spacing between attachment tabs
			attached_files_container->add_theme_constant_override("v_separation", 4); // Vertical spacing if wrapping
			attached_files_container->set_visible(false);
			add_child(attached_files_container);

					// Create file dialog
		file_dialog = memnew(EditorFileDialog);
		file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILES);
		file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
		file_dialog->connect("files_selected", callable_mp(this, &AIChatDock::_on_files_selected));
		
		// Add support for common file types including images
		file_dialog->add_filter("*.png, *.jpg, *.jpeg, *.gif, *.bmp, *.webp, *.svg", "Image Files");
		file_dialog->add_filter("*.gd, *.cs, *.cpp, *.h, *.py, *.js, *.json, *.xml, *.txt, *.md", "Code & Text Files");
		file_dialog->add_filter("*", "All Files");
		
		add_child(file_dialog);

		// Create image warning dialog
		image_warning_dialog = memnew(AcceptDialog);
		image_warning_dialog->set_title("Image Downsampled");
		add_child(image_warning_dialog);

		// Create save image dialog
		save_image_dialog = memnew(EditorFileDialog);
		save_image_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
		save_image_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
		save_image_dialog->add_filter("*.png", "PNG Images");
		save_image_dialog->add_filter("*.jpg", "JPEG Images");
		save_image_dialog->connect("files_selected", callable_mp(this, &AIChatDock::_on_save_image_location_selected));
		add_child(save_image_dialog);

			// Chat history area - expand to fill available space
			chat_scroll = memnew(ScrollContainer);
			chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			chat_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
			add_child(chat_scroll);

			chat_container = memnew(VBoxContainer);
			chat_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			chat_scroll->add_child(chat_container);

			// Attach files button row (above input)
			HBoxContainer *attach_container = memnew(HBoxContainer);
			add_child(attach_container);

			// Add spacer to push button to the right
			Control *spacer = memnew(Control);
			spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			attach_container->add_child(spacer);

			// Attach files button
			attach_button = memnew(Button);
			attach_button->set_text("Attach");
			attach_button->set_tooltip_text("Attach project files to your message");
			attach_button->add_theme_icon_override("icon", get_theme_icon(SNAME("FileList"), SNAME("EditorIcons")));
			attach_button->set_custom_minimum_size(Size2(80, 32));
			attach_button->connect("pressed", callable_mp(this, &AIChatDock::_on_attach_button_pressed));
			attach_container->add_child(attach_button);

			// Add some spacing before input area
			Control *input_spacer = memnew(Control);
			input_spacer->set_custom_minimum_size(Size2(0, 4));
			add_child(input_spacer);

			// Input area at the bottom
			HBoxContainer *input_container = memnew(HBoxContainer);
			input_container->add_theme_constant_override("separation", 8); // Gap between input and send button
			add_child(input_container);

			input_field = memnew(TextEdit);
			Ref<StyleBoxFlat> input_style = memnew(StyleBoxFlat);
			input_style->set_bg_color(get_theme_color(SNAME("dark_color_1"), SNAME("Editor")));
			input_style->set_border_width_all(2);
			input_style->set_border_color(get_theme_color(SNAME("dark_color_3"), SNAME("Editor")));
			input_style->set_corner_radius_all(8); // Modern rounded input
			input_style->set_content_margin_all(8); // Better inner padding
			input_field->add_theme_style_override("normal", input_style);
			
			// Focus style for modern interaction
			Ref<StyleBoxFlat> input_focus_style = memnew(StyleBoxFlat);
			input_focus_style->set_bg_color(get_theme_color(SNAME("dark_color_1"), SNAME("Editor")));
			input_focus_style->set_border_width_all(2);
			input_focus_style->set_border_color(get_theme_color(SNAME("accent_color"), SNAME("Editor")) * Color(1, 1, 1, 0.6));
			input_focus_style->set_corner_radius_all(8);
			input_focus_style->set_content_margin_all(8);
			input_field->add_theme_style_override("focus", input_focus_style);
			
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
			send_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Play"), SNAME("EditorIcons")));
			send_button->set_custom_minimum_size(Size2(80, 40));
			
			// Modern button styling
			Ref<StyleBoxFlat> button_style = memnew(StyleBoxFlat);
			button_style->set_bg_color(get_theme_color(SNAME("accent_color"), SNAME("Editor")));
			button_style->set_corner_radius_all(6);
			button_style->set_content_margin_all(8);
			send_button->add_theme_style_override("normal", button_style);
			
			// Hover style
			Ref<StyleBoxFlat> button_hover_style = memnew(StyleBoxFlat);
			button_hover_style->set_bg_color(get_theme_color(SNAME("accent_color"), SNAME("Editor")) * Color(1.1, 1.1, 1.1));
			button_hover_style->set_corner_radius_all(6);
			button_hover_style->set_content_margin_all(8);
			send_button->add_theme_style_override("hover", button_hover_style);
			
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

			// Initialize conversation system after UI is ready
			conversations_file_path = OS::get_singleton()->get_user_data_dir().path_join("ai_chat_conversations.simplifine");
			_load_conversations();
			
			// Create first conversation if none exist
			if (conversations.is_empty()) {
				_create_new_conversation();
			} else {
				// Switch to the most recent conversation
				_switch_to_conversation(conversations.size() - 1);
			}
			
			_update_conversation_dropdown();
		} break;
		case NOTIFICATION_PROCESS: {
			if (http_client.is_valid()) {
				http_client->poll();
				HTTPClient::Status client_status = http_client->get_status();
				if (http_status == STATUS_CONNECTING && client_status == HTTPClient::STATUS_CONNECTED) {
					http_status = STATUS_REQUESTING;
					Error err = http_client->request(HTTPClient::METHOD_POST, pending_request_path, pending_request_headers, pending_request_body.ptr(), pending_request_body.size());
					if (err != OK) {
						_add_message_to_chat("system", "Failed to send request to backend.");
						is_waiting_for_response = false;
						_update_ui_state();
						http_status = STATUS_DONE;
						return;
					}
				} else if (http_status == STATUS_REQUESTING && client_status == HTTPClient::STATUS_BODY) {
					http_status = STATUS_BODY;
				}

				if (client_status == HTTPClient::STATUS_BODY) {
					PackedByteArray chunk = http_client->read_response_body_chunk();
					if (chunk.size() > 0) {
						_handle_response_chunk(chunk);
					}
				}

				if (client_status == HTTPClient::STATUS_DISCONNECTED || client_status == HTTPClient::STATUS_CONNECTION_ERROR || client_status == HTTPClient::STATUS_CANT_CONNECT) {
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
	
	// Create message with attached files
	AIChatDock::ChatMessage msg;
	msg.role = "user";
	msg.content = message;
	msg.timestamp = _get_timestamp();
	msg.attached_files = current_attached_files;
	
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	chat_history.push_back(msg);
	_create_message_bubble(msg);
	
	// Update conversation timestamp and title
	if (current_conversation_index >= 0) {
		conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
		// Update title if it's still "New Conversation"
		if (conversations[current_conversation_index].title == "New Conversation") {
			conversations.write[current_conversation_index].title = _generate_conversation_title(_get_current_chat_history());
		}
		_save_conversations();
		_update_conversation_dropdown();
	}
	
	// Clear attachments after sending
	_clear_attachments();
	
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

void AIChatDock::_on_attach_button_pressed() {
	if (file_dialog) {
		file_dialog->popup_file_dialog();
	}
}

void AIChatDock::_on_files_selected(const Vector<String> &p_files) {
	for (int i = 0; i < p_files.size(); i++) {
		String file_path = p_files[i];
		
		// Check if file is already attached
		bool already_attached = false;
		for (const AttachedFile &existing_file : current_attached_files) {
			if (existing_file.path == file_path) {
				already_attached = true;
				break;
			}
		}
		
		if (!already_attached) {
			AIChatDock::AttachedFile attached_file;
			attached_file.path = file_path;
			attached_file.name = file_path.get_file();
			attached_file.is_image = _is_image_file(file_path);
			attached_file.mime_type = _get_mime_type_from_extension(file_path);
			
			if (attached_file.is_image) {
				// Process image (resize if needed, convert to base64)
				if (_process_image_attachment(attached_file)) {
					current_attached_files.push_back(attached_file);
				} else {
					print_line("AI Chat: Failed to process image: " + file_path);
				}
			} else {
				// Read text file content
				Error err;
				String content = FileAccess::get_file_as_string(file_path, &err);
				if (err == OK) {
					attached_file.content = content;
					current_attached_files.push_back(attached_file);
				} else {
					print_line("AI Chat: Failed to read file: " + file_path);
				}
			}
		}
	}
	
	_update_attached_files_display();
}

void AIChatDock::_on_remove_attachment(const String &p_path) {
	for (int i = 0; i < current_attached_files.size(); i++) {
		if (current_attached_files[i].path == p_path) {
			current_attached_files.remove_at(i);
			break;
		}
	}
	_update_attached_files_display();
}

void AIChatDock::_update_attached_files_display() {
	// Clear existing file display
	for (int i = attached_files_container->get_child_count() - 1; i >= 0; i--) {
		Node *child = attached_files_container->get_child(i);
		child->queue_free();
	}
	
	// Show container only if there are attached files
	attached_files_container->set_visible(current_attached_files.size() > 0);
	
	// Add file chips for each attached file
	for (const AttachedFile &file : current_attached_files) {
		PanelContainer *file_chip = memnew(PanelContainer);
		attached_files_container->add_child(file_chip);
		
		// Modern tab-like styling
		Ref<StyleBoxFlat> chip_style = memnew(StyleBoxFlat);
		// Subtle background with modern styling
		chip_style->set_bg_color(get_theme_color(SNAME("dark_color_2"), SNAME("Editor")));
		chip_style->set_border_width_all(1);
		chip_style->set_border_color(get_theme_color(SNAME("dark_color_3"), SNAME("Editor")));
		chip_style->set_corner_radius_all(6); // More modern rounded corners
		chip_style->set_content_margin(SIDE_TOP, 2);
		chip_style->set_content_margin(SIDE_RIGHT, 6);
		chip_style->set_content_margin(SIDE_BOTTOM, 2);
		chip_style->set_content_margin(SIDE_LEFT, 6);
		chip_style->set_shadow_color(Color(0, 0, 0, 0.15));
		chip_style->set_shadow_size(1);
		file_chip->add_theme_style_override("panel", chip_style);
		
		// Create compact tab-like attachment for both images and files
		HBoxContainer *chip_container = memnew(HBoxContainer);
		chip_container->set_custom_minimum_size(Size2(0, 32)); // Fixed height for consistent tabs
		file_chip->add_child(chip_container);
		
		// File type icon (different icons for different file types)
		Label *file_icon = memnew(Label);
		String icon_name = _get_file_type_icon(file);
		file_icon->add_theme_icon_override("icon", get_theme_icon(icon_name, SNAME("EditorIcons")));
		chip_container->add_child(file_icon);
		
		// File name label (truncated if too long)
		Label *file_label = memnew(Label);
		String display_name = file.name;
		if (display_name.length() > 20) {
			display_name = display_name.substr(0, 17) + "...";
		}
		file_label->set_text(display_name);
		file_label->add_theme_font_size_override("font_size", 12);
		file_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")));
		file_label->set_clip_contents(true);
		file_label->set_tooltip_text(file.name);
		chip_container->add_child(file_label);
		
		// Small spacer
		Control *spacer = memnew(Control);
		spacer->set_custom_minimum_size(Size2(4, 0));
		chip_container->add_child(spacer);
		
		// Remove button
		Button *remove_button = memnew(Button);
		remove_button->set_flat(true);
		remove_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Close"), SNAME("EditorIcons")));
		remove_button->add_theme_color_override("icon_normal_color", get_theme_color(SNAME("error_color"), SNAME("Editor")) * Color(1, 1, 1, 0.6));
		remove_button->add_theme_color_override("icon_hover_color", get_theme_color(SNAME("error_color"), SNAME("Editor")));
		remove_button->set_tooltip_text("Remove " + file.name);
		remove_button->set_custom_minimum_size(Size2(20, 20));
		remove_button->connect("pressed", callable_mp(this, &AIChatDock::_on_remove_attachment).bind(file.path));
		chip_container->add_child(remove_button);
	}
}

void AIChatDock::_clear_attachments() {
	current_attached_files.clear();
	_update_attached_files_display();
}

// Standard drag and drop support for file attachments and scene nodes
bool AIChatDock::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	Dictionary drag_data = p_data;
	
	// Support for drag and drop from filesystem and external sources
	
	// Check if this is a file/directory drag operation from FileSystem dock
	if (drag_data.has("type") && drag_data.has("files")) {
		String type = drag_data["type"];
		if (type == "files" || type == "files_and_dirs") {
			Array files = drag_data["files"];
			// Only accept if there are files to drop (not directories)
			return files.size() > 0;
		}
	}
	
	// Check if this is an external file drop (from OS file manager)
	if (drag_data.has("type")) {
		String type = drag_data["type"];
		if (type == "files_and_dirs_external" || type == "files_external") {
			if (drag_data.has("files")) {
				Array files = drag_data["files"];
				return files.size() > 0;
			}
		}
	}
	
	// Check for direct file paths (alternative external file format)
	if (drag_data.has("files") && !drag_data.has("type")) {
		Array files = drag_data["files"];
		return files.size() > 0;
	}
	
	// Check if this is a scene node drag operation
	if (drag_data.has("type") && drag_data.has("nodes")) {
		String type = drag_data["type"];
		if (type == "nodes") {
			Array nodes = drag_data["nodes"];
			return nodes.size() > 0;
		}
	}
	
	return false;
}

void AIChatDock::drop_data(const Point2 &p_point, const Variant &p_data) {
	if (!can_drop_data(p_point, p_data)) {
		return;
	}
	
	Dictionary drag_data = p_data;
	String type = "";
	if (drag_data.has("type")) {
		type = drag_data["type"];
	}
	
	// Handle file drops (internal FileSystem dock files)
	if (type == "files" || type == "files_and_dirs") {
		Array files = drag_data["files"];
		
		Vector<String> file_paths;
		for (int i = 0; i < files.size(); i++) {
			String path = files[i];
			// Skip directories for now (may add support later)
			if (!path.ends_with("/")) {
				file_paths.push_back(path);
			}
		}
		
		if (file_paths.size() > 0) {
			_attach_dragged_files(file_paths);
		}
	}
	// Handle external file drops (from OS file manager)
	else if (type == "files_and_dirs_external" || type == "files_external" || 
			 (drag_data.has("files") && !drag_data.has("type"))) {
		Array files = drag_data["files"];
		
		Vector<String> file_paths;
		for (int i = 0; i < files.size(); i++) {
			String path = files[i];
			// Skip directories for external files too
			if (!path.ends_with("/") && !path.ends_with("\\")) {
				file_paths.push_back(path);
			}
		}
		
		if (file_paths.size() > 0) {
			_attach_external_files(file_paths);
		}
	}
	// Handle scene node drops
	else if (type == "nodes") {
		Array nodes = drag_data["nodes"];
		_attach_dragged_nodes(nodes);
	}
}

// Forwarded drag and drop support (kept for compatibility)
bool AIChatDock::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	return can_drop_data(p_point, p_data);
}

void AIChatDock::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	drop_data(p_point, p_data);
}

void AIChatDock::_attach_dragged_files(const Vector<String> &p_files) {
	// Use the existing attachment logic for internal project files
	_on_files_selected(p_files);
}

void AIChatDock::_attach_external_files(const Vector<String> &p_files) {
	for (int i = 0; i < p_files.size(); i++) {
		String file_path = p_files[i];
		
		// Check if file exists
		if (!FileAccess::exists(file_path)) {
			print_line("AI Chat: External file does not exist: " + file_path);
			continue;
		}
		
		// Check if file is already attached
		bool already_attached = false;
		for (const AttachedFile &existing_file : current_attached_files) {
			if (existing_file.path == file_path) {
				already_attached = true;
				break;
			}
		}
		
		if (!already_attached) {
			AIChatDock::AttachedFile attached_file;
			attached_file.path = file_path;
			attached_file.name = file_path.get_file();
			attached_file.is_image = _is_image_file(file_path);
			attached_file.mime_type = _get_mime_type_from_extension(file_path);
			
			if (attached_file.is_image) {
				// Process image (resize if needed, convert to base64)
				if (_process_image_attachment(attached_file)) {
					current_attached_files.push_back(attached_file);
				} else {
					print_line("AI Chat: Failed to process external image: " + file_path);
				}
			} else {
				// Read text file content
				Error err;
				String content = FileAccess::get_file_as_string(file_path, &err);
				if (err == OK) {
					attached_file.content = content;
					current_attached_files.push_back(attached_file);
				} else {
					print_line("AI Chat: Failed to read external file: " + file_path + " (Error: " + String::num_int64(err) + ")");
				}
			}
		}
	}
	
	_update_attached_files_display();
}

void AIChatDock::_attach_dragged_nodes(const Array &p_nodes) {
	for (int i = 0; i < p_nodes.size(); i++) {
		NodePath node_path = p_nodes[i];
		
		// Get the node to check if it exists and get info
		Node *node = get_node_or_null(node_path);
		if (!node) {
			continue; // Skip invalid nodes
		}
		
		// Check if node is already attached
		bool already_attached = false;
		for (const AttachedFile &existing_file : current_attached_files) {
			if (existing_file.is_node && existing_file.node_path == node_path) {
				already_attached = true;
				break;
			}
		}
		
		if (!already_attached) {
			AIChatDock::AttachedFile attached_node;
			attached_node.is_node = true;
			attached_node.node_path = node_path;
			attached_node.name = node->get_name();
			attached_node.node_type = node->get_class_name();
			attached_node.path = String(node_path); // Store as string for display
			
			// Create descriptive content for the node
			String node_content = "Node: " + String(node_path) + "\n";
			node_content += "Type: " + attached_node.node_type + "\n";
			node_content += "Name: " + attached_node.name + "\n";
			
			// Add additional properties if script is attached
			Ref<Script> script = node->get_script();
			if (script.is_valid()) {
				node_content += "Script: " + script->get_path().get_file() + "\n";
			}
			
			// Add position for Node2D/Node3D
			Node2D *node2d = Object::cast_to<Node2D>(node);
			if (node2d) {
				node_content += "Position: " + String(node2d->get_position()) + "\n";
			}
			Node3D *node3d = Object::cast_to<Node3D>(node);
			if (node3d) {
				node_content += "Position: " + String(node3d->get_position()) + "\n";
			}
			
			attached_node.content = node_content;
			current_attached_files.push_back(attached_node);
		}
	}
	
	_update_attached_files_display();
}

String AIChatDock::_get_file_type_icon(const AttachedFile &p_file) {
	// Check if this is a node attachment
	if (p_file.is_node) {
		// Return icon based on node type
		if (p_file.node_type == "Node2D") {
			return "Node2D";
		} else if (p_file.node_type == "Node3D") {
			return "Node3D";
		} else if (p_file.node_type == "Control") {
			return "Control";
		} else if (p_file.node_type == "Label") {
			return "Label";
		} else if (p_file.node_type == "Button") {
			return "Button";
		} else {
			return "Node";
		}
	}
	
	String extension = p_file.path.get_extension().to_lower();
	
	// Images
	if (extension == "png" || extension == "jpg" || extension == "jpeg" || 
		extension == "gif" || extension == "bmp" || extension == "svg" || 
		extension == "webp" || extension == "tga" || extension == "exr" || extension == "hdr") {
		return "ImageTexture";
	}
	
	// Scripts
	if (extension == "gd" || extension == "cs") {
		return "Script";
	}
	
	// Scenes
	if (extension == "tscn" || extension == "scn") {
		return "PackedScene";
	}
	
	// Resources
	if (extension == "tres" || extension == "res") {
		return "Object";
	}
	
	// Shaders
	if (extension == "gdshader" || extension == "glsl") {
		return "Shader";
	}
	
	// Audio
	if (extension == "ogg" || extension == "wav" || extension == "mp3") {
		return "AudioStreamOggVorbis";
	}
	
	// Models
	if (extension == "gltf" || extension == "glb" || extension == "obj" || 
		extension == "fbx" || extension == "dae") {
		return "MeshInstance3D";
	}
	
	// Text/Code files
	if (extension == "txt" || extension == "md" || extension == "json" || 
		extension == "xml" || extension == "yaml" || extension == "yml" ||
		extension == "csv" || extension == "cfg" || extension == "ini") {
		return "TextFile";
	}
	
	// Default file icon
	return "File";
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
	
	// Handle completed tool execution results
	if (data.has("status") && data["status"] == "tool_completed") {
		String tool_executed = data.get("tool_executed", "");
		Dictionary tool_result = data.get("tool_result", Dictionary());
		
		print_line("AI Chat: Tool completed: " + tool_executed + " (success: " + String(tool_result.get("success", false) ? "true" : "false") + ")");
		
		// Handle image generation tool results
		if (tool_executed == "generate_image" && tool_result.get("success", false)) {
			String image_data = tool_result.get("image_data", "");
			String prompt = tool_result.get("prompt", "");
			
			if (!image_data.is_empty()) {
				print_line("AI Chat: Displaying generated image for prompt: " + prompt + " (" + String::num(image_data.length()) + " chars base64)");
				_handle_generated_image(image_data, "generated_" + String::num(OS::get_singleton()->get_ticks_msec()));
			}
		}
		
		// Add tool result to chat history for persistence
		Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
		if (!chat_history.is_empty()) {
			ChatMessage &last_msg = chat_history.write[chat_history.size() - 1];
			if (last_msg.role == "assistant") {
				// Ensure tool_results array exists
				if (last_msg.tool_results.is_empty()) {
					last_msg.tool_results = Array();
				}
				last_msg.tool_results.push_back(tool_result);
			}
		}
		
		return; // Stop further processing for this line
	}

	// Handle image generation results (legacy - keeping for backward compatibility)
	if (data.has("status") && data["status"] == "image_generated") {
		if (data.has("image_generated")) {
			Dictionary image_data = data["image_generated"];
			_handle_generated_image(image_data.get("base64_data", ""), image_data.get("id", ""));
		}
		return; // Stop further processing for this line
	}

	// This handles streaming content deltas
	if (data.has("content_delta")) {
		RichTextLabel *label = _get_or_create_current_assistant_message_label();
		String delta = data["content_delta"];
		Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
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
		// Skip to avoid duplicate messages.
		if (assistant_message.has("tool_calls")) {
			print_line("AI_CHAT_DOCK: Skipping duplicate assistant message with tool calls");
			return;
		}

		RichTextLabel *label = _get_or_create_current_assistant_message_label();
		String final_content = assistant_message.get("content", "");
		if (final_content == "<null>") {
			final_content = "";
		}
		
		Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
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
		// Make sure its parent is visible with null checks.
		Node *parent = current_assistant_message_label->get_parent();
		if (parent != nullptr) {
			Node *grandparent = parent->get_parent();
			if (grandparent != nullptr) {
				Control *bubble_panel = Object::cast_to<Control>(grandparent);
				if (bubble_panel && !bubble_panel->is_visible()) {
					bubble_panel->set_visible(true);
				}
			}
		}
		return current_assistant_message_label;
	}



	// If the last message was an assistant message (likely with tool calls but no content yet),
	// find its label and reuse it.
	// First ensure we have a valid conversation
	if (current_conversation_index < 0 || current_conversation_index >= conversations.size()) {
		// No valid conversation, create a new one
		_create_new_conversation();
	}
	
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	if (!chat_history.is_empty()) {
		const AIChatDock::ChatMessage &last_msg = chat_history[chat_history.size() - 1];
		if (last_msg.role == "assistant") {
			// Find the last message bubble created.
			if (chat_container != nullptr) {
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
		} else if (function_name == "list_project_files") {
			result = EditorTools::list_project_files(args);
		} else if (function_name == "read_file_content") {
			result = EditorTools::read_file_content(args);
		} else if (function_name == "read_file_advanced") {
			result = EditorTools::read_file_advanced(args);
		} else if (function_name == "apply_edit") {
			// Use the enhanced EditorTools::apply_edit which returns diff and compilation errors
			result = EditorTools::apply_edit(args);
		} else if (function_name == "check_compilation_errors") {
			result = EditorTools::check_compilation_errors(args);
		} else if (function_name == "run_scene") {
			result = EditorTools::run_scene(args);
		} else if (function_name == "get_scene_tree_hierarchy") {
			result = EditorTools::get_scene_tree_hierarchy(args);
		} else if (function_name == "inspect_physics_body") {
			result = EditorTools::inspect_physics_body(args);
		} else if (function_name == "get_camera_info") {
			result = EditorTools::get_camera_info(args);
		} else if (function_name == "take_screenshot") {
			result = EditorTools::take_screenshot(args);
		} else if (function_name == "check_node_in_scene_tree") {
			result = EditorTools::check_node_in_scene_tree(args);
		} else if (function_name == "inspect_animation_state") {
			result = EditorTools::inspect_animation_state(args);
		} else if (function_name == "get_layers_and_zindex") {
			result = EditorTools::get_layers_and_zindex(args);
		} else if (function_name == "generate_image") {
			// This tool should be handled by the backend, not the frontend
			// If we receive it here, it means something went wrong in the backend filtering
			result["success"] = false;
			result["message"] = "Image generation should be handled by backend, not frontend";
			print_line("AI Chat: Received generate_image tool in frontend - this should be handled by backend");
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
	AIChatDock::ChatMessage msg;
	msg.role = p_role;
	msg.content = p_content;
	if (msg.content == "<null>") {
		msg.content = "";
	}
	msg.timestamp = _get_timestamp();
	msg.tool_calls = p_tool_calls;

	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	chat_history.push_back(msg);
	_create_message_bubble(msg);
	// Show tool call placeholders immediately for better UX
	if (p_role == "assistant") {
		_create_tool_call_bubbles(p_tool_calls);
	}

	// Update conversation timestamp and save
	if (current_conversation_index >= 0) {
		conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
		_save_conversations();
	}

	// Auto-scroll to bottom
	call_deferred("_scroll_to_bottom");
}

void AIChatDock::_add_tool_response_to_chat(const String &p_tool_call_id, const String &p_name, const Dictionary &p_args, const Dictionary &p_result) {
	Ref<JSON> json;
	json.instantiate();

	AIChatDock::ChatMessage msg;
	msg.role = "tool";
	msg.tool_call_id = p_tool_call_id;
	msg.name = p_name;
	msg.content = json->stringify(p_result);
	msg.timestamp = _get_timestamp();

	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	chat_history.push_back(msg);

	// Find the placeholder for this tool and replace its content.
	if (chat_container == nullptr) {
		return;
	}
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
	String status_text = success ? "SUCCESS" : "ERROR";
	toggle_button->set_text(status_text + " - " + p_name + ": " + message);
	
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

	} else if (p_name == "generate_image" && success) {
		// Special handling for image generation results
		Dictionary data = p_result;
		String base64_data = data.get("image_data", "");
		
		if (!base64_data.is_empty()) {
			// Display the generated image
			_display_generated_image_in_tool_result(content_vbox, base64_data, data);
		} else {
			// Fallback to text display if no image data
			RichTextLabel *content_label = memnew(RichTextLabel);
			content_label->add_theme_font_override("normal_font", get_theme_font(SNAME("source"), SNAME("EditorFonts")));
			content_label->set_text(json->stringify(p_result, "  "));
			content_label->set_fit_content(true);
			content_label->set_selection_enabled(true);
			content_vbox->add_child(content_label);
		}
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

void AIChatDock::_create_message_bubble(const AIChatDock::ChatMessage &p_message) {
	if (chat_container == nullptr) {
		return;
	}
	PanelContainer *message_panel = memnew(PanelContainer);
	
	// Add spacing before each message for cleaner layout
	if (chat_container->get_child_count() > 0) {
		Control *spacer = memnew(Control);
		spacer->set_custom_minimum_size(Size2(0, 8)); // 8px gap between messages
		chat_container->add_child(spacer);
	}
	
	chat_container->add_child(message_panel);

	// Default to invisible. We'll show it only if it has content.
	message_panel->set_visible(false);

	// --- Modern Message Bubble Styling ---
	Ref<StyleBoxFlat> panel_style = memnew(StyleBoxFlat);
	panel_style->set_content_margin_all(12); // Slightly more padding for modern look
	panel_style->set_corner_radius_all(8); // Rounded corners for modern appearance
	Color role_color;

	if (p_message.role == "user") {
		// User messages: subtle blue background like modern chat apps
		panel_style->set_bg_color(get_theme_color(SNAME("accent_color"), SNAME("Editor")) * Color(1, 1, 1, 0.08));
		panel_style->set_border_width_all(1);
		panel_style->set_border_color(get_theme_color(SNAME("accent_color"), SNAME("Editor")) * Color(1, 1, 1, 0.2));
		role_color = get_theme_color(SNAME("accent_color"), SNAME("Editor"));
	} else { // Assistant and System
		// Assistant messages: clean subtle background
		panel_style->set_bg_color(get_theme_color(SNAME("dark_color_2"), SNAME("Editor")));
		panel_style->set_border_width_all(1);
		panel_style->set_border_color(get_theme_color(SNAME("dark_color_3"), SNAME("Editor")));
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
	
	// Display attached files if any
	if (!p_message.attached_files.is_empty()) {
		message_panel->set_visible(true);
		
		VBoxContainer *files_container = memnew(VBoxContainer);
		message_vbox->add_child(files_container);
		
		Label *files_header = memnew(Label);
		files_header->set_text("Attached Files:");
		files_header->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		files_header->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
		files_container->add_child(files_header);
		
		// Create a flow container for files to arrange them nicely
		HFlowContainer *files_flow = memnew(HFlowContainer);
		files_container->add_child(files_flow);
		
		for (const AttachedFile &file : p_message.attached_files) {
			if (file.is_image) {
				// Create image display with improved styling
				PanelContainer *image_panel = memnew(PanelContainer);
				files_flow->add_child(image_panel);
				
				Ref<StyleBoxFlat> image_style = memnew(StyleBoxFlat);
				image_style->set_bg_color(get_theme_color(SNAME("dark_color_1"), SNAME("Editor")));
				image_style->set_border_width_all(2);
				image_style->set_border_color(get_theme_color(SNAME("success_color"), SNAME("Editor")));
				image_style->set_corner_radius_all(12);
				image_style->set_content_margin_all(6);
				image_style->set_shadow_color(Color(0, 0, 0, 0.2));
				image_style->set_shadow_size(4);
				image_panel->add_theme_style_override("panel", image_style);
				
				VBoxContainer *image_container = memnew(VBoxContainer);
				image_panel->add_child(image_container);
				
				// Image preview
				if (!file.base64_data.is_empty()) {
					Vector<uint8_t> image_data = CoreBind::Marshalls::get_singleton()->base64_to_raw(file.base64_data);
					if (image_data.size() > 0) {
						Ref<Image> display_image = memnew(Image);
						if (file.mime_type == "image/jpeg" || file.mime_type == "image/jpg") {
							display_image->load_jpg_from_buffer(image_data);
						} else {
							display_image->load_png_from_buffer(image_data);
						}
						
						if (!display_image->is_empty()) {
							// Create display sized image (max 150px for chat)
							Vector2i display_size = _calculate_downsampled_size(file.display_size, 150);
							display_image->resize(display_size.x, display_size.y, Image::INTERPOLATE_LANCZOS);
							
							Ref<ImageTexture> display_texture = ImageTexture::create_from_image(display_image);
							
							TextureRect *image_display = memnew(TextureRect);
							image_display->set_texture(display_texture);
							image_display->set_expand_mode(TextureRect::EXPAND_FIT_WIDTH_PROPORTIONAL);
							image_display->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
							image_display->set_custom_minimum_size(Size2(display_size.x, display_size.y));
							image_container->add_child(image_display);
						}
					}
				}
				
				// Image info
				HBoxContainer *image_info = memnew(HBoxContainer);
				image_container->add_child(image_info);
				
				Label *image_icon = memnew(Label);
				image_icon->add_theme_icon_override("icon", get_theme_icon(SNAME("Image"), SNAME("EditorIcons")));
				image_info->add_child(image_icon);
				
				VBoxContainer *info_vbox = memnew(VBoxContainer);
				image_info->add_child(info_vbox);
				
				Button *file_link = memnew(Button);
				file_link->set_text(file.name);
				file_link->set_flat(true);
				file_link->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
				file_link->set_tooltip_text("Click to open: " + file.path);
				file_link->connect("pressed", callable_mp(this, &AIChatDock::_on_tool_file_link_pressed).bind(file.path));
				info_vbox->add_child(file_link);
				
				Label *size_info = memnew(Label);
				String size_text = String::num_int64(file.display_size.x) + "×" + String::num_int64(file.display_size.y);
				if (file.was_downsampled) {
					size_text += " (resized)";
				}
				size_info->set_text(size_text);
				size_info->add_theme_font_size_override("font_size", 10);
				size_info->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
				info_vbox->add_child(size_info);
				
				// Add spacer to push save button to the right
				Control *spacer = memnew(Control);
				spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				image_info->add_child(spacer);
				
				// Save button for attached images
				Button *save_button = memnew(Button);
				save_button->set_text("Save");
				save_button->set_flat(true);
				save_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Save"), SNAME("EditorIcons")));
				save_button->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
				save_button->add_theme_color_override("icon_normal_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
				save_button->set_tooltip_text("Save this image to your project");
				String image_format = (file.mime_type == "image/jpeg" || file.mime_type == "image/jpg") ? "jpg" : "png";
				save_button->connect("pressed", callable_mp(this, &AIChatDock::_on_save_image_pressed).bind(file.base64_data, image_format));
				image_info->add_child(save_button);
			} else {
				// Text file display
				HBoxContainer *file_row = memnew(HBoxContainer);
				files_flow->add_child(file_row);
				
				Label *file_icon = memnew(Label);
				file_icon->add_theme_icon_override("icon", get_theme_icon(SNAME("File"), SNAME("EditorIcons")));
				file_row->add_child(file_icon);
				
				Button *file_link = memnew(Button);
				file_link->set_text(file.name);
				file_link->set_flat(true);
				file_link->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
				file_link->set_tooltip_text("Click to open: " + file.path);
				file_link->connect("pressed", callable_mp(this, &AIChatDock::_on_tool_file_link_pressed).bind(file.path));
				file_row->add_child(file_link);
			}
		}
	}
	
	// Display tool results (like generated images) if any
	if (!p_message.tool_results.is_empty()) {
		message_panel->set_visible(true);
		
		for (int i = 0; i < p_message.tool_results.size(); i++) {
			Dictionary tool_result = p_message.tool_results[i];
			
			// Check if this is an image generation result
			if (tool_result.get("success", false) && tool_result.has("image_data")) {
				String image_data = tool_result.get("image_data", "");
				String prompt = tool_result.get("prompt", "Generated Image");
				
				if (!image_data.is_empty()) {
					print_line("AI Chat: Displaying saved image from tool result: " + prompt + " (" + String::num(image_data.length()) + " chars base64)");
					_display_generated_image_in_tool_result(message_vbox, image_data, tool_result);
				}
			}
		}
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
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	for (int i = 0; i < chat_history.size(); i++) {
		const ChatMessage &msg = chat_history[i];
		Dictionary api_msg;
		api_msg["role"] = msg.role;
		
		// For user messages with attached files, handle images and text differently
		if (msg.role == "user" && !msg.attached_files.is_empty()) {
			// Check if we have any images
			bool has_images = false;
			for (const AttachedFile &file : msg.attached_files) {
				if (file.is_image) {
					has_images = true;
					break;
				}
			}
			
			if (has_images) {
				// Use the new multi-modal format for images
				Array content_array;
				
				// Add text content
				if (!msg.content.is_empty()) {
					Dictionary text_part;
					text_part["type"] = "text";
					text_part["text"] = msg.content;
					content_array.push_back(text_part);
				}
				
				// Add images and text files
				for (const AttachedFile &file : msg.attached_files) {
					if (file.is_image) {
						Dictionary image_part;
						image_part["type"] = "image_url";
						Dictionary image_url;
						image_url["url"] = "data:" + file.mime_type + ";base64," + file.base64_data;
						image_part["image_url"] = image_url;
						content_array.push_back(image_part);
					} else {
						// Add text files as text content
						Dictionary text_part;
						text_part["type"] = "text";
						text_part["text"] = "\n\n**File: " + file.name + " (" + file.path + ")**\n```\n" + file.content + "\n```\n";
						content_array.push_back(text_part);
					}
				}
				
				api_msg["content"] = content_array;
			} else {
				// Legacy format for text-only attachments
				String combined_content = msg.content;
				combined_content += "\n\n**Attached Files:**\n";
				
				for (const AttachedFile &file : msg.attached_files) {
					combined_content += "\n### " + file.name + " (" + file.path + ")\n";
					combined_content += "```\n" + file.content + "\n```\n";
				}
				
				api_msg["content"] = combined_content;
			}
		} else {
			api_msg["content"] = msg.content;
		}
		
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

	// Debug: Print the messages being sent to OpenAI
	print_line("=== AI CHAT DEBUG: Messages being sent to OpenAI ===");
	for (int i = 0; i < messages.size(); i++) {
		Dictionary msg = messages[i];
		print_line("Message " + String::num_int64(i) + ":");
		print_line("  Role: " + String(msg.get("role", "")));
		print_line("  Content: " + String(msg.get("content", "")).substr(0, 200) + (String(msg.get("content", "")).length() > 200 ? "..." : ""));
		if (msg.has("tool_calls")) {
			Array tool_calls = msg.get("tool_calls", Array());
			print_line("  Tool calls: " + String::num_int64(tool_calls.size()));
		}
		if (msg.has("tool_call_id")) {
			print_line("  Tool call ID: " + String(msg.get("tool_call_id", "")));
		}
		if (msg.has("name")) {
			print_line("  Name: " + String(msg.get("name", "")));
		}
		print_line("---");
	}
	print_line("=== End OpenAI Messages Debug ===");

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
					processed_line = "[indent]* " + _process_inline_markdown(item_content) + "[/indent]";
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
	conversations.clear();
	current_conversation_index = -1;
	_save_conversations();
	_update_conversation_dropdown();

	// Clear UI
	if (chat_container != nullptr) {
		for (int i = chat_container->get_child_count() - 1; i >= 0; i--) {
			Node *child = chat_container->get_child(i);
			if (child != nullptr) {
				child->queue_free();
			}
		}
	}
}

void AIChatDock::clear_current_conversation() {
	if (current_conversation_index >= 0) {
		Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
		chat_history.clear();
		
		// Update conversation timestamp
		conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
		_save_conversations();

		// Clear UI
		if (chat_container != nullptr) {
			for (int i = chat_container->get_child_count() - 1; i >= 0; i--) {
				Node *child = chat_container->get_child(i);
				if (child != nullptr) {
					child->queue_free();
				}
			}
		}
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

// Conversation management methods
void AIChatDock::_load_conversations() {
	if (!FileAccess::exists(conversations_file_path)) {
		return;
	}
	
	Error err;
	String file_content = FileAccess::get_file_as_string(conversations_file_path, &err);
	if (err != OK) {
		print_line("AI Chat: Failed to load conversations file");
		return;
	}
	
	Ref<JSON> json;
	json.instantiate();
	err = json->parse(file_content);
	if (err != OK) {
		print_line("AI Chat: Failed to parse conversations file");
		return;
	}
	
	Dictionary data = json->get_data();
	if (!data.has("conversations")) {
		return;
	}
	
	Array conversations_array = data["conversations"];
	conversations.clear();
	
	for (int i = 0; i < conversations_array.size(); i++) {
		Dictionary conv_dict = conversations_array[i];
		AIChatDock::Conversation conv;
		conv.id = conv_dict.get("id", "");
		conv.title = conv_dict.get("title", "");
		conv.created_timestamp = conv_dict.get("created_timestamp", "");
		conv.last_modified_timestamp = conv_dict.get("last_modified_timestamp", "");
		
		Array messages_array = conv_dict.get("messages", Array());
		for (int j = 0; j < messages_array.size(); j++) {
			Dictionary msg_dict = messages_array[j];
			AIChatDock::ChatMessage msg;
			msg.role = msg_dict.get("role", "");
			msg.content = msg_dict.get("content", "");
			msg.timestamp = msg_dict.get("timestamp", "");
			msg.tool_calls = msg_dict.get("tool_calls", Array());
			msg.tool_call_id = msg_dict.get("tool_call_id", "");
			msg.name = msg_dict.get("name", "");
			
			// Load attached files
			Array attached_files_array = msg_dict.get("attached_files", Array());
			for (int k = 0; k < attached_files_array.size(); k++) {
				Dictionary file_dict = attached_files_array[k];
				AIChatDock::AttachedFile file;
				file.path = file_dict.get("path", "");
				file.name = file_dict.get("name", "");
				file.content = file_dict.get("content", "");
				file.is_image = file_dict.get("is_image", false);
				file.mime_type = file_dict.get("mime_type", "");
				file.base64_data = file_dict.get("base64_data", "");
				file.original_size.x = file_dict.get("original_size_x", 0);
				file.original_size.y = file_dict.get("original_size_y", 0);
				file.display_size.x = file_dict.get("display_size_x", 0);
				file.display_size.y = file_dict.get("display_size_y", 0);
				file.was_downsampled = file_dict.get("was_downsampled", false);
				msg.attached_files.push_back(file);
			}
			
			// Load tool results 
			msg.tool_results = msg_dict.get("tool_results", Array());
			
			conv.messages.push_back(msg);
		}
		conversations.push_back(conv);
	}
}

void AIChatDock::_save_conversations() {
	Dictionary data;
	Array conversations_array;
	
	for (int i = 0; i < conversations.size(); i++) {
		const AIChatDock::Conversation &conv = conversations[i];
		Dictionary conv_dict;
		conv_dict["id"] = conv.id;
		conv_dict["title"] = conv.title;
		conv_dict["created_timestamp"] = conv.created_timestamp;
		conv_dict["last_modified_timestamp"] = conv.last_modified_timestamp;
		
		Array messages_array;
		for (int j = 0; j < conv.messages.size(); j++) {
			const ChatMessage &msg = conv.messages[j];
			Dictionary msg_dict;
			msg_dict["role"] = msg.role;
			msg_dict["content"] = msg.content;
			msg_dict["timestamp"] = msg.timestamp;
			msg_dict["tool_calls"] = msg.tool_calls;
			msg_dict["tool_call_id"] = msg.tool_call_id;
			msg_dict["name"] = msg.name;
			
			// Save attached files
			Array attached_files_array;
			for (int k = 0; k < msg.attached_files.size(); k++) {
				const AttachedFile &file = msg.attached_files[k];
				Dictionary file_dict;
				file_dict["path"] = file.path;
				file_dict["name"] = file.name;
				file_dict["content"] = file.content;
				file_dict["is_image"] = file.is_image;
				file_dict["mime_type"] = file.mime_type;
				file_dict["base64_data"] = file.base64_data;
				file_dict["original_size_x"] = file.original_size.x;
				file_dict["original_size_y"] = file.original_size.y;
				file_dict["display_size_x"] = file.display_size.x;
				file_dict["display_size_y"] = file.display_size.y;
				file_dict["was_downsampled"] = file.was_downsampled;
				attached_files_array.push_back(file_dict);
			}
			msg_dict["attached_files"] = attached_files_array;
			msg_dict["tool_results"] = msg.tool_results;
			
			messages_array.push_back(msg_dict);
		}
		conv_dict["messages"] = messages_array;
		conversations_array.push_back(conv_dict);
	}
	
	data["conversations"] = conversations_array;
	
	Ref<JSON> json;
	json.instantiate();
	String json_string = json->stringify(data, "  ");
	
	Error err;
	Ref<FileAccess> file = FileAccess::open(conversations_file_path, FileAccess::WRITE, &err);
	if (err != OK) {
		print_line("AI Chat: Failed to save conversations file");
		return;
	}
	
	file->store_string(json_string);
	file->close();
}

void AIChatDock::_create_new_conversation() {
	AIChatDock::Conversation new_conv;
	new_conv.id = _generate_conversation_id();
	new_conv.title = "New Conversation";
	new_conv.created_timestamp = _get_timestamp();
	new_conv.last_modified_timestamp = _get_timestamp();
	
	conversations.push_back(new_conv);
	current_conversation_index = conversations.size() - 1;
	_save_conversations();
	
	// Clear UI for new conversation
	if (chat_container != nullptr) {
		for (int i = chat_container->get_child_count() - 1; i >= 0; i--) {
			Node *child = chat_container->get_child(i);
			if (child != nullptr) {
				child->queue_free();
			}
		}
	}
}

void AIChatDock::_switch_to_conversation(int p_index) {
	if (p_index < 0 || p_index >= conversations.size()) {
		return;
	}
	
	current_conversation_index = p_index;
	
	// Clear current UI
	if (chat_container != nullptr) {
		for (int i = chat_container->get_child_count() - 1; i >= 0; i--) {
			Node *child = chat_container->get_child(i);
			if (child != nullptr) {
				child->queue_free();
			}
		}
	}
	
	// Rebuild UI from conversation messages
	const Vector<AIChatDock::ChatMessage> &messages = conversations[p_index].messages;
	for (int i = 0; i < messages.size(); i++) {
		_create_message_bubble(messages[i]);
	}
	
	call_deferred("_scroll_to_bottom");
}

void AIChatDock::_update_conversation_dropdown() {
	if (!conversation_history_dropdown) {
		return;
	}
	
	conversation_history_dropdown->clear();
	
	for (int i = 0; i < conversations.size(); i++) {
		const AIChatDock::Conversation &conv = conversations[i];
		String display_title = conv.title;
		if (display_title.length() > 30) {
			display_title = display_title.substr(0, 27) + "...";
		}
		display_title += " (" + conv.last_modified_timestamp + ")";
		conversation_history_dropdown->add_item(display_title);
	}
	
	if (current_conversation_index >= 0) {
		conversation_history_dropdown->select(current_conversation_index);
	}
}

String AIChatDock::_generate_conversation_id() {
	return "conv_" + String::num_uint64(Time::get_singleton()->get_unix_time_from_system()) + "_" + String::num(Math::rand() % 10000);
}

String AIChatDock::_generate_conversation_title(const Vector<AIChatDock::ChatMessage> &p_messages) {
	// Generate title from first user message
	for (int i = 0; i < p_messages.size(); i++) {
		if (p_messages[i].role == "user" && !p_messages[i].content.is_empty()) {
			String content = p_messages[i].content.strip_edges();
			if (content.length() > 50) {
				content = content.substr(0, 47) + "...";
			}
			return content;
		}
	}
	return "New Conversation";
}

Vector<AIChatDock::ChatMessage> &AIChatDock::_get_current_chat_history() {
	static Vector<AIChatDock::ChatMessage> empty_history;
	if (current_conversation_index >= 0 && current_conversation_index < conversations.size()) {
		return conversations.write[current_conversation_index].messages;
	}
	return empty_history;
}

void AIChatDock::_on_conversation_selected(int p_index) {
	if (p_index != current_conversation_index) {
		_switch_to_conversation(p_index);
	}
}

void AIChatDock::_on_new_conversation_pressed() {
	_create_new_conversation();
	_update_conversation_dropdown();
}

AIChatDock::AIChatDock() {
	set_name("AI Chat");

	// HTTP request for API calls
	http_client = HTTPClient::create();

	diff_viewer = memnew(DiffViewer);
	add_child(diff_viewer);
	diff_viewer->connect("diff_accepted", callable_mp(this, &AIChatDock::_on_diff_accepted));

	// Start the AI tool server
	tool_server.instantiate();
	Error err = tool_server->listen(8001);
	if (err == OK) {
		print_line("AI Chat Dock: Tool server started on port 8001");
	} else {
		print_line("AI Chat Dock: Failed to start tool server on port 8001");
	}
}

void AIChatDock::_on_diff_accepted(const String &p_path, const String &p_content) {
    // Changes are now applied directly to script editor
    // No need to write to disk immediately - user can save when ready
    print_line("Diff accepted for: " + p_path);
}

// Image processing methods
bool AIChatDock::_is_image_file(const String &p_path) {
	String ext = p_path.get_extension().to_lower();
	return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || 
		   ext == "bmp" || ext == "webp" || ext == "svg";
}

String AIChatDock::_get_mime_type_from_extension(const String &p_path) {
	String ext = p_path.get_extension().to_lower();
	if (ext == "png") return "image/png";
	if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
	if (ext == "gif") return "image/gif";
	if (ext == "bmp") return "image/bmp";
	if (ext == "webp") return "image/webp";
	if (ext == "svg") return "image/svg+xml";
	return "text/plain";
}

bool AIChatDock::_process_image_attachment(AttachedFile &p_file) {
	Ref<Image> image = Image::load_from_file(p_file.path);
	if (image.is_null() || image->is_empty()) {
		return false;
	}

	Vector2i original_size = Vector2i(image->get_width(), image->get_height());
	p_file.original_size = original_size;
	
	// Check if image needs to be downsampled (max 1024px on any side)
	const int MAX_DIMENSION = 1024;
	Vector2i target_size = _calculate_downsampled_size(original_size, MAX_DIMENSION);
	
	if (target_size != original_size) {
		p_file.was_downsampled = true;
		image->resize(target_size.x, target_size.y, Image::INTERPOLATE_LANCZOS);
		
		// Show warning dialog
		call_deferred("_show_image_warning_dialog", p_file.name, original_size, target_size);
	}
	
	p_file.display_size = target_size;
	
	// Convert to base64 for API transmission
	Vector<uint8_t> png_buffer;
	if (p_file.mime_type == "image/jpeg" || p_file.mime_type == "image/jpg") {
		png_buffer = image->save_jpg_to_buffer(0.85f); // Good quality JPEG
	} else {
		png_buffer = image->save_png_to_buffer();
	}
	
	if (png_buffer.size() == 0) {
		return false;
	}
	
	// Encode to base64
	p_file.base64_data = CoreBind::Marshalls::get_singleton()->raw_to_base64(png_buffer);
	
	return true;
}

Vector2i AIChatDock::_calculate_downsampled_size(const Vector2i &p_original, int p_max_dimension) {
	if (p_original.x <= p_max_dimension && p_original.y <= p_max_dimension) {
		return p_original;
	}
	
	float aspect_ratio = (float)p_original.x / (float)p_original.y;
	Vector2i new_size;
	
	if (p_original.x > p_original.y) {
		// Landscape
		new_size.x = p_max_dimension;
		new_size.y = (int)(p_max_dimension / aspect_ratio);
	} else {
		// Portrait or square
		new_size.y = p_max_dimension;
		new_size.x = (int)(p_max_dimension * aspect_ratio);
	}
	
	return new_size;
}

void AIChatDock::_show_image_warning_dialog(const String &p_filename, const Vector2i &p_original, const Vector2i &p_new_size) {
	if (!image_warning_dialog) {
		return;
	}
	
	String message = String("Image '{0}' was downsampled from {1}×{2} to {3}×{4} to reduce file size for transmission.")
		.format(varray(p_filename, p_original.x, p_original.y, p_new_size.x, p_new_size.y));
	
	image_warning_dialog->set_text(message);
	image_warning_dialog->popup_centered(Size2i(500, 150));
}

void AIChatDock::_handle_generated_image(const String &p_base64_data, const String &p_id) {
	if (p_base64_data.is_empty()) {
		return;
	}
	
	// Defer image display to next frame to avoid UI race conditions during streaming
	call_deferred("_display_generated_image_deferred", p_base64_data, p_id);
}

void AIChatDock::_display_generated_image_deferred(const String &p_base64_data, const String &p_id) {
	// Decode base64 to image
	Vector<uint8_t> image_data = CoreBind::Marshalls::get_singleton()->base64_to_raw(p_base64_data);
	if (image_data.size() == 0) {
		print_line("AI Chat: Failed to decode generated image data");
		return;
	}
	
	// Create image from data
	Ref<Image> generated_image = memnew(Image);
	Error err = generated_image->load_png_from_buffer(image_data);
	if (err != OK) {
		// Try JPEG if PNG fails
		err = generated_image->load_jpg_from_buffer(image_data);
		if (err != OK) {
			print_line("AI Chat: Failed to load generated image");
			return;
		}
	}
	
	if (generated_image->is_empty()) {
		print_line("AI Chat: Generated image is empty");
		return;
	}
	
	// Create a container for the generated image
	RichTextLabel *label = _get_or_create_current_assistant_message_label();
	if (!label || !label->is_inside_tree()) {
		print_line("AI Chat: Current assistant message label is not valid or not in tree - creating new message bubble");
		// If the current label is invalid, create a new assistant message bubble
		_add_message_to_chat("assistant", "");
		label = current_assistant_message_label;
		if (!label || !label->is_inside_tree()) {
			print_line("AI Chat: Failed to create valid message label for generated image");
			return;
		}
	}
	
	// Find the message bubble container with safe traversal
	Control *bubble_panel = label;
	int max_traversal = 10; // Prevent infinite loops
	while (bubble_panel && max_traversal > 0 && !Object::cast_to<PanelContainer>(bubble_panel)) {
		Node *parent = bubble_panel->get_parent();
		if (!parent || !Object::cast_to<Control>(parent)) {
			break;
		}
		bubble_panel = Object::cast_to<Control>(parent);
		max_traversal--;
	}
	
	if (!bubble_panel || !Object::cast_to<PanelContainer>(bubble_panel)) {
		print_line("AI Chat: Could not find message bubble container for generated image");
		return;
	}
	
	// Find the VBoxContainer inside the message bubble
	VBoxContainer *message_vbox = nullptr;
	for (int i = 0; i < bubble_panel->get_child_count(); i++) {
		message_vbox = Object::cast_to<VBoxContainer>(bubble_panel->get_child(i));
		if (message_vbox) {
			break;
		}
	}
	
	if (!message_vbox) {
		return;
	}
	
	// Create image display container
	PanelContainer *image_panel = memnew(PanelContainer);
	message_vbox->add_child(image_panel);
	
	// Style the image panel
	Ref<StyleBoxFlat> image_style = memnew(StyleBoxFlat);
	image_style->set_bg_color(get_theme_color(SNAME("dark_color_1"), SNAME("Editor")));
	image_style->set_border_width_all(2);
	image_style->set_border_color(get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	image_style->set_corner_radius_all(8);
	image_style->set_content_margin_all(8);
	image_panel->add_theme_style_override("panel", image_style);
	
	VBoxContainer *image_container = memnew(VBoxContainer);
	image_panel->add_child(image_container);
	
	// Add "Generated Image" label
	Label *header_label = memnew(Label);
	header_label->set_text("🎨 Generated Image");
	header_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
	header_label->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	image_container->add_child(header_label);
	
	// Resize image for display (max 512px to keep it reasonable)
	Vector2i original_size = Vector2i(generated_image->get_width(), generated_image->get_height());
	Vector2i display_size = _calculate_downsampled_size(original_size, 512);
	
	if (display_size != original_size) {
		generated_image->resize(display_size.x, display_size.y, Image::INTERPOLATE_LANCZOS);
	}
	
	// Create texture and display
	Ref<ImageTexture> generated_texture = ImageTexture::create_from_image(generated_image);
	
	TextureRect *image_display = memnew(TextureRect);
	image_display->set_texture(generated_texture);
	image_display->set_expand_mode(TextureRect::EXPAND_FIT_WIDTH_PROPORTIONAL);
	image_display->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	image_display->set_custom_minimum_size(Size2(display_size.x, display_size.y));
	image_container->add_child(image_display);
	
	// Add image info
	HBoxContainer *info_container = memnew(HBoxContainer);
	image_container->add_child(info_container);
	
	Label *size_label = memnew(Label);
	size_label->set_text(String::num_int64(original_size.x) + "×" + String::num_int64(original_size.y));
	size_label->add_theme_font_size_override("font_size", 10);
	size_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
	info_container->add_child(size_label);
	
	// Store the generated image in the current message
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	if (!chat_history.is_empty()) {
		ChatMessage &last_msg = chat_history.write[chat_history.size() - 1];
		if (last_msg.role == "assistant") {
			// Add the generated image as an attachment for persistence
			AIChatDock::AttachedFile generated_file;
			generated_file.path = "generated://" + p_id;
			generated_file.name = "Generated Image " + p_id;
			generated_file.is_image = true;
			generated_file.mime_type = "image/png";
			generated_file.base64_data = p_base64_data;
			generated_file.original_size = original_size;
			generated_file.display_size = display_size;
			generated_file.was_downsampled = (display_size != original_size);
			last_msg.attached_files.push_back(generated_file);
		}
	}
	
	// Update conversation and scroll
	if (current_conversation_index >= 0) {
		conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
		_save_conversations();
	}
	
	call_deferred("_scroll_to_bottom");
}

void AIChatDock::_display_generated_image_in_tool_result(VBoxContainer *p_container, const String &p_base64_data, const Dictionary &p_data) {
	if (!p_container || p_base64_data.is_empty()) {
		return;
	}
	
	// Decode base64 to image
	Vector<uint8_t> image_data = CoreBind::Marshalls::get_singleton()->base64_to_raw(p_base64_data);
	if (image_data.size() == 0) {
		print_line("AI Chat: Failed to decode generated image data from tool result");
		return;
	}
	
	// Create image from data
	Ref<Image> generated_image = memnew(Image);
	Error err = generated_image->load_png_from_buffer(image_data);
	if (err != OK) {
		// Try JPEG if PNG fails
		err = generated_image->load_jpg_from_buffer(image_data);
		if (err != OK) {
			print_line("AI Chat: Failed to load generated image from tool result");
			return;
		}
	}
	
	if (generated_image->is_empty()) {
		print_line("AI Chat: Generated image from tool result is empty");
		return;
	}
	
	// Create image display container
	VBoxContainer *image_container = memnew(VBoxContainer);
	p_container->add_child(image_container);
	
	// Add image info
	HBoxContainer *info_container = memnew(HBoxContainer);
	image_container->add_child(info_container);
	
	HBoxContainer *prompt_container = memnew(HBoxContainer);
	info_container->add_child(prompt_container);
	
	Label *prompt_icon = memnew(Label);
	prompt_icon->add_theme_icon_override("icon", get_theme_icon(SNAME("Image"), SNAME("EditorIcons")));
	prompt_container->add_child(prompt_icon);
	
	Label *prompt_label = memnew(Label);
	String prompt = p_data.get("prompt", "Generated Image");
	prompt_label->set_text(prompt);
	prompt_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
	prompt_label->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	prompt_container->add_child(prompt_label);
	
	// Resize image for display (max 200px in tool results to keep them compact)
	Vector2i original_size = Vector2i(generated_image->get_width(), generated_image->get_height());
	Vector2i display_size = _calculate_downsampled_size(original_size, 200);
	
	if (display_size != original_size) {
		generated_image->resize(display_size.x, display_size.y, Image::INTERPOLATE_LANCZOS);
	}
	
	// Create texture and display
	Ref<ImageTexture> generated_texture = ImageTexture::create_from_image(generated_image);
	
	TextureRect *image_display = memnew(TextureRect);
	image_display->set_texture(generated_texture);
	image_display->set_expand_mode(TextureRect::EXPAND_FIT_WIDTH_PROPORTIONAL);
	image_display->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	image_display->set_custom_minimum_size(Size2(display_size.x, display_size.y));
	image_container->add_child(image_display);
	
	// Add technical details and save button
	HBoxContainer *tech_container = memnew(HBoxContainer);
	image_container->add_child(tech_container);
	
	Label *size_label = memnew(Label);
	size_label->set_text(String::num_int64(original_size.x) + "×" + String::num_int64(original_size.y));
	size_label->add_theme_font_size_override("font_size", 10);
	size_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
	tech_container->add_child(size_label);
	
	Label *model_label = memnew(Label);
	String model = p_data.get("model", "DALL-E");
	model_label->set_text(" • " + model);
	model_label->add_theme_font_size_override("font_size", 10);
	model_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
	tech_container->add_child(model_label);
	
	// Add spacer to push save button to the right
	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tech_container->add_child(spacer);
	
	// Save button
	Button *save_button = memnew(Button);
	save_button->set_text("Save to...");
	save_button->set_flat(true);
	save_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Save"), SNAME("EditorIcons")));
	save_button->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	save_button->add_theme_color_override("icon_normal_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	save_button->set_tooltip_text("Save this image to your project");
	save_button->connect("pressed", callable_mp(this, &AIChatDock::_on_save_image_pressed).bind(p_base64_data, "png"));
	tech_container->add_child(save_button);
}

void AIChatDock::_on_save_image_pressed(const String &p_base64_data, const String &p_format) {
	if (p_base64_data.is_empty()) {
		return;
	}
	
	// Store the image data for saving
	pending_save_image_data = p_base64_data;
	pending_save_image_format = p_format;
	
	// Set up the save dialog
	save_image_dialog->set_current_file("generated_image." + p_format);
	save_image_dialog->popup_centered(Size2(800, 600));
}

void AIChatDock::_on_save_image_location_selected(const Vector<String> &p_files) {
	if (p_files.size() == 0 || pending_save_image_data.is_empty()) {
		return;
	}
	
	String save_path = p_files[0];
	
	// Decode base64 to image data
	Vector<uint8_t> image_data = CoreBind::Marshalls::get_singleton()->base64_to_raw(pending_save_image_data);
	if (image_data.size() == 0) {
		print_line("AI Chat: Failed to decode image data for saving");
		return;
	}
	
	// Save the image file
	Ref<FileAccess> file = FileAccess::open(save_path, FileAccess::WRITE);
	if (file.is_null()) {
		print_line("AI Chat: Failed to open file for writing: " + save_path);
		return;
	}
	
	file->store_buffer(image_data);
	file->close();
	
	print_line("AI Chat: Image saved successfully to: " + save_path);
	
	// Clear pending data
	pending_save_image_data = "";
	pending_save_image_format = "";
}

