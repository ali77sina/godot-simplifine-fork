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
#include "core/config/project_settings.h"
#include "scene/resources/image_texture.h"
#include "editor/editor_node.h"
#include "editor/editor_interface.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_settings.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/script/script_text_editor.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/menu_button.h"
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
#include "scene/gui/popup_menu.h"
#include "scene/main/http_request.h"
#include "scene/resources/style_box_flat.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"
#include "core/string/string_name.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/gui/popup.h"
#include "scene/gui/control.h"

#include "../ai/editor_tools.h"
#include "diff_viewer.h"

void AIChatDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_send_button_pressed"), &AIChatDock::_on_send_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_stop_button_pressed"), &AIChatDock::_on_stop_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_stop_request_completed"), &AIChatDock::_on_stop_request_completed);
	ClassDB::bind_method(D_METHOD("_on_edit_message_pressed"), &AIChatDock::_on_edit_message_pressed);
	ClassDB::bind_method(D_METHOD("_on_edit_message_send_pressed"), &AIChatDock::_on_edit_message_send_pressed);
	ClassDB::bind_method(D_METHOD("_on_edit_send_button_pressed"), &AIChatDock::_on_edit_send_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_edit_message_cancel_pressed"), &AIChatDock::_on_edit_message_cancel_pressed);
	ClassDB::bind_method(D_METHOD("_on_edit_field_gui_input"), &AIChatDock::_on_edit_field_gui_input);
	ClassDB::bind_method(D_METHOD("_process_send_request_async"), &AIChatDock::_process_send_request_async);
	ClassDB::bind_method(D_METHOD("_save_conversations_async"), &AIChatDock::_save_conversations_async);
	ClassDB::bind_method(D_METHOD("_on_input_text_changed"), &AIChatDock::_on_input_text_changed);
	ClassDB::bind_method(D_METHOD("_update_at_mention_popup"), &AIChatDock::_update_at_mention_popup);
	ClassDB::bind_method(D_METHOD("_populate_at_mention_tree"), &AIChatDock::_populate_at_mention_tree);
	ClassDB::bind_method(D_METHOD("_populate_tree_recursive"), &AIChatDock::_populate_tree_recursive);
	ClassDB::bind_method(D_METHOD("_on_at_mention_item_selected"), &AIChatDock::_on_at_mention_item_selected);
	ClassDB::bind_method(D_METHOD("_on_input_field_gui_input"), &AIChatDock::_on_input_field_gui_input);
	ClassDB::bind_method(D_METHOD("_on_model_selected"), &AIChatDock::_on_model_selected);
	ClassDB::bind_method(D_METHOD("_on_tool_output_toggled"), &AIChatDock::_on_tool_output_toggled);
	ClassDB::bind_method(D_METHOD("_on_tool_file_link_pressed", "path"), &AIChatDock::_on_tool_file_link_pressed);

	ClassDB::bind_method(D_METHOD("_on_attachment_menu_item_pressed"), &AIChatDock::_on_attachment_menu_item_pressed);
	ClassDB::bind_method(D_METHOD("_on_attach_files_pressed"), &AIChatDock::_on_attach_files_pressed);
	ClassDB::bind_method(D_METHOD("_on_attach_scene_nodes_pressed"), &AIChatDock::_on_attach_scene_nodes_pressed);
	ClassDB::bind_method(D_METHOD("_on_attach_current_script_pressed"), &AIChatDock::_on_attach_current_script_pressed);
	ClassDB::bind_method(D_METHOD("_on_attach_resources_pressed"), &AIChatDock::_on_attach_resources_pressed);
	ClassDB::bind_method(D_METHOD("_on_scene_tree_node_selected"), &AIChatDock::_on_scene_tree_node_selected);
	ClassDB::bind_method(D_METHOD("_on_files_selected"), &AIChatDock::_on_files_selected);
	ClassDB::bind_method(D_METHOD("_on_remove_attachment", "path"), &AIChatDock::_on_remove_attachment);
	ClassDB::bind_method(D_METHOD("_on_conversation_selected"), &AIChatDock::_on_conversation_selected);
	ClassDB::bind_method(D_METHOD("_on_new_conversation_pressed"), &AIChatDock::_on_new_conversation_pressed);
	ClassDB::bind_method(D_METHOD("_on_save_image_pressed", "base64_data", "format"), &AIChatDock::_on_save_image_pressed);
	ClassDB::bind_method(D_METHOD("_on_save_image_location_selected", "file_path"), &AIChatDock::_on_save_image_location_selected);

	ClassDB::bind_method(D_METHOD("_save_conversations_to_disk"), &AIChatDock::_save_conversations_to_disk);
	ClassDB::bind_method(D_METHOD("_process_image_attachment_async"), &AIChatDock::_process_image_attachment_async);
	ClassDB::bind_method(D_METHOD("_send_chat_request_chunked"), &AIChatDock::_send_chat_request_chunked);
	ClassDB::bind_method(D_METHOD("_apply_tool_result_deferred"), &AIChatDock::_apply_tool_result_deferred);
	ClassDB::bind_method(D_METHOD("_create_assistant_message_with_tool_placeholder"), &AIChatDock::_create_assistant_message_with_tool_placeholder);
	ClassDB::bind_method(D_METHOD("_finalize_chat_request"), &AIChatDock::_finalize_chat_request);
	ClassDB::bind_method(D_METHOD("_save_conversations_chunked"), &AIChatDock::_save_conversations_chunked);
	ClassDB::bind_method(D_METHOD("_finalize_conversations_save"), &AIChatDock::_finalize_conversations_save);
	ClassDB::bind_method(D_METHOD("_execute_delayed_save"), &AIChatDock::_execute_delayed_save);
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
			
			// Setup authentication UI
			_setup_authentication_ui();

			// Container for attached files (initially hidden) - will be added to bottom panel later
			attached_files_container = memnew(HFlowContainer);
			attached_files_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			attached_files_container->add_theme_constant_override("h_separation", 6); // Horizontal spacing between attachment tabs
			attached_files_container->add_theme_constant_override("v_separation", 4); // Vertical spacing if wrapping
			attached_files_container->set_visible(false);

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
		save_image_dialog->connect("file_selected", callable_mp(this, &AIChatDock::_on_save_image_location_selected));
		add_child(save_image_dialog);

		// --- At-Mention Popup ---
		at_mention_popup = memnew(PopupPanel);
		at_mention_popup->set_name("at_mention_popup");
		at_mention_popup->set_size(Size2i(300, 400));
		
		VBoxContainer *at_mention_vbox = memnew(VBoxContainer);
		at_mention_popup->add_child(at_mention_vbox);
		
		at_mention_tree = memnew(Tree);
		at_mention_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		at_mention_tree->connect("item_activated", callable_mp(this, &AIChatDock::_on_at_mention_item_selected));
		at_mention_vbox->add_child(at_mention_tree);
		
		add_child(at_mention_popup);



		// --- Scene Tree Popup ---
		scene_tree_popup = memnew(PopupPanel);
		scene_tree_popup->set_name("scene_tree_popup");
		scene_tree_popup->set_size(Size2i(400, 500));
		
		VBoxContainer *scene_tree_vbox = memnew(VBoxContainer);
		scene_tree_popup->add_child(scene_tree_vbox);
		
		Label *scene_tree_label = memnew(Label);
		scene_tree_label->set_text("Select Scene Nodes to Attach:");
		scene_tree_vbox->add_child(scene_tree_label);
		
		scene_tree = memnew(Tree);
		scene_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		scene_tree->set_select_mode(Tree::SELECT_MULTI);
		scene_tree->connect("item_activated", callable_mp(this, &AIChatDock::_on_scene_tree_node_selected));
		scene_tree_vbox->add_child(scene_tree);
		
		HBoxContainer *scene_tree_buttons = memnew(HBoxContainer);
		scene_tree_vbox->add_child(scene_tree_buttons);
		
		Button *attach_selected_button = memnew(Button);
		attach_selected_button->set_text("Attach Selected");
		attach_selected_button->connect("pressed", callable_mp(this, &AIChatDock::_on_scene_tree_node_selected));
		scene_tree_buttons->add_child(attach_selected_button);
		
		Control *spacer_control = memnew(Control); // Spacer
		spacer_control->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		scene_tree_buttons->add_child(spacer_control);
		
		Button *scene_tree_cancel_button = memnew(Button);
		scene_tree_cancel_button->set_text("Cancel");
		scene_tree_cancel_button->connect("pressed", Callable(scene_tree_popup, "hide"));
		scene_tree_buttons->add_child(scene_tree_cancel_button);
		
		add_child(scene_tree_popup);

		// --- Resource Dialog ---
		resource_dialog = memnew(EditorFileDialog);
		resource_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILES);
		resource_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
		resource_dialog->add_filter("*.tres, *.res", "Resources");
		resource_dialog->add_filter("*.tscn, *.scn", "Scenes");
		resource_dialog->add_filter("*.png, *.jpg, *.jpeg, *.svg", "Textures");
		resource_dialog->add_filter("*.ogg, *.wav, *.mp3", "Audio");
		resource_dialog->add_filter("*", "All Files");
		resource_dialog->connect("files_selected", callable_mp(this, &AIChatDock::_on_files_selected));
		add_child(resource_dialog);

			// Chat history area - expand to fill available space
			chat_scroll = memnew(ScrollContainer);
			chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			chat_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
			add_child(chat_scroll);

			chat_container = memnew(VBoxContainer);
			chat_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			chat_scroll->add_child(chat_container);

			// Add a container for attachments just above the input field
			VBoxContainer *bottom_panel = memnew(VBoxContainer);
			add_child(bottom_panel);
			
			// Add attachments container to the bottom panel (positioned above input)
			bottom_panel->add_child(attached_files_container);
			
			// Attach files button row (above input)
			HBoxContainer *attach_container = memnew(HBoxContainer);
			bottom_panel->add_child(attach_container);

			// Add spacer to push button to the right
			Control *spacer = memnew(Control);
			spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			attach_container->add_child(spacer);

			// Attach files button
			attach_button = memnew(MenuButton);
			attach_button->set_text("Attach");
			attach_button->set_tooltip_text("Attach project files to your message");
			attach_button->add_theme_icon_override("icon", get_theme_icon(SNAME("FileList"), SNAME("EditorIcons")));
			attach_button->set_custom_minimum_size(Size2(80, 32));
			
			// Set up the popup menu
			PopupMenu *popup = attach_button->get_popup();
			popup->add_item("Files", 0);
			popup->set_item_icon(0, get_theme_icon(SNAME("FileList"), SNAME("EditorIcons")));
			popup->add_item("Scene Nodes", 1);
			popup->set_item_icon(1, get_theme_icon(SNAME("SceneTree"), SNAME("EditorIcons")));
			popup->add_item("Current Script", 2);
			popup->set_item_icon(2, get_theme_icon(SNAME("Script"), SNAME("EditorIcons")));
			popup->add_item("Resources", 3);
			popup->set_item_icon(3, get_theme_icon(SNAME("ResourcePreloader"), SNAME("EditorIcons")));
			popup->connect("id_pressed", callable_mp(this, &AIChatDock::_on_attachment_menu_item_pressed));
			
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

			// Stop button (initially hidden)
			stop_button = memnew(Button);
			stop_button->set_text("Stop");
			stop_button->set_visible(false); // Hidden by default
			stop_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Stop"), SNAME("EditorIcons")));
			stop_button->set_custom_minimum_size(Size2(80, 40));
			
			// Stop button styling (red theme)
			Ref<StyleBoxFlat> stop_button_style = memnew(StyleBoxFlat);
			stop_button_style->set_bg_color(Color(0.8, 0.2, 0.2)); // Red color
			stop_button_style->set_corner_radius_all(6);
			stop_button_style->set_content_margin_all(8);
			stop_button->add_theme_style_override("normal", stop_button_style);
			
			// Stop button hover style
			Ref<StyleBoxFlat> stop_button_hover_style = memnew(StyleBoxFlat);
			stop_button_hover_style->set_bg_color(Color(0.9, 0.3, 0.3)); // Lighter red on hover
			stop_button_hover_style->set_corner_radius_all(6);
			stop_button_hover_style->set_content_margin_all(8);
			stop_button->add_theme_style_override("hover", stop_button_hover_style);
			
			stop_button->connect("pressed", callable_mp(this, &AIChatDock::_on_stop_button_pressed));
			input_container->add_child(stop_button);

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
					if (stream_completed_successfully) {
						print_line("AI Chat: Stream completed successfully, server closed connection");
					} else {
						print_line("AI Chat: HTTP connection failed with status: " + String::num_int64(client_status));
						_add_message_to_chat("system", "Connection lost or failed (Status: " + String::num_int64(client_status) + ")");
					}
					
					is_waiting_for_response = false;
					// Clean up stop mechanism state
					stop_requested = false;
					current_request_id = "";
					_update_ui_state();
					http_status = STATUS_DONE;
					current_assistant_message_label = nullptr;
					set_process(false);
					
					// CRITICAL: Save conversation when request completes to ensure no data loss
					if (current_conversation_index >= 0) {
						conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
						_queue_delayed_save();
					}
				}
			}
		} break;
		case NOTIFICATION_ENTER_TREE: {
			// Load API key from editor settings
			if (EditorSettings::get_singleton()->has_setting("ai_chat/api_key")) {
				api_key = EditorSettings::get_singleton()->get_setting("ai_chat/api_key");
			}
		} break;
		case NOTIFICATION_READY: {
			// Auto-verify saved authentication when everything is fully ready
			_auto_verify_saved_credentials();
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

	// Auto-suggest relevant files based on message content
	if (embedding_system_initialized && initial_indexing_done) {
		_auto_attach_relevant_context();
	}

	// PERFORMANCE OPTIMIZATION: Instant UI feedback with deferred heavy processing
	// This prevents UI freezing by splitting operations across frames
	input_field->set_text("");
	is_waiting_for_response = true;
	_update_ui_state();
	
	// Create message with attached files (lightweight operation)
	AIChatDock::ChatMessage msg;
	msg.role = "user";
	msg.content = message;
	msg.timestamp = _get_timestamp();
	msg.attached_files = current_attached_files;
	
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	chat_history.push_back(msg);
	_create_message_bubble(msg, chat_history.size() - 1);
	
	// Clear attachments immediately for instant feedback
	_clear_attachments();
	
	// Reset stream completion flag for new request
	stream_completed_successfully = false;
	
	// Defer heavy processing to next frame to keep UI responsive
	call_deferred("_process_send_request_async");
	input_field->grab_focus();
}

void AIChatDock::_on_stop_button_pressed() {
	print_line("AI Chat: Stop button pressed! is_waiting_for_response=" + String(is_waiting_for_response ? "true" : "false") + ", current_request_id='" + current_request_id + "'");
	
	if (!is_waiting_for_response || current_request_id.is_empty()) {
		print_line("AI Chat: Stop button press ignored - not waiting for response or no request ID");
		return;
	}
	
	print_line("AI Chat: Stop button pressed, sending stop request for: " + current_request_id);
	stop_requested = true;
	_send_stop_request();
}

void AIChatDock::_send_stop_request() {
	if (current_request_id.is_empty()) {
		print_line("AI Chat: Cannot send stop request - no current request ID");
		return;
	}
	
	// Prepare stop request data
	Dictionary request_data;
	request_data["request_id"] = current_request_id;
	
	Ref<JSON> json;
	json.instantiate();
	String request_body = json->stringify(request_data);
	
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	
	// Extract stop endpoint from main API endpoint
	String stop_endpoint = api_endpoint;
	if (stop_endpoint.ends_with("/chat")) {
		stop_endpoint = stop_endpoint.trim_suffix("/chat") + "/stop";
	} else {
		// Fallback - just add /stop to the base
		stop_endpoint += "/stop";
	}
	
	print_line("AI Chat: Sending stop request to: " + stop_endpoint);
	stop_http_request->request(stop_endpoint, headers, HTTPClient::METHOD_POST, request_body);
}

void AIChatDock::_on_stop_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	String response_body = String::utf8((const char *)p_body.ptr(), p_body.size());
	print_line("AI Chat: Stop request completed - Result: " + String::num(p_result) + ", Code: " + String::num(p_code) + ", Body: " + response_body);
	
	if (p_code == 200) {
		print_line("AI Chat: Stop request successful");
	} else {
		print_line("AI Chat: Stop request failed with code: " + String::num(p_code));
	}
}

void AIChatDock::_on_edit_message_pressed(int p_message_index) {
	print_line("AI Chat: Edit button pressed for message index: " + String::num(p_message_index));
	
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	print_line("AI Chat: Chat history size: " + String::num(chat_history.size()));
	
	if (p_message_index < 0 || p_message_index >= chat_history.size()) {
		print_line("AI Chat: Invalid message index for editing: " + String::num(p_message_index));
		return;
	}
	
	ChatMessage &message = chat_history.write[p_message_index];
	if (message.role != "user") {
		print_line("AI Chat: Can only edit user messages, but got role: " + message.role);
		return;
	}
	
	print_line("AI Chat: Editing message at index " + String::num(p_message_index) + ": " + message.content);
	
	// Find the corresponding UI element in chat_container
	// Each message creates 2 children: spacer + message_panel
	// So message at index i should be at UI child index (i * 2 + 1) if there are spacers
	// But since the first message doesn't have a spacer, it's more complex
	// Let's just rebuild the UI to be safe
	
	// Clear chat UI and rebuild in edit mode
	for (int i = 0; i < chat_container->get_child_count(); i++) {
		Node *child = chat_container->get_child(i);
		child->queue_free();
	}
	
	// Rebuild conversation UI but with edit mode for the selected message
	for (int i = 0; i < chat_history.size(); i++) {
		if (i == p_message_index) {
			_create_edit_message_bubble(chat_history[i], i);
		} else {
			_create_message_bubble(chat_history[i], i);
		}
	}
	
	call_deferred("_scroll_to_bottom");
}

void AIChatDock::_create_edit_message_bubble(const AIChatDock::ChatMessage &p_message, int p_message_index) {
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
	message_panel->set_visible(true);

	// User message styling (editing mode)
	Ref<StyleBoxFlat> panel_style = memnew(StyleBoxFlat);
	panel_style->set_content_margin_all(12);
	panel_style->set_corner_radius_all(8);
	panel_style->set_bg_color(get_theme_color(SNAME("accent_color"), SNAME("Editor")) * Color(1, 1, 1, 0.15)); // Brighter to indicate edit mode
	panel_style->set_border_width_all(2);
	panel_style->set_border_color(get_theme_color(SNAME("accent_color"), SNAME("Editor"))); // Accent border for edit mode
	message_panel->add_theme_style_override("panel", panel_style);

	// Content
	VBoxContainer *message_vbox = memnew(VBoxContainer);
	message_panel->add_child(message_vbox);

	// Role label
	Label *role_label = memnew(Label);
	role_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
	role_label->set_text("User (Editing)");
	role_label->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	message_vbox->add_child(role_label);

	// Editable text field
	TextEdit *edit_field = memnew(TextEdit);
	edit_field->set_text(p_message.content);
	edit_field->set_custom_minimum_size(Size2(0, 100));
	edit_field->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	// TextEdit wrapping might have different method name in Godot 4
	message_vbox->add_child(edit_field);

	// Buttons
	HBoxContainer *button_container = memnew(HBoxContainer);
	message_vbox->add_child(button_container);

	Button *send_button = memnew(Button);
	send_button->set_text("Send");
	send_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Play"), SNAME("EditorIcons")));
	// Store the edit field reference in the button's metadata so we can get the text when pressed
	send_button->set_meta("edit_field", edit_field);
	send_button->set_meta("message_index", p_message_index);
	send_button->connect("pressed", callable_mp(this, &AIChatDock::_on_edit_send_button_pressed).bind(send_button));
	button_container->add_child(send_button);
	
	// Connect keyboard input for Enter key support
	edit_field->connect("gui_input", callable_mp(this, &AIChatDock::_on_edit_field_gui_input).bind(send_button));

	Button *cancel_button = memnew(Button);
	cancel_button->set_text("Cancel");
	cancel_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Stop"), SNAME("EditorIcons")));
	cancel_button->connect("pressed", callable_mp(this, &AIChatDock::_on_edit_message_cancel_pressed).bind(p_message_index));
	button_container->add_child(cancel_button);
	
	// Focus the edit field
	edit_field->grab_focus();
}

void AIChatDock::_on_edit_send_button_pressed(Button *p_button) {
	// Get the edit field and message index from the button's metadata
	TextEdit *edit_field = Object::cast_to<TextEdit>(p_button->get_meta("edit_field"));
	int message_index = p_button->get_meta("message_index");
	
	if (!edit_field) {
		print_line("AI Chat: Error - could not find edit field from button metadata");
		return;
	}
	
	String new_content = edit_field->get_text();
	_on_edit_message_send_pressed(message_index, new_content);
}

void AIChatDock::_on_edit_message_send_pressed(int p_message_index, const String &p_new_content) {
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	if (p_message_index < 0 || p_message_index >= chat_history.size()) {
		print_line("AI Chat: Invalid message index for sending edit: " + String::num(p_message_index));
		return;
	}
	
	String trimmed_content = p_new_content.strip_edges();
	if (trimmed_content.is_empty()) {
		print_line("AI Chat: Cannot send empty message");
		return;
	}
	
	print_line("AI Chat: Sending edited message at index " + String::num(p_message_index) + ": " + trimmed_content);
	
	// Update the message content
	chat_history.write[p_message_index].content = trimmed_content;
	chat_history.write[p_message_index].timestamp = _get_timestamp();
	
	// Truncate conversation at this point (remove everything after this message)
	chat_history.resize(p_message_index + 1);
	
	// Update conversation timestamp
	if (current_conversation_index >= 0) {
		conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
	}
	
	// Rebuild UI with the updated conversation
	for (int i = 0; i < chat_container->get_child_count(); i++) {
		Node *child = chat_container->get_child(i);
		child->queue_free();
	}
	_rebuild_conversation_ui(chat_history);
	
	// Reset the assistant message label so streaming can create a new one
	current_assistant_message_label = nullptr;
	
	// Queue a delayed save instead of immediate save
	_queue_delayed_save();
	
	// Send the request as if it's a new message
	is_waiting_for_response = true;
	_update_ui_state();
	call_deferred("_process_send_request_async");
}

void AIChatDock::_on_edit_field_gui_input(const Ref<InputEvent> &p_event, Button *p_send_button) {
	// Handle Enter key to send the edited message
	Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid() && key_event->is_pressed()) {
		if (key_event->get_keycode() == Key::ENTER || key_event->get_keycode() == Key::KP_ENTER) {
			// Check if Shift is not pressed (Shift+Enter should add new line)
			if (!key_event->is_shift_pressed()) {
				print_line("AI Chat: Enter key pressed in edit field, triggering send");
				// Trigger the send button
				p_send_button->emit_signal("pressed");
				// Accept the event to prevent further processing
				get_viewport()->set_input_as_handled();
			}
		} else if (key_event->get_keycode() == Key::ESCAPE) {
			// Escape key cancels editing
			print_line("AI Chat: Escape key pressed in edit field, cancelling edit");
			// Find the message index from the send button's metadata
			int message_index = p_send_button->get_meta("message_index");
			_on_edit_message_cancel_pressed(message_index);
			get_viewport()->set_input_as_handled();
		}
	}
}

void AIChatDock::_on_edit_message_cancel_pressed(int p_message_index) {
	print_line("AI Chat: Cancelled editing message at index " + String::num(p_message_index));
	
	// Simply rebuild the UI without changes
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	
	for (int i = 0; i < chat_container->get_child_count(); i++) {
		Node *child = chat_container->get_child(i);
		child->queue_free();
	}
	_rebuild_conversation_ui(chat_history);
	
	call_deferred("_scroll_to_bottom");
}

// Embedding System Implementation

void AIChatDock::_initialize_embedding_system() {
	if (embedding_system_initialized) {
		return;
	}
	
	// Require user authentication for embedding system
	if (!_is_user_authenticated()) {
		print_line("AI Chat: Cannot initialize embedding system - user not authenticated");
		return;
	}
	
	print_line("AI Chat: Initializing embedding system for user: " + current_user_name);
	
	// Reset indexing flag to ensure fresh indexing for this user/session
	initial_indexing_done = false;
	
	// Create HTTP request for embedding API calls
	if (!embedding_request) {
		embedding_request = memnew(HTTPRequest);
		add_child(embedding_request);
		embedding_request->connect("request_completed", callable_mp(this, &AIChatDock::_on_embedding_request_completed));
	}
	
	// Connect to file system signals
	EditorFileSystem *filesystem = EditorFileSystem::get_singleton();
	if (filesystem) {
		if (!filesystem->is_connected("filesystem_changed", callable_mp(this, &AIChatDock::_on_filesystem_changed))) {
			filesystem->connect("filesystem_changed", callable_mp(this, &AIChatDock::_on_filesystem_changed));
		}
		if (!filesystem->is_connected("sources_changed", callable_mp(this, &AIChatDock::_on_sources_changed))) {
			filesystem->connect("sources_changed", callable_mp(this, &AIChatDock::_on_sources_changed));
		}
	}
	
	embedding_system_initialized = true;
	
	// Start initial project indexing (always happens since we reset the flag above)
	call_deferred("_perform_initial_indexing");
}

void AIChatDock::_perform_initial_indexing() {
	print_line("AI Chat: 🔄 Starting comprehensive project indexing for enhanced AI assistance...");
	print_line("AI Chat: This may take a moment depending on project size");
	
	Dictionary data;
	data["action"] = "index_project";
	data["project_root"] = _get_project_root_path();
	data["force_reindex"] = true;  // Force complete reindex on login for fresh start
	
	_send_embedding_request("index_project", data);
}

void AIChatDock::_on_filesystem_changed() {
	if (!embedding_system_initialized) {
		return;
	}
	
	print_line("AI Chat: Filesystem changed, updating embeddings");
	
	// Get project summary to see what changed
	Dictionary data;
	data["action"] = "status";
	data["project_root"] = _get_project_root_path();
	
	_send_embedding_request("status", data);
}

void AIChatDock::_on_sources_changed(bool p_exist) {
	if (!embedding_system_initialized || !p_exist) {
		return;
	}
	
	print_line("AI Chat: Sources changed, performing incremental update");
	
	// For now, do a light reindex
	Dictionary data;
	data["action"] = "index_project";
	data["project_root"] = _get_project_root_path();
	data["force_reindex"] = false;
	
	_send_embedding_request("index_project", data);
}

void AIChatDock::_update_file_embedding(const String &p_file_path) {
	if (!embedding_system_initialized || !_should_index_file(p_file_path)) {
		return;
	}
	
	print_line("AI Chat: Updating embedding for file: " + p_file_path);
	
	Dictionary data;
	data["action"] = "update_file";
	data["project_root"] = _get_project_root_path();
	data["file_path"] = p_file_path;
	
	_send_embedding_request("update_file", data);
}

void AIChatDock::_remove_file_embedding(const String &p_file_path) {
	if (!embedding_system_initialized) {
		return;
	}
	
	print_line("AI Chat: Removing embedding for file: " + p_file_path);
	
	Dictionary data;
	data["action"] = "remove_file";
	data["project_root"] = _get_project_root_path();
	data["file_path"] = p_file_path;
	
	_send_embedding_request("remove_file", data);
}

void AIChatDock::_send_embedding_request(const String &p_action, const Dictionary &p_data) {
	if (!embedding_request) {
		print_line("AI Chat: Embedding request not initialized");
		return;
	}
	
	if (!_is_user_authenticated()) {
		print_line("AI Chat: Cannot send embedding request - user not authenticated");
		return;
	}
	
	// Prepare request with user authentication
	String base_url = api_endpoint.replace("/chat", "/embed");
	
	// Add user authentication to request data
	Dictionary auth_data = p_data.duplicate();
	auth_data["user_id"] = current_user_id;
	auth_data["machine_id"] = OS::get_singleton()->get_unique_id();
	
	String json_data = JSON::stringify(auth_data);
	
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	if (!auth_token.is_empty()) {
		headers.push_back("Authorization: Bearer " + auth_token);
	}
	if (!api_key.is_empty()) {
		headers.push_back("X-API-Key: " + api_key);
	}
	
	Error err = embedding_request->request(base_url, headers, HTTPClient::METHOD_POST, json_data);
	if (err != OK) {
		print_line("AI Chat: Failed to send embedding request: " + String::num_int64(err));
	}
}

void AIChatDock::_on_embedding_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	
	if (p_code != 200) {
		print_line("AI Chat: Embedding request failed with code " + String::num_int64(p_code) + ": " + response_text);
		return;
	}
	
	JSON json;
	Error parse_err = json.parse(response_text);
	if (parse_err != OK) {
		print_line("AI Chat: Failed to parse embedding response: " + response_text);
		return;
	}
	
	Dictionary response = json.get_data();
	bool success = response.get("success", false);
	String action = response.get("action", "");
	
	if (!success) {
		print_line("AI Chat: Embedding operation failed: " + String(response.get("error", "Unknown error")));
		return;
	}
	
	print_line("AI Chat: Embedding operation '" + action + "' completed successfully");
	
	if (action == "index_project") {
		Dictionary stats = response.get("stats", Dictionary());
		int indexed = stats.get("indexed", 0);
		int skipped = stats.get("skipped", 0);
		int errors = stats.get("errors", 0);
		
		print_line("AI Chat: Project indexing completed - Indexed: " + String::num_int64(indexed) + 
				   ", Skipped: " + String::num_int64(skipped) + 
				   (errors > 0 ? ", Errors: " + String::num_int64(errors) : ""));
		
		if (!initial_indexing_done) {
			initial_indexing_done = true;
			print_line("AI Chat: ✅ Project is now fully indexed and ready for AI assistance!");
		} else {
			print_line("AI Chat: ✅ Project index updated successfully");
		}
	} else if (action == "search") {
		// Handle multimodal search results for relevant file suggestions
		Array results = response.get("results", Array());
		if (results.size() > 0) {
			print_line("AI Chat: Found " + String::num_int64(results.size()) + " relevant files");
			
			// Auto-attach highly relevant files (similarity > 0.7)
			for (int i = 0; i < results.size(); i++) {
				Dictionary result = results[i];
				String file_path = result.get("file_path", "");
				float similarity = result.get("similarity", 0.0f);
				String modality = result.get("modality", "text");
				
				if (similarity > 0.7 && !file_path.is_empty()) {
					// Check if file is not already attached
					bool already_attached = false;
					for (const AttachedFile &existing_file : current_attached_files) {
						if (existing_file.path == file_path) {
							already_attached = true;
							break;
						}
					}
					
					if (!already_attached) {
						print_line("AI Chat: Auto-attaching relevant " + modality + " file: " + file_path + " (similarity: " + String::num(similarity) + ")");
						Vector<String> files_to_attach;
						files_to_attach.push_back(file_path);
						_on_files_selected(files_to_attach);
					}
				}
			}
		}
	} else if (action == "status") {
		// Handle status response from filesystem changes
		Dictionary stats = response.get("stats", Dictionary());
		int total_files = stats.get("total_files", 0);
		int indexed_files = stats.get("indexed_files", 0);
		
		print_line("AI Chat: Status check - " + String::num_int64(indexed_files) + "/" + String::num_int64(total_files) + " files indexed");
		
		// Check if we need to re-index (files may have changed)
		// For now, always do an incremental re-index on filesystem changes
		Dictionary reindex_data;
		reindex_data["action"] = "index_project";
		reindex_data["project_root"] = _get_project_root_path();
		reindex_data["force_reindex"] = false; // Incremental update
		
		print_line("AI Chat: Triggering incremental re-index due to filesystem changes");
		_send_embedding_request("index_project", reindex_data);
	}
}

bool AIChatDock::_should_index_file(const String &p_file_path) {
	// Check file extension for multimodal support
	String ext = p_file_path.get_extension().to_lower();
	static const HashSet<String> indexable_extensions = {
		// Text files
		"gd", "cs", "js",                    // Scripts
		"tscn", "scn",                      // Scenes
		"tres", "res",                      // Resources
		"json", "cfg",                      // Config files
		"md", "txt",                        // Documentation
		"glsl", "shader",                   // Shaders
		
		// Image files
		"png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "tga", "exr",
		
		// Audio files
		"wav", "ogg", "mp3", "aac", "flac",
		
		// 3D models
		"obj", "fbx", "gltf", "glb"
	};
	
	if (!indexable_extensions.has(ext)) {
		return false;
	}
	
	// Skip hidden files and temp directories
	if (p_file_path.find("/.") != -1 || 
		p_file_path.find("/.godot/") != -1 || 
		p_file_path.find("/build/") != -1 || 
		p_file_path.find("/temp/") != -1) {
		return false;
	}
	
	return true;
}

String AIChatDock::_get_project_root_path() {
	return ProjectSettings::get_singleton()->get_resource_path();
}

void AIChatDock::_suggest_relevant_files(const String &p_query) {
	if (!embedding_system_initialized || p_query.is_empty()) {
		return;
	}
	
	print_line("AI Chat: Searching for files relevant to: " + p_query);
	
	Dictionary data;
	data["action"] = "search";
	data["project_root"] = _get_project_root_path();
	data["query"] = p_query;
	data["k"] = 5; // Get top 5 results
	
	_send_embedding_request("search", data);
}

void AIChatDock::_auto_attach_relevant_context() {
	// Get the current message text
	String message_text = input_field->get_text().strip_edges();
	if (message_text.is_empty()) {
		return;
	}
	
	// Auto-suggest relevant files if the message is substantial
	if (message_text.length() > 20) {
		_suggest_relevant_files(message_text);
	}
}

// Authentication Implementation

void AIChatDock::_setup_authentication_ui() {
	// User authentication row
	HBoxContainer *auth_container = memnew(HBoxContainer);
	add_child(auth_container);
	
	Label *auth_label = memnew(Label);
	auth_label->set_text("User:");
	auth_container->add_child(auth_label);
	
	user_status_label = memnew(Label);
	user_status_label->set_text("Not logged in");
	user_status_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	auth_container->add_child(user_status_label);
	
	login_button = memnew(Button);
	login_button->set_text("Login");
	login_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Key"), SNAME("EditorIcons")));
	login_button->connect("pressed", callable_mp(this, &AIChatDock::_on_login_button_pressed));
	auth_container->add_child(login_button);
	
	// Create HTTP request for authentication
	auth_request = memnew(HTTPRequest);
	add_child(auth_request);
	auth_request->connect("request_completed", callable_mp(this, &AIChatDock::_on_auth_request_completed));
}

void AIChatDock::_on_login_button_pressed() {
	if (_is_user_authenticated()) {
		// User is logged in, offer logout
		_logout_user();
		return;
	}
	
	// Open authentication in system browser
	String auth_url = api_endpoint.replace("/chat", "/auth/login");
	auth_url += "?machine_id=" + OS::get_singleton()->get_unique_id() + "&provider=google";
	OS::get_singleton()->shell_open(auth_url);
	
	// Show simple notification (no manual action needed)
	AcceptDialog *auth_dialog = memnew(AcceptDialog);
	auth_dialog->set_text("🔐 Authentication opened in your browser.\n\nComplete the login and this will automatically detect when you're logged in!");
	auth_dialog->set_title("AI Chat - Login");
	auth_dialog->get_ok_button()->set_text("Got it!");
	
	get_viewport()->add_child(auth_dialog);
	auth_dialog->popup_centered();
	auth_dialog->connect("confirmed", callable_mp((Node *)auth_dialog, &Node::queue_free));
	
	// Start automatic polling for login completion
	user_status_label->set_text("Waiting for login...");
	_start_login_polling();
}

void AIChatDock::_on_auth_dialog_action(const StringName &p_action) {
	// This method is kept for compatibility but no longer used
	// Login polling handles authentication automatically
}

void AIChatDock::_check_authentication_status() {
	// Check with backend if user is authenticated
	String auth_check_url = api_endpoint.replace("/chat", "/auth/status");
	
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	
	Dictionary data;
	data["machine_id"] = OS::get_singleton()->get_unique_id();
	String json_data = JSON::stringify(data);
	
	Error err = auth_request->request(auth_check_url, headers, HTTPClient::METHOD_POST, json_data);
	if (err != OK) {
		print_line("AI Chat: Failed to check authentication status: " + String::num_int64(err));
	}
}

void AIChatDock::_on_auth_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	
	print_line("AI Chat: 📡 Auth request completed - Result: " + String::num_int64(p_result) + ", Code: " + String::num_int64(p_code));
	
	if (p_code != 200) {
		print_line("AI Chat: ❌ Authentication request failed with code " + String::num_int64(p_code) + ": " + response_text);
		// If it's a connection error, the backend might not be running
		if (p_code == 0) {
			print_line("AI Chat: 🔌 Backend server might not be running. Please start the backend server.");
		}
		return;
	}
	
	JSON json;
	Error parse_err = json.parse(response_text);
	if (parse_err != OK) {
		print_line("AI Chat: Failed to parse authentication response: " + response_text);
		return;
	}
	
	Dictionary response = json.get_data();
	bool success = response.get("success", false);
	
	if (success && response.has("user")) {
		Dictionary user_data = response.get("user", Dictionary());
		current_user_id = user_data.get("id", "");
		current_user_name = user_data.get("name", "");
		auth_token = response.get("token", "");
		
		// Save authentication
		EditorSettings::get_singleton()->set_setting("ai_chat/auth_token", auth_token);
		EditorSettings::get_singleton()->set_setting("ai_chat/user_id", current_user_id);
		EditorSettings::get_singleton()->set_setting("ai_chat/user_name", current_user_name);
		
		print_line("AI Chat: ✅ User authenticated successfully: " + current_user_name);
		
		// Stop login polling if it was running
		_stop_login_polling();
		
		// Force UI update
		_update_user_status();
		
		// Force comprehensive project indexing
		_ensure_project_indexing();
	} else {
		// Authentication failed - clear any invalid saved credentials
		current_user_id = "";
		current_user_name = "";
		auth_token = "";
		
		// Remove invalid credentials from settings
		if (EditorSettings::get_singleton()->has_setting("ai_chat/auth_token")) {
			EditorSettings::get_singleton()->erase("ai_chat/auth_token");
			EditorSettings::get_singleton()->erase("ai_chat/user_id");
			EditorSettings::get_singleton()->erase("ai_chat/user_name");
			print_line("AI Chat: Cleared invalid saved credentials");
		}
		
		_update_user_status();
		print_line("AI Chat: Authentication failed: " + String(response.get("error", "Unknown error")));
	}
}

void AIChatDock::_update_user_status() {
	if (_is_user_authenticated()) {
		user_status_label->set_text(current_user_name);
		login_button->set_text("Logout");
		login_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Unlock"), SNAME("EditorIcons")));
	} else {
		user_status_label->set_text("Not logged in");
		login_button->set_text("Login");
		login_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Key"), SNAME("EditorIcons")));
	}
}

void AIChatDock::_logout_user() {
	// Clear authentication data
	current_user_id = "";
	current_user_name = "";
	auth_token = "";
	
	// Reset embedding system state
	embedding_system_initialized = false;
	initial_indexing_done = false;
	
	// Remove from settings
	EditorSettings::get_singleton()->erase("ai_chat/auth_token");
	EditorSettings::get_singleton()->erase("ai_chat/user_id");
	EditorSettings::get_singleton()->erase("ai_chat/user_name");
	
	_update_user_status();
	print_line("AI Chat: User logged out - embedding system reset");
}

bool AIChatDock::_is_user_authenticated() const {
	return !current_user_id.is_empty() && !auth_token.is_empty();
}

void AIChatDock::_auto_verify_saved_credentials() {
	print_line("AI Chat: 🔍 Checking for saved authentication credentials...");
	
	// Check if we have saved authentication credentials
	if (EditorSettings::get_singleton()->has_setting("ai_chat/auth_token")) {
		String saved_token = EditorSettings::get_singleton()->get_setting("ai_chat/auth_token");
		String saved_user_id = EditorSettings::get_singleton()->get_setting("ai_chat/user_id");
		String saved_user_name = EditorSettings::get_singleton()->get_setting("ai_chat/user_name");
		
		if (!saved_token.is_empty() && !saved_user_id.is_empty()) {
			// Set credentials for verification
			auth_token = saved_token;
			current_user_id = saved_user_id;
			current_user_name = saved_user_name;
			
			print_line("AI Chat: 🔐 Auto-verifying saved authentication for user: " + saved_user_name);
			
			// Show temporary status while verifying
			user_status_label->set_text("Verifying login...");
			
			// Verify with backend
			_check_authentication_status();
		} else {
			print_line("AI Chat: ❌ Saved credentials found but incomplete");
		}
	} else {
		print_line("AI Chat: ℹ️ No saved authentication credentials found");
	}
}

void AIChatDock::_start_login_polling() {
	login_poll_attempts = 0;
	login_poll_max_attempts = 30; // Poll for 30 seconds (every 1 second)
	
	// Create timer if it doesn't exist
	if (!login_poll_timer) {
		login_poll_timer = memnew(Timer);
		add_child(login_poll_timer);
		login_poll_timer->connect("timeout", callable_mp(this, &AIChatDock::_poll_login_status));
	}
	
	// Start polling every 1 second
	login_poll_timer->set_wait_time(1.0);
	login_poll_timer->set_one_shot(false);
	login_poll_timer->start();
	
	print_line("AI Chat: 🔄 Started automatic login polling");
}

void AIChatDock::_poll_login_status() {
	login_poll_attempts++;
	
	if (login_poll_attempts > login_poll_max_attempts) {
		// Stop polling after max attempts
		login_poll_timer->stop();
		user_status_label->set_text("Login timeout - try again");
		print_line("AI Chat: ⏰ Login polling timed out");
		return;
	}
	
	// Check authentication status
	_check_authentication_status();
}

void AIChatDock::_stop_login_polling() {
	if (login_poll_timer && login_poll_timer->is_connected("timeout", callable_mp(this, &AIChatDock::_poll_login_status))) {
		login_poll_timer->stop();
		print_line("AI Chat: ✅ Stopped login polling");
	}
}

void AIChatDock::_ensure_project_indexing() {
	print_line("AI Chat: 🔄 Ensuring project indexing starts...");
	
	// Make sure user is authenticated
	if (!_is_user_authenticated()) {
		print_line("AI Chat: ❌ Cannot start indexing - user not authenticated");
		return;
	}
	
	// Initialize embedding system if needed
	if (!embedding_system_initialized) {
		print_line("AI Chat: 📝 Initializing embedding system...");
		_initialize_embedding_system();
	} else {
		print_line("AI Chat: 📝 Embedding system already initialized, forcing indexing...");
		// Reset the flag to ensure fresh indexing
		initial_indexing_done = false;
		// Start indexing immediately
		call_deferred("_perform_initial_indexing");
	}
}

void AIChatDock::_process_send_request_async() {
	// This runs in the next frame, keeping UI responsive
	
	// Update conversation timestamp and title (heavier operations)
	if (current_conversation_index >= 0) {
		conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
		// Update title if it's still "New Conversation"
		if (conversations[current_conversation_index].title == "New Conversation") {
			conversations.write[current_conversation_index].title = _generate_conversation_title(_get_current_chat_history());
		}
		_update_conversation_dropdown();
	}
	
	// Now send the actual request (this is the heavy operation)
	_send_chat_request();
}

void AIChatDock::_save_conversations_async() {
	// Start the async save to prevent UI blocking during file operations
	call_deferred("_save_conversations_chunked", 0);
}

void AIChatDock::_save_conversations_to_disk(const String &p_json_data) {
	// This runs in the next frame, preventing blocking of current frame
	Error err;
	Ref<FileAccess> file = FileAccess::open(conversations_file_path, FileAccess::WRITE, &err);
	if (err != OK) {
		print_line("AI Chat: Failed to save conversations file: " + String::num_int64(err));
		return;
	}
	
	file->store_string(p_json_data);
	file->close();
	// print_line("AI Chat: Conversations saved successfully");
}

void AIChatDock::_save_conversations_chunked(int p_start_index) {
	// Process conversations in chunks of 5 to avoid blocking UI
	const int CHUNK_SIZE = 5;
	int end_index = MIN(p_start_index + CHUNK_SIZE, conversations.size());
	
	// If this is the first chunk, initialize the array
	if (p_start_index == 0) {
		_chunked_conversations_array.clear();
	}
	
	// Process this chunk
	for (int i = p_start_index; i < end_index; i++) {
		const AIChatDock::Conversation &conv = conversations[i];
		Dictionary conv_dict;
		conv_dict["id"] = conv.id;
		conv_dict["title"] = conv.title;
		conv_dict["created_timestamp"] = conv.created_timestamp;
		conv_dict["last_modified_timestamp"] = conv.last_modified_timestamp;
		
		Array messages_array;
		for (int j = 0; j < conv.messages.size(); j++) {
			messages_array.push_back(_build_api_message(conv.messages[j]));
		}
		conv_dict["messages"] = messages_array;
		_chunked_conversations_array.push_back(conv_dict);
	}
	
	print_line("AI Chat: Processed conversation save chunk " + String::num_int64(p_start_index) + "-" + String::num_int64(end_index-1) + " of " + String::num_int64(conversations.size()));
	
	// If more chunks to process, defer the next chunk
	if (end_index < conversations.size()) {
		call_deferred("_save_conversations_chunked", end_index);
		return;
	}
	
	// All chunks processed, now finalize the save
	call_deferred("_finalize_conversations_save");
}

void AIChatDock::_finalize_conversations_save() {
	Dictionary data;
	data["conversations"] = _chunked_conversations_array;
	
	Ref<JSON> json;
	json.instantiate();
	String json_string = json->stringify(data, "  ");
	
	_save_conversations_to_disk(json_string);
	
	// Clear the chunked array to free memory
	_chunked_conversations_array.clear();
}

void AIChatDock::_queue_delayed_save() {
	// If a save is already scheduled, do nothing
	if (save_pending) {
		return;
	}
	
	save_pending = true;
	// Start (or restart) the save timer for 3 seconds
	if (save_timer) {
		save_timer->stop();
		save_timer->start(3.0); // wait 3 seconds of idle time before saving
	}
}

void AIChatDock::_execute_delayed_save() {
	if (!save_pending) {
		return; // No save pending
	}
	
	if (save_thread_busy) {
		return; // A save is already running
	}
	
	save_pending = false;
	save_thread_busy = true;
	
	// Create a data structure to pass both the snapshot and the instance
	struct SaveData {
		Vector<Conversation> *snapshot;
		AIChatDock *instance;
		String file_path;
	};
	
	SaveData *save_data = memnew(SaveData);
	save_data->snapshot = memnew(Vector<Conversation>(conversations));
	save_data->instance = this;
	save_data->file_path = conversations_file_path;
	
	save_thread = memnew(Thread);
	save_thread->start(_background_save, save_data);
	
	print_line("AI Chat: Started background conversation save");
}

void AIChatDock::_background_save(void *p_data_ptr) {
	struct SaveData {
		Vector<Conversation> *snapshot;
		AIChatDock *instance;
		String file_path;
	};
	
	SaveData *save_data = reinterpret_cast<SaveData*>(p_data_ptr);
	Vector<Conversation> &snapshot = *save_data->snapshot;
	
	// Build the data structure (heavy JSON stringify)
	Dictionary data;
	Array conv_array;
	
	for (const Conversation &conv : snapshot) {
		Dictionary conv_dict;
		conv_dict["id"] = conv.id;
		conv_dict["title"] = conv.title;
		conv_dict["created_timestamp"] = conv.created_timestamp;
		conv_dict["last_modified_timestamp"] = conv.last_modified_timestamp;
		
		Array messages_array;
		for (const ChatMessage &msg : conv.messages) {
			Dictionary msg_dict;
			msg_dict["role"] = msg.role;
			msg_dict["content"] = msg.content;
			msg_dict["timestamp"] = msg.timestamp;
			msg_dict["tool_calls"] = msg.tool_calls;
			msg_dict["tool_call_id"] = msg.tool_call_id;
			msg_dict["name"] = msg.name;
			msg_dict["tool_results"] = msg.tool_results;
			
			// Handle attached files
			Array files_array;
			for (const AttachedFile &file : msg.attached_files) {
				Dictionary file_dict;
				file_dict["path"] = file.path;
				file_dict["name"] = file.name;
				file_dict["content"] = file.content;
				file_dict["is_image"] = file.is_image;
				file_dict["mime_type"] = file.mime_type;
				file_dict["base64_data"] = file.base64_data;
				Array original_size_array;
				original_size_array.push_back(file.original_size.x);
				original_size_array.push_back(file.original_size.y);
				file_dict["original_size"] = original_size_array;
				
				Array display_size_array;
				display_size_array.push_back(file.display_size.x);
				display_size_array.push_back(file.display_size.y);
				file_dict["display_size"] = display_size_array;
				file_dict["was_downsampled"] = file.was_downsampled;
				file_dict["is_node"] = file.is_node;
				file_dict["node_path"] = file.node_path;
				file_dict["node_type"] = file.node_type;
				files_array.push_back(file_dict);
			}
			msg_dict["attached_files"] = files_array;
			
			messages_array.push_back(msg_dict);
		}
		conv_dict["messages"] = messages_array;
		conv_array.push_back(conv_dict);
	}
	data["conversations"] = conv_array;
	
	// JSON stringify (heavy operation)
	Ref<JSON> json;
	json.instantiate();
	String json_string = json->stringify(data, "  ");
	
	// File I/O (heavy operation)
	Error err;
	Ref<FileAccess> file = FileAccess::open(save_data->file_path, FileAccess::WRITE, &err);
	if (err == OK) {
		file->store_string(json_string);
		file->close();
	}
	
	// Signal back to main thread
	AIChatDock *instance = save_data->instance;
	
	// Clean up
	memdelete(save_data->snapshot);
	memdelete(save_data);
	
	// Signal completion to main thread
	if (instance) {
		instance->call_deferred("_on_background_save_finished");
	}
}

void AIChatDock::_on_background_save_finished() {
	if (save_thread) {
		save_thread->wait_to_finish();
		memdelete(save_thread);
		save_thread = nullptr;
	}
	save_thread_busy = false;
	print_line("AI Chat: Background save finished");
}

void AIChatDock::_process_image_attachment_async(const String &p_file_path, const String &p_name, const String &p_mime_type) {
	// This runs in the next frame to prevent UI blocking during image processing
	Ref<Image> image = Image::load_from_file(p_file_path);
	if (image.is_null() || image->is_empty()) {
		print_line("AI Chat: Failed to load image: " + p_file_path);
		return;
	}

	AIChatDock::AttachedFile attached_file;
	attached_file.path = p_file_path;
	// Create unique image ID: timestamp + clean filename
	String clean_name = p_name.get_basename(); // Remove extension
	clean_name = clean_name.to_lower().replace(" ", "_");
	attached_file.name = "img_" + String::num_int64(OS::get_singleton()->get_ticks_msec()) + "_" + clean_name;
	attached_file.is_image = true;
	attached_file.mime_type = p_mime_type;

	Vector2i original_size = Vector2i(image->get_width(), image->get_height());
	attached_file.original_size = original_size;
	
	// Check if image needs to be downsampled (max 1024px on any side)
	const int MAX_DIMENSION = 1024;
	Vector2i target_size = _calculate_downsampled_size(original_size, MAX_DIMENSION);
	
	if (target_size != original_size) {
		attached_file.was_downsampled = true;
		image->resize(target_size.x, target_size.y, Image::INTERPOLATE_LANCZOS);
		
		// Show warning dialog - defer this to ensure UI is responsive
		call_deferred("_show_image_warning_dialog", attached_file.name, original_size, target_size);
	}
	
	attached_file.display_size = target_size;
	
	// Convert to base64 for API transmission
	Vector<uint8_t> buffer;
	if (attached_file.mime_type == "image/jpeg" || attached_file.mime_type == "image/jpg") {
		buffer = image->save_jpg_to_buffer(0.85f); // Good quality JPEG
	} else {
		buffer = image->save_png_to_buffer();
	}
	
	if (buffer.size() == 0) {
		print_line("AI Chat: Failed to encode image: " + p_file_path);
		return;
	}
	
	// Encode to base64
	attached_file.base64_data = CoreBind::Marshalls::get_singleton()->raw_to_base64(buffer);
	
	// Add to attached files and update display
	current_attached_files.push_back(attached_file);
	_update_attached_files_display();
	
	print_line("AI Chat: Successfully processed image: " + p_name + " -> ID: " + attached_file.name);
}

void AIChatDock::_on_input_text_changed() {
	send_button->set_disabled(input_field->get_text().strip_edges().is_empty() || is_waiting_for_response);
}

// --- At-Mention Implementation ---
// This is actually not working rn! :/ 

void AIChatDock::_update_at_mention_popup() {
	String text = input_field->get_text();
	int cursor_pos = input_field->get_caret_column();
	
	int at_pos = text.rfind("@", cursor_pos);
	if (at_pos == -1) {
		at_mention_popup->hide();
		return;
	}
	
	String query = text.substr(at_pos + 1, cursor_pos - at_pos - 1);
	
	// Check for spaces after '@' which would invalidate the mention
	if (query.find(" ") != -1) {
		at_mention_popup->hide();
		return;
	}
	
	// Populate and show the popup
	_populate_at_mention_tree(query);
	
	Point2i popup_pos = input_field->get_screen_position() + Point2i(0, -at_mention_popup->get_size().y);
	at_mention_popup->set_position(popup_pos);
	at_mention_popup->popup();
}

void AIChatDock::_populate_at_mention_tree(const String &p_filter) {
	at_mention_tree->clear();
	TreeItem *root = at_mention_tree->create_item();
	at_mention_tree->set_hide_root(true);
	
	// Use EditorFileSystem to get project files
	EditorFileSystem *fs = EditorFileSystem::get_singleton();
	
	// Recursive function to populate the tree
	_populate_tree_recursive(fs->get_filesystem(), root, p_filter);
}

void AIChatDock::_populate_tree_recursive(EditorFileSystemDirectory *p_dir, TreeItem *p_parent, const String &p_filter) {
	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		TreeItem *dir_item = at_mention_tree->create_item(p_parent);
		dir_item->set_text(0, p_dir->get_subdir(i)->get_name());
		dir_item->set_icon(0, get_theme_icon(SNAME("Folder"), SNAME("EditorIcons")));
		_populate_tree_recursive(p_dir->get_subdir(i), dir_item, p_filter);
	}
	
	for (int i = 0; i < p_dir->get_file_count(); i++) {
		String file_name = p_dir->get_file(i);
		if (p_filter.is_empty() || file_name.findn(p_filter) != -1) {
			// Exclude image files
			String ext = file_name.get_extension().to_lower();
			if (ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "gif" && ext != "bmp" && ext != "webp" && ext != "svg") {
				TreeItem *file_item = at_mention_tree->create_item(p_parent);
				file_item->set_text(0, file_name);
				file_item->set_metadata(0, p_dir->get_file_path(i));
				file_item->set_icon(0, get_theme_icon(SNAME("File"), SNAME("EditorIcons")));
			}
		}
	}
}

void AIChatDock::_on_at_mention_item_selected() {
	TreeItem *selected = at_mention_tree->get_selected();
	if (!selected || selected->get_metadata(0).is_null()) {
		return; // It's a directory
	}
	
	String file_path = selected->get_metadata(0);
	String file_name = selected->get_text(0);
	
	// Add the file to attachments
	Vector<String> files_to_add;
	files_to_add.push_back(file_path);
	_on_files_selected(files_to_add);
	
	// Replace the @mention with the file name
	String text = input_field->get_text();
	int cursor_pos = input_field->get_caret_column();
	int at_pos = text.rfind("@", cursor_pos);
	
	String before = text.substr(0, at_pos);
	String after = text.substr(cursor_pos);
	
	input_field->set_text(before + file_name + " " + after);
	input_field->set_caret_column(at_pos + file_name.length() + 1);
	
	at_mention_popup->hide();
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



void AIChatDock::_on_attachment_menu_item_pressed(int p_id) {
	switch (p_id) {
		case 0: // Files
			_on_attach_files_pressed();
			break;
		case 1: // Scene Nodes
			_on_attach_scene_nodes_pressed();
			break;
		case 2: // Current Script
			_on_attach_current_script_pressed();
			break;
		case 3: // Resources
			_on_attach_resources_pressed();
			break;
	}
}

void AIChatDock::_on_attach_files_pressed() {
	if (file_dialog) {
		file_dialog->popup_file_dialog();
	}
}

void AIChatDock::_on_attach_scene_nodes_pressed() {
	if (scene_tree_popup && scene_tree) {
		// Populate the scene tree with current scene nodes
		scene_tree->clear();
		Node *current_scene = EditorNode::get_singleton()->get_edited_scene();
		if (current_scene) {
			TreeItem *root = scene_tree->create_item();
			root->set_text(0, current_scene->get_name());
			root->set_icon(0, get_theme_icon(SNAME("PackedScene"), SNAME("EditorIcons")));
			root->set_metadata(0, current_scene->get_path());
			_populate_scene_tree_recursive(current_scene, root);
			root->set_collapsed(false);
		} else {
			TreeItem *root = scene_tree->create_item();
			root->set_text(0, "No scene open");
			root->set_selectable(0, false);
		}
		
		// Position and show the popup
		Rect2 button_rect = attach_button->get_global_rect();
		Vector2 popup_pos = Vector2(button_rect.position.x, button_rect.position.y + button_rect.size.y + 5);
		scene_tree_popup->set_position(popup_pos);
		scene_tree_popup->popup();
	}
}

void AIChatDock::_on_attach_current_script_pressed() {
	_attach_current_script();
}

void AIChatDock::_on_attach_resources_pressed() {
	if (resource_dialog) {
		resource_dialog->popup_file_dialog();
	}
}

void AIChatDock::_on_scene_tree_node_selected() {
	if (!scene_tree) {
		return;
	}
	
	// Get all selected items
	TreeItem *selected = scene_tree->get_selected();
	if (selected) {
		NodePath node_path = selected->get_metadata(0);
		Node *current_scene = EditorNode::get_singleton()->get_edited_scene();
		if (current_scene) {
			Node *node = current_scene->get_node_or_null(node_path);
			if (node) {
				_attach_scene_node(node);
			}
		}
	}
	
	// Hide the popup after selection
	scene_tree_popup->hide();
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
			if (_is_image_file(file_path)) {
				// Create unique image ID for images
				String clean_name = file_path.get_file().get_basename().to_lower().replace(" ", "_");
				attached_file.name = "img_" + String::num_int64(OS::get_singleton()->get_ticks_msec()) + "_" + clean_name;
			} else {
				attached_file.name = file_path.get_file();
			}
			attached_file.is_image = _is_image_file(file_path);
			attached_file.mime_type = _get_mime_type_from_extension(file_path);
			
			if (attached_file.is_image) {
				// Process image asynchronously to prevent UI blocking
				call_deferred("_process_image_attachment_async", file_path, attached_file.name, attached_file.mime_type);
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

// this now fixed
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
	
	// Debug: Print drag data to understand what we're receiving
	print_line("AI Chat: Drag data received:");
	print_line("  Type: " + Variant::get_type_name(p_data.get_type()));
	if (drag_data.has("type")) {
		print_line("  Drag type: " + String(drag_data["type"]));
	}
	if (drag_data.has("files")) {
		Array files = drag_data["files"];
		print_line("  Files count: " + String::num_int64(files.size()));
		for (int i = 0; i < files.size() && i < 3; i++) {
			print_line("    File " + String::num_int64(i) + ": " + String(files[i]));
		}
	}
	if (drag_data.has("nodes")) {
		Array nodes = drag_data["nodes"];
		print_line("  Nodes count: " + String::num_int64(nodes.size()));
		for (int i = 0; i < nodes.size() && i < 3; i++) {
			print_line("    Node " + String::num_int64(i) + ": " + String(nodes[i]));
		}
	}
	// Print all keys in drag_data
	Array keys = drag_data.keys();
	print_line("  All keys: " + String(Variant(keys)));
	print_line("---");
	
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
			if (_is_image_file(file_path)) {
				// Create unique image ID for images
				String clean_name = file_path.get_file().get_basename().to_lower().replace(" ", "_");
				attached_file.name = "img_" + String::num_int64(OS::get_singleton()->get_ticks_msec()) + "_" + clean_name;
			} else {
				attached_file.name = file_path.get_file();
			}
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

	if (data.has("status") && (data["status"] == "finished" || data["status"] == "completed")) {
		stream_completed_successfully = true;
		print_line("AI Chat: Server signaled end of stream");
		return;
	}

	// Handle request_id from backend (first message)
	if (data.has("request_id") && data.has("status") && data["status"] == "started") {
		current_request_id = data["request_id"];
		print_line("AI Chat: Received request ID: " + current_request_id);
		// Update UI state to enable the stop button now that we have a request ID
		_update_ui_state();
		return;
	}

	// Handle stop status from backend
	if (data.has("status") && data["status"] == "stopped") {
		stream_completed_successfully = true; // Mark as successful completion to avoid connection error
		String message = data.get("message", "Request stopped");
		print_line("AI Chat: " + message);
		
		// IMPORTANT: Save conversation when request is stopped to preserve partial content
		if (current_conversation_index >= 0) {
			conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
			_queue_delayed_save();
		}
		
		// Reset state silently (no system message shown to user)
		is_waiting_for_response = false;
		stop_requested = false;
		current_request_id = "";
		_update_ui_state();
		return;
	}

	// Handle tool starting messages for immediate feedback
	if (data.has("status") && data["status"] == "tool_starting") {
		String tool_name = data.get("tool_starting", "unknown_tool");
		String tool_id = data.get("tool_id", "");
		
		// Show immediate feedback to user that tool is starting
		print_line("AI Chat: Tool starting - " + tool_name + " (ID: " + tool_id + ")");
		
		// Create an assistant message with tool call placeholder for ALL tools
		// This ensures immediate visual feedback regardless of tool type
		call_deferred("_create_assistant_message_with_tool_placeholder", tool_name, tool_id);
		
		return; // Stop further processing for this line
	}

	if (data.has("status") && data["status"] == "executing_tools") {
		if (data.has("assistant_message")) {
			Dictionary assistant_message = data["assistant_message"];
			// Check if we already created a placeholder for this tool execution
			// by looking for existing tool placeholders in the UI
			bool already_has_placeholder = false;
			Array tool_calls = assistant_message.get("tool_calls", Array());
			
			if (chat_container != nullptr && tool_calls.size() > 0) {
				// Check if we already have placeholders for any of these tool calls
				for (int i = 0; i < tool_calls.size(); i++) {
					Dictionary tool_call = tool_calls[i];
					String tool_call_id = tool_call.get("id", "");
					if (!tool_call_id.is_empty()) {
						PanelContainer *existing_placeholder = Object::cast_to<PanelContainer>(
							chat_container->find_child("tool_placeholder_" + tool_call_id, true, false));
						if (existing_placeholder) {
							already_has_placeholder = true;
							break;
						}
					}
				}
			}
			
			if (!already_has_placeholder) {
				// This status means a new assistant turn is starting with tool calls.
				// We must create a new message for it, not update a previous one.
				_add_message_to_chat("assistant", assistant_message.get("content", ""), assistant_message.get("tool_calls", Array()));
			}

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
		
		// Handle image generation specially (critical functionality)
		if (tool_executed == "image_operation" && tool_result.get("success", false)) {
			String image_data = tool_result.get("image_data", "");
			String prompt = tool_result.get("prompt", "");
			
			if (!image_data.is_empty()) {
				print_line("AI Chat: Displaying generated image for prompt: " + prompt + " (" + String::num(image_data.length()) + " chars base64)");
				_handle_generated_image(image_data, "generated_" + String::num(OS::get_singleton()->get_ticks_msec()));
			}
		} else {
			// Handle other tools using the standard tool result system
			String tool_call_id = data.get("tool_call_id", "");
			if (!tool_call_id.is_empty()) {
				// Create args dictionary for proper UI recreation
				Dictionary args;
				
				// Use the standard tool result system for consistent UX
				_add_tool_response_to_chat(tool_call_id, tool_executed, args, tool_result);
			} else {
				print_line("AI Chat: Warning - tool_completed missing tool_call_id, cannot update placeholder");
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
		
		// Safety check: ensure label is valid
		if (!label) {
			print_line("AI Chat: Warning - invalid label in content_delta handler");
			return;
		}
		
		String delta = data["content_delta"];
		Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
		if (!chat_history.is_empty()) {
			ChatMessage &last_msg = chat_history.write[chat_history.size() - 1];
			if (last_msg.role == "assistant") {
				last_msg.content += delta;
				
				// Update conversation timestamp for streaming content (but don't save yet)
				if (current_conversation_index >= 0) {
					conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
				}
				
				// Safety check: ensure content is valid before processing
				if (!last_msg.content.is_empty()) {
					String bbcode_content = _markdown_to_bbcode(last_msg.content);
					// Additional safety check for the converted content
					if (!bbcode_content.is_empty()) {
						label->set_text(bbcode_content);
					}
				}
				
				// Note: We don't save during streaming to avoid performance issues
				// Saving happens when the message is complete or stopped
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
		
		// Safety check: ensure label is valid
		if (!label) {
			print_line("AI Chat: Warning - invalid label in assistant_message handler");
			return;
		}
		
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
				
				// Update conversation timestamp for final content
				if (current_conversation_index >= 0) {
					conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
				}
				
				if (!last_msg.content.is_empty()) {
					String bbcode_content = _markdown_to_bbcode(last_msg.content);
					if (!bbcode_content.is_empty()) {
						label->set_text(bbcode_content);
					}
				}
				
				// CRITICAL: Save final assistant message content to prevent loss
				_queue_delayed_save();
			}
		} else {
			// This case should ideally not happen if the stream started correctly.
			if (!final_content.is_empty()) {
				String bbcode_content = _markdown_to_bbcode(final_content);
				if (!bbcode_content.is_empty()) {
					label->set_text(bbcode_content);
				}
			}
		}
	}
}

RichTextLabel *AIChatDock::_get_or_create_current_assistant_message_label() {
	// If a label is already assigned, validate it and use it.
	if (current_assistant_message_label != nullptr) {
		// First check if the label itself is still valid
		bool label_is_valid = false;
		if (chat_container != nullptr) {
			// Check if the label is still a child of some node in the chat container
			for (int i = 0; i < chat_container->get_child_count(); i++) {
				Node *child = chat_container->get_child(i);
				if (_is_label_descendant_of_node(current_assistant_message_label, child)) {
					label_is_valid = true;
					break;
				}
			}
		}
		
		if (label_is_valid) {
			// Make sure its parent hierarchy is visible with safe checks
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
		} else {
			// Label is invalid, clear it
			current_assistant_message_label = nullptr;
		}
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

		// Show immediate visual feedback that tool is starting
		print_line("AI Chat: 🔧 Executing tool: " + function_name);
		
		// Update placeholder to show execution status
		_update_tool_placeholder_status(tool_call_id, function_name, "starting");

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
		} else if (function_name == "search_project_files") {
			// search_project_files was renamed to list_project_files
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
		} else if (function_name == "create_script_file") {
			// create_script_file method no longer exists, use apply_edit instead
			result["success"] = false;
			result["message"] = "create_script_file is no longer available, use apply_edit instead";
		} else if (function_name == "delete_file_safe") {
			// delete_file_safe method no longer exists
			result["success"] = false;
			result["message"] = "delete_file_safe is no longer available";
		} else if (function_name == "edit_file_with_diff") {
			// edit_file_with_diff method no longer exists, use apply_edit instead
			result["success"] = false;
			result["message"] = "edit_file_with_diff is no longer available, use apply_edit instead";
		} else if (function_name == "image_operation") {
			// This tool should be handled by the backend, not the frontend
			// If we receive it here, it means something went wrong in the backend filtering
			result["success"] = false;
			result["message"] = "Image generation should be handled by backend, not frontend";
			print_line("AI Chat: Received image_operation tool in frontend - this should be handled by backend");
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
	
	// Send the conversation with tool results back to backend to continue processing
	// This is expected by the backend after frontend tool execution
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
	_create_message_bubble(msg, chat_history.size() - 1);
	// Show tool call placeholders immediately for better UX
	if (p_role == "assistant" && !p_tool_calls.is_empty()) {
		_create_tool_call_bubbles(p_tool_calls);
		// Make sure the message bubble is visible when we have tool calls
		if (chat_container != nullptr && chat_container->get_child_count() > 0) {
			Node *last_child = chat_container->get_child(chat_container->get_child_count() - 1);
			PanelContainer *message_panel = Object::cast_to<PanelContainer>(last_child);
			if (message_panel) {
				message_panel->set_visible(true);
			}
		}
	}

	// Update conversation timestamp and save
	if (current_conversation_index >= 0) {
		conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
		_queue_delayed_save();
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
	
	// Store the original tool arguments for proper UI recreation
	msg.tool_results.clear();
	msg.tool_results.push_back(p_result);
	msg.tool_results.push_back(p_args); // Store args as second element

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

	// Clear the "loading" text immediately.
	while (placeholder->get_child_count() > 0) {
		Node *child = placeholder->get_child(0);
		placeholder->remove_child(child);
		child->queue_free();
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

	// Use the shared tool-specific UI creation function
	_create_tool_specific_ui(content_vbox, p_name, p_result, success, p_args);

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

void AIChatDock::_create_message_bubble(const AIChatDock::ChatMessage &p_message, int p_message_index) {
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

	// Role label with edit button for user messages
	HBoxContainer *role_container = memnew(HBoxContainer);
	message_vbox->add_child(role_container);
	
	Label *role_label = memnew(Label);
	role_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
	role_label->set_text(p_message.role.capitalize());
	role_label->add_theme_color_override("font_color", role_color);
	role_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	role_container->add_child(role_label);
	
	// Add edit button for user messages only
	if (p_message.role == "user" && p_message_index >= 0) {
		print_line("AI Chat: Creating edit button for user message at index: " + String::num(p_message_index));
		Button *edit_button = memnew(Button);
		edit_button->set_text("Edit");
		edit_button->set_custom_minimum_size(Size2(50, 20));
		edit_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Edit"), SNAME("EditorIcons")));
		
		edit_button->connect("pressed", callable_mp(this, &AIChatDock::_on_edit_message_pressed).bind(p_message_index));
		role_container->add_child(edit_button);
		print_line("AI Chat: Edit button created and connected for index: " + String::num(p_message_index));
	}

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
		String bbcode_content = _markdown_to_bbcode(p_message.content);
		if (!bbcode_content.is_empty()) {
			content_label->set_text(bbcode_content);
		}
		message_panel->set_visible(true);
	}

	if (!p_message.tool_calls.is_empty()) {
		message_panel->set_visible(true);
		// Recreate tool call placeholders when loading saved conversations
		_create_tool_call_bubbles(p_message.tool_calls);
	}
	
	// Handle tool result messages specially
	if (p_message.role == "tool" && !p_message.tool_call_id.is_empty()) {
		message_panel->set_visible(true);
		// Find and update the corresponding tool placeholder
		_update_tool_placeholder_with_result(p_message);
	}
	
	// Display attached files if any
	if (!p_message.attached_files.is_empty()) {
		message_panel->set_visible(true);
		
		// Track displayed generated images to avoid duplication
		HashSet<String> displayed_generated_images;
		
		// Display all attached files with unified UI
		for (const AttachedFile &file : p_message.attached_files) {
			if (file.is_image && !file.base64_data.is_empty()) {
				// Create metadata dictionary for unified display
				Dictionary metadata;
				metadata["name"] = file.name;
				metadata["path"] = file.path;
				metadata["mime_type"] = file.mime_type;
				metadata["original_size_x"] = file.original_size.x;
				metadata["original_size_y"] = file.original_size.y;
				metadata["was_downsampled"] = file.was_downsampled;
				
				// For generated images, track them to prevent duplication
				if (file.path.begins_with("generated://")) {
					displayed_generated_images.insert(file.base64_data);
				}
				
				_display_image_unified(message_vbox, file.base64_data, metadata);
			} else if (!file.is_image) {
				// Display non-image files with existing logic
				VBoxContainer *files_container = memnew(VBoxContainer);
				message_vbox->add_child(files_container);
				
				Label *files_header = memnew(Label);
				files_header->set_text("Attached Files:");
				files_header->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
				files_header->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
				files_container->add_child(files_header);
				
				HFlowContainer *files_flow = memnew(HFlowContainer);
				files_container->add_child(files_flow);
				
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
		
		// Store displayed images to prevent duplication in tool_results section  
		current_displayed_images = displayed_generated_images;
	}
	
	// Display tool results (like generated images) if any, but avoid duplication
	if (!p_message.tool_results.is_empty()) {
		message_panel->set_visible(true);
		
		for (int i = 0; i < p_message.tool_results.size(); i++) {
			Dictionary tool_result = p_message.tool_results[i];
			
			// Check if this is an image generation result
			if (tool_result.get("success", false) && tool_result.has("image_data")) {
				String image_data = tool_result.get("image_data", "");
				String prompt = tool_result.get("prompt", "Generated Image");
				
				// Only display if not already displayed in attached_files section
				if (!image_data.is_empty() && !current_displayed_images.has(image_data)) {
					print_line("AI Chat: Displaying saved image from tool result: " + prompt + " (" + String::num(image_data.length()) + " chars base64)");
					
					// Create metadata for unified display
					Dictionary metadata;
					metadata["prompt"] = prompt;
					metadata["model"] = tool_result.get("model", "DALL-E");
					metadata["path"] = "generated://tool_result";
					
					_display_image_unified(message_vbox, image_data, metadata);
				}
			}
		}
	}
	
	// Clear the displayed images set for next message
	current_displayed_images.clear();

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

void AIChatDock::_update_tool_placeholder_with_result(const ChatMessage &p_tool_message) {
	if (chat_container == nullptr) {
		return;
	}
	
	// Find the placeholder for this tool call ID
	PanelContainer *placeholder = Object::cast_to<PanelContainer>(chat_container->find_child("tool_placeholder_" + p_tool_message.tool_call_id, true, false));
	if (!placeholder) {
		// If no placeholder exists, this might be a saved conversation 
		// where the tool call UI wasn't recreated yet. Skip for now.
		print_line("AI Chat: Warning - Could not find tool placeholder for ID: " + p_tool_message.tool_call_id + " (normal for loaded conversations)");
		return;
	}

	// Clear the "loading" text or any existing content immediately
	while (placeholder->get_child_count() > 0) {
		Node *child = placeholder->get_child(0);
		placeholder->remove_child(child);
		child->queue_free();
	}

	// Parse the tool result from JSON content with better error handling
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(p_tool_message.content);
	Dictionary result;
	if (err == OK) {
		result = json->get_data();
	} else {
		// If parsing fails, check if tool_results array has the data
		if (!p_tool_message.tool_results.is_empty()) {
			result = p_tool_message.tool_results[0]; // First element is the result
		} else {
			// Fallback if both JSON parsing and tool_results fail
			result["success"] = false;
			result["message"] = "Failed to parse tool result: " + p_tool_message.content;
		}
	}

	// Create the tool result UI using the same logic as _add_tool_response_to_chat
	VBoxContainer *tool_container = memnew(VBoxContainer);
	placeholder->add_child(tool_container);

	Button *toggle_button = memnew(Button);
	
	// Show success/failure status in the button
	bool success = result.get("success", false);
	String message = result.get("message", "");
	String status_text = success ? "SUCCESS" : "ERROR";
	toggle_button->set_text(status_text + " - " + p_tool_message.name + ": " + message);
	
	toggle_button->set_flat(false);
	toggle_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toggle_button->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	toggle_button->add_theme_icon_override("icon", get_theme_icon(success ? SNAME("StatusSuccess") : SNAME("StatusError"), SNAME("EditorIcons")));
	toggle_button->add_theme_color_override("font_color", success ? get_theme_color(SNAME("success_color"), SNAME("Editor")) : get_theme_color(SNAME("error_color"), SNAME("Editor")));
	tool_container->add_child(toggle_button);

	PanelContainer *content_panel = memnew(PanelContainer);
	content_panel->set_visible(false); // Collapsed by default
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

	Label *status_label = memnew(Label);
	status_label->set_text(success ? "Tool Succeeded" : "Tool Failed");
	status_label->add_theme_color_override("font_color", success ? get_theme_color(SNAME("success_color"), SNAME("Editor")) : get_theme_color(SNAME("error_color"), SNAME("Editor")));
	status_label->add_theme_icon_override("icon", get_theme_icon(success ? SNAME("StatusSuccess") : SNAME("StatusError"), SNAME("EditorIcons")));
	header_hbox->add_child(status_label);

	content_vbox->add_child(memnew(HSeparator));

	// Extract arguments if they were stored - better error handling
	Dictionary args;
	if (p_tool_message.tool_results.size() > 1) {
		args = p_tool_message.tool_results[1]; // Args are stored as second element
	}

	// Create specific UI based on the tool that was called
	_create_tool_specific_ui(content_vbox, p_tool_message.name, result, success, args);
}

void AIChatDock::_create_tool_specific_ui(VBoxContainer *p_content_vbox, const String &p_tool_name, const Dictionary &p_result, bool p_success, const Dictionary &p_args) {
	Ref<JSON> json;
	json.instantiate();
	
	// Create specific UI based on the tool that was called
	if (p_tool_name == "list_project_files" && p_success) {
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
		p_content_vbox->add_child(file_tree);

	} else if (p_tool_name == "read_file_content" && p_success) {
		VBoxContainer *file_content_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(file_content_vbox);

		// Get the file path from the original arguments
		String file_path = p_args.get("path", p_result.get("file_path", "Unknown file"));
		
		Button *file_link = memnew(Button);
		file_link->set_text(file_path);
		file_link->set_flat(true);
		file_link->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
		file_link->add_theme_icon_override("icon", get_theme_icon(SNAME("File"), SNAME("EditorIcons")));
		file_link->connect("pressed", callable_mp(this, &AIChatDock::_on_tool_file_link_pressed).bind(file_path));
		file_content_vbox->add_child(file_link);

		TextEdit *file_content = memnew(TextEdit);
		file_content->set_text(p_result.get("content", ""));
		file_content->set_editable(false);
		file_content->set_custom_minimum_size(Size2(0, 300));
		file_content_vbox->add_child(file_content);

	} else if (p_tool_name == "get_scene_info" && p_success) {
		VBoxContainer *scene_info_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(scene_info_vbox);

		Dictionary root_node = p_result.get("root_node", Dictionary());
		String scene_name = p_result.get("scene_name", "Unknown");
		
		// Scene info summary
		Label *scene_label = memnew(Label);
		scene_label->set_text("Scene: " + scene_name);
		scene_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		scene_info_vbox->add_child(scene_label);
		
		if (!root_node.is_empty()) {
			HBoxContainer *root_hbox = memnew(HBoxContainer);
			scene_info_vbox->add_child(root_hbox);
			
			Label *root_label = memnew(Label);
			String root_name = root_node.get("name", "Unknown");
			String root_type = root_node.get("type", "Unknown");
			root_label->set_text("Root Node: " + root_name + " (" + root_type + ")");
			root_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Node"), SNAME("EditorIcons")));
			root_hbox->add_child(root_label);
			
			// Child count
			int child_count = root_node.get("child_count", 0);
			Label *child_label = memnew(Label);
			child_label->set_text("Children: " + String::num_int64(child_count));
			root_hbox->add_child(child_label);
		}

	} else if (p_tool_name == "get_all_nodes" && p_success) {
		VBoxContainer *nodes_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(nodes_vbox);

		Array nodes = p_result.get("nodes", Array());
		
		Label *count_label = memnew(Label);
		count_label->set_text("Found " + String::num_int64(nodes.size()) + " nodes:");
		count_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		nodes_vbox->add_child(count_label);
		
		// Create a tree for better organization
		Tree *nodes_tree = memnew(Tree);
		nodes_tree->set_hide_root(true);
		nodes_tree->set_custom_minimum_size(Size2(0, 200));
		nodes_tree->set_columns(2); // FIX: Set to 2 columns before accessing column 1
		TreeItem *root = nodes_tree->create_item();
		
		for (int i = 0; i < nodes.size(); i++) {
			Dictionary node = nodes[i];
			TreeItem *item = nodes_tree->create_item(root);
			
			String name = node.get("name", "Unknown");
			String type = node.get("type", "Unknown");
			String path = node.get("path", "");
			
			item->set_text(0, name + " (" + type + ")");
			item->set_tooltip_text(0, "Path: " + path);
			item->set_icon(0, get_theme_icon(SNAME("Node"), SNAME("EditorIcons")));
			
			// Show child count if available
			int child_count = node.get("child_count", -1);
			if (child_count >= 0) {
				item->set_text(1, String::num_int64(child_count) + " children");
			}
		}
		
		nodes_tree->set_column_title(0, "Node");
		nodes_tree->set_column_title(1, "Children");
		nodes_tree->set_column_titles_visible(true);
		nodes_vbox->add_child(nodes_tree);

	} else if (p_tool_name == "create_node" && p_success) {
		VBoxContainer *create_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(create_vbox);

		String node_name = p_result.get("node_name", "Unknown");
		String node_type = p_result.get("node_type", "Unknown");
		String parent_path = p_result.get("parent_path", "Unknown");
		
		HBoxContainer *node_hbox = memnew(HBoxContainer);
		create_vbox->add_child(node_hbox);
		
		Label *node_label = memnew(Label);
		node_label->set_text("Created: " + node_name + " (" + node_type + ")");
		node_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Node"), SNAME("EditorIcons")));
		node_hbox->add_child(node_label);
		
		Label *parent_label = memnew(Label);
		parent_label->set_text("Parent: " + parent_path);
		parent_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
		create_vbox->add_child(parent_label);

	} else if (p_tool_name == "search_nodes_by_type" && p_success) {
		VBoxContainer *search_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(search_vbox);

		Array nodes = p_result.get("nodes", Array());
		String node_type = p_args.get("node_type", "Unknown");
		
		Label *count_label = memnew(Label);
		count_label->set_text("Found " + String::num_int64(nodes.size()) + " nodes of type: " + node_type);
		count_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		search_vbox->add_child(count_label);
		
		if (nodes.size() > 0) {
			Tree *nodes_tree = memnew(Tree);
			nodes_tree->set_hide_root(true);
			nodes_tree->set_custom_minimum_size(Size2(0, 150));
			TreeItem *root = nodes_tree->create_item();
			
			for (int i = 0; i < nodes.size(); i++) {
				Dictionary node = nodes[i];
				TreeItem *item = nodes_tree->create_item(root);
				
				String name = node.get("name", "Unknown");
				String path = node.get("path", "");
				
				item->set_text(0, name);
				item->set_tooltip_text(0, "Path: " + path);
				item->set_icon(0, get_theme_icon(SNAME("Node"), SNAME("EditorIcons")));
			}
			search_vbox->add_child(nodes_tree);
		}

	} else if (p_tool_name == "get_editor_selection" && p_success) {
		VBoxContainer *selection_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(selection_vbox);

		Array selected_nodes = p_result.get("selected_nodes", Array());
		
		Label *count_label = memnew(Label);
		count_label->set_text("Selected Nodes: " + String::num_int64(selected_nodes.size()));
		count_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		selection_vbox->add_child(count_label);
		
		if (selected_nodes.size() > 0) {
			for (int i = 0; i < selected_nodes.size(); i++) {
				Dictionary node = selected_nodes[i];
				HBoxContainer *node_hbox = memnew(HBoxContainer);
				selection_vbox->add_child(node_hbox);
				
				Label *node_label = memnew(Label);
				String name = node.get("name", "Unknown");
				String type = node.get("type", "Unknown");
				node_label->set_text(name + " (" + type + ")");
				node_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Node"), SNAME("EditorIcons")));
				node_hbox->add_child(node_label);
			}
		} else {
			Label *empty_label = memnew(Label);
			empty_label->set_text("No nodes selected");
			empty_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.6));
			selection_vbox->add_child(empty_label);
		}

	} else if (p_tool_name == "get_node_properties" && p_success) {
		VBoxContainer *props_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(props_vbox);

		String node_path = p_args.get("node_path", "Unknown");
		Dictionary properties = p_result.get("properties", Dictionary());
		
		Label *node_label = memnew(Label);
		node_label->set_text("Properties for: " + node_path);
		node_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		props_vbox->add_child(node_label);
		
		if (properties.size() > 0) {
			Tree *props_tree = memnew(Tree);
			props_tree->set_hide_root(true);
			props_tree->set_custom_minimum_size(Size2(0, 200));
			props_tree->set_columns(2); // FIX: Set to 2 columns before accessing column 1
			TreeItem *root = props_tree->create_item();
			props_tree->set_column_title(0, "Property");
			props_tree->set_column_title(1, "Value");
			props_tree->set_column_titles_visible(true);
			
			Array keys = properties.keys();
			for (int i = 0; i < keys.size(); i++) {
				TreeItem *item = props_tree->create_item(root);
				String key = keys[i];
				String value = String(properties[key]);
				item->set_text(0, key);
				item->set_text(1, value);
			}
			props_vbox->add_child(props_tree);
		}

	} else if (p_tool_name == "delete_node" && p_success) {
		VBoxContainer *delete_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(delete_vbox);

		String node_path = p_args.get("node_path", "Unknown");
		
		HBoxContainer *delete_hbox = memnew(HBoxContainer);
		delete_vbox->add_child(delete_hbox);
		
		Label *delete_label = memnew(Label);
		delete_label->set_text("Deleted node: " + node_path);
		delete_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Remove"), SNAME("EditorIcons")));
		delete_label->add_theme_color_override("font_color", get_theme_color(SNAME("warning_color"), SNAME("Editor")));
		delete_hbox->add_child(delete_label);

	} else if (p_tool_name == "set_node_property" && p_success) {
		VBoxContainer *set_prop_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(set_prop_vbox);

		String node_path = p_args.get("node_path", "Unknown");
		String property = p_args.get("property", "Unknown");
		String value = String(p_args.get("value", ""));
		
		Label *node_label = memnew(Label);
		node_label->set_text("Updated: " + node_path);
		node_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		set_prop_vbox->add_child(node_label);
		
		HBoxContainer *prop_hbox = memnew(HBoxContainer);
		set_prop_vbox->add_child(prop_hbox);
		
		Label *prop_label = memnew(Label);
		prop_label->set_text(property + " = " + value);
		prop_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Edit"), SNAME("EditorIcons")));
		prop_hbox->add_child(prop_label);

	} else if (p_tool_name == "save_scene" && p_success) {
		VBoxContainer *save_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(save_vbox);

		String scene_path = p_result.get("scene_path", "Unknown");
		
		HBoxContainer *save_hbox = memnew(HBoxContainer);
		save_vbox->add_child(save_hbox);
		
		Label *save_label = memnew(Label);
		save_label->set_text("Scene saved: " + scene_path);
		save_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Save"), SNAME("EditorIcons")));
		save_label->add_theme_color_override("font_color", get_theme_color(SNAME("success_color"), SNAME("Editor")));
		save_hbox->add_child(save_label);

	} else if (p_tool_name == "get_available_classes" && p_success) {
		VBoxContainer *classes_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(classes_vbox);

		Array classes = p_result.get("classes", Array());
		
		Label *count_label = memnew(Label);
		count_label->set_text("Available Classes: " + String::num_int64(classes.size()));
		count_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		classes_vbox->add_child(count_label);
		
		if (classes.size() > 0) {
			Tree *classes_tree = memnew(Tree);
			classes_tree->set_hide_root(true);
			classes_tree->set_custom_minimum_size(Size2(0, 200));
			TreeItem *root = classes_tree->create_item();
			
			for (int i = 0; i < classes.size(); i++) {
				TreeItem *item = classes_tree->create_item(root);
				String class_name = classes[i];
				item->set_text(0, class_name);
				item->set_icon(0, get_theme_icon(SNAME("Object"), SNAME("EditorIcons")));
			}
			classes_vbox->add_child(classes_tree);
		}

	} else if (p_tool_name == "move_node" && p_success) {
		VBoxContainer *move_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(move_vbox);

		String node_path = p_args.get("node_path", "Unknown");
		String new_parent = p_args.get("new_parent", "Unknown");
		
		Label *move_label = memnew(Label);
		move_label->set_text("Moved: " + node_path);
		move_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		move_vbox->add_child(move_label);
		
		Label *parent_label = memnew(Label);
		parent_label->set_text("New Parent: " + new_parent);
		parent_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
		move_vbox->add_child(parent_label);

	} else if (p_tool_name == "search_project_files" && p_success) {
		VBoxContainer *search_files_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(search_files_vbox);

		Array files = p_result.get("files", Array());
		String search_term = p_args.get("search_term", "");
		
		Label *count_label = memnew(Label);
		count_label->set_text("Found " + String::num_int64(files.size()) + " files matching: " + search_term);
		count_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		search_files_vbox->add_child(count_label);
		
		if (files.size() > 0) {
			Tree *files_tree = memnew(Tree);
			files_tree->set_hide_root(true);
			files_tree->set_custom_minimum_size(Size2(0, 200));
			TreeItem *root = files_tree->create_item();
			
			for (int i = 0; i < files.size(); i++) {
				String file_path = files[i];
				TreeItem *item = files_tree->create_item(root);
				item->set_text(0, file_path);
				item->set_icon(0, get_theme_icon(SNAME("File"), SNAME("EditorIcons")));
			}
			search_files_vbox->add_child(files_tree);
		}

	} else if (p_tool_name == "apply_edit" && p_success) {
		VBoxContainer *edit_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(edit_vbox);

		String file_path = p_args.get("file_path", "Unknown");
		Dictionary diff = p_result.get("diff", Dictionary());
		
		Label *file_label = memnew(Label);
		file_label->set_text("Applied edit to: " + file_path);
		file_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		edit_vbox->add_child(file_label);
		
		if (!diff.is_empty()) {
			Label *changes_label = memnew(Label);
			int added = diff.get("lines_added", 0);
			int removed = diff.get("lines_removed", 0);
			changes_label->set_text("Changes: +" + String::num_int64(added) + " -" + String::num_int64(removed));
			changes_label->add_theme_color_override("font_color", get_theme_color(SNAME("success_color"), SNAME("Editor")));
			edit_vbox->add_child(changes_label);
		}

	} else if (p_tool_name == "get_scene_tree_hierarchy" && p_success) {
		VBoxContainer *hierarchy_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(hierarchy_vbox);

		Dictionary hierarchy = p_result.get("hierarchy", Dictionary());
		
		Label *title_label = memnew(Label);
		title_label->set_text("Scene Tree Hierarchy:");
		title_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		hierarchy_vbox->add_child(title_label);
		
		// Create a simplified hierarchy display
		Tree *hierarchy_tree = memnew(Tree);
		hierarchy_tree->set_hide_root(true);
		hierarchy_tree->set_custom_minimum_size(Size2(0, 300));
		TreeItem *root = hierarchy_tree->create_item();
		
		// Build tree recursively (simplified version)
		if (!hierarchy.is_empty()) {
			_build_hierarchy_tree_item(hierarchy_tree, root, hierarchy);
		}
		
		hierarchy_vbox->add_child(hierarchy_tree);

	} else if (p_tool_name == "take_screenshot" && p_success) {
		VBoxContainer *screenshot_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(screenshot_vbox);

		String image_path = p_result.get("image_path", "");
		
		HBoxContainer *screenshot_hbox = memnew(HBoxContainer);
		screenshot_vbox->add_child(screenshot_hbox);
		
		Label *screenshot_label = memnew(Label);
		screenshot_label->set_text("Screenshot saved: " + image_path);
		screenshot_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Image"), SNAME("EditorIcons")));
		screenshot_label->add_theme_color_override("font_color", get_theme_color(SNAME("success_color"), SNAME("Editor")));
		screenshot_hbox->add_child(screenshot_label);

	} else if (p_tool_name == "create_script_file" && p_success) {
		VBoxContainer *script_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(script_vbox);

		String script_path = p_result.get("script_path", "Unknown");
		String script_language = p_args.get("language", "GDScript");
		
		Label *script_label = memnew(Label);
		script_label->set_text("Created " + script_language + " script:");
		script_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		script_vbox->add_child(script_label);
		
		Button *file_link = memnew(Button);
		file_link->set_text(script_path);
		file_link->set_flat(true);
		file_link->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
		file_link->add_theme_icon_override("icon", get_theme_icon(SNAME("Script"), SNAME("EditorIcons")));
		file_link->connect("pressed", callable_mp(this, &AIChatDock::_on_tool_file_link_pressed).bind(script_path));
		script_vbox->add_child(file_link);

	} else if (p_tool_name == "attach_script" && p_success) {
		VBoxContainer *attach_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(attach_vbox);

		String node_path = p_args.get("node_path", "Unknown");
		String script_path = p_args.get("script_path", "Unknown");
		
		Label *attach_label = memnew(Label);
		attach_label->set_text("Attached script to: " + node_path);
		attach_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		attach_vbox->add_child(attach_label);
		
		Label *script_label = memnew(Label);
		script_label->set_text("Script: " + script_path);
		script_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
		attach_vbox->add_child(script_label);

	} else if (p_tool_name == "run_scene" && p_success) {
		VBoxContainer *run_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(run_vbox);

		String scene_path = p_args.get("scene_path", "Current scene");
		
		HBoxContainer *run_hbox = memnew(HBoxContainer);
		run_vbox->add_child(run_hbox);
		
		Label *run_label = memnew(Label);
		run_label->set_text("Running scene: " + scene_path);
		run_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Play"), SNAME("EditorIcons")));
		run_label->add_theme_color_override("font_color", get_theme_color(SNAME("success_color"), SNAME("Editor")));
		run_hbox->add_child(run_label);

	} else if (p_tool_name == "check_compilation_errors" && p_success) {
		VBoxContainer *compile_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(compile_vbox);

		Array errors = p_result.get("errors", Array());
		
		Label *status_label = memnew(Label);
		if (errors.size() == 0) {
			status_label->set_text("✓ No compilation errors found");
			status_label->add_theme_color_override("font_color", get_theme_color(SNAME("success_color"), SNAME("Editor")));
		} else {
			status_label->set_text("✗ Found " + String::num_int64(errors.size()) + " compilation errors");
			status_label->add_theme_color_override("font_color", get_theme_color(SNAME("error_color"), SNAME("Editor")));
		}
		status_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		compile_vbox->add_child(status_label);
		
		if (errors.size() > 0) {
			for (int i = 0; i < errors.size(); i++) {
				Dictionary error = errors[i];
				VBoxContainer *error_vbox = memnew(VBoxContainer);
				compile_vbox->add_child(error_vbox);
				
				Label *error_label = memnew(Label);
				String file = error.get("file", "Unknown");
				int line = error.get("line", 0);
				String message = error.get("message", "Unknown error");
				error_label->set_text(file + ":" + String::num_int64(line) + " - " + message);
				error_label->add_theme_color_override("font_color", get_theme_color(SNAME("error_color"), SNAME("Editor")));
				error_vbox->add_child(error_label);
			}
		}

	} else if (p_tool_name == "image_operation" && p_success) {
		// Special handling for image generation results
		String base64_data = p_result.get("image_data", "");
		
		if (!base64_data.is_empty()) {
			// Display the generated image with unified method
			Dictionary metadata;
			metadata["prompt"] = p_result.get("prompt", "Generated Image");
			metadata["model"] = p_result.get("model", "DALL-E");
			metadata["path"] = "generated://tool_operation";
			
			_display_image_unified(p_content_vbox, base64_data, metadata);
		} else {
			// Fallback to text display if no image data
			RichTextLabel *content_label = memnew(RichTextLabel);
			content_label->add_theme_font_override("normal_font", get_theme_font(SNAME("source"), SNAME("EditorFonts")));
			content_label->set_text(json->stringify(p_result, "  "));
			content_label->set_fit_content(true);
			content_label->set_selection_enabled(true);
			p_content_vbox->add_child(content_label);
		}
	} else if (p_tool_name == "search_across_project" && p_success) {
		// Display search results with nice formatting
		VBoxContainer *search_vbox = memnew(VBoxContainer);
		p_content_vbox->add_child(search_vbox);

		Dictionary results = p_result.get("results", Dictionary());
		Array similar_files = results.get("similar_files", Array());
		Array central_files = results.get("central_files", Array());
		Dictionary graph_summary = results.get("graph_summary", Dictionary());
		String query = p_args.get("query", "Unknown query");
		
		// Header with query info
		Label *query_label = memnew(Label);
		query_label->set_text("Search Results for: \"" + query + "\"");
		query_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		query_label->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
		search_vbox->add_child(query_label);
		
		search_vbox->add_child(memnew(HSeparator));
		
		// Similar files section
		if (similar_files.size() > 0) {
			Label *similar_header = memnew(Label);
			similar_header->set_text("📁 Similar Files (" + String::num_int64(similar_files.size()) + ")");
			similar_header->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
			search_vbox->add_child(similar_header);
			
			for (int i = 0; i < similar_files.size(); i++) {
				Dictionary file_result = similar_files[i];
				String file_path = file_result.get("file_path", "");
				float similarity = file_result.get("similarity", 0.0);
				String modality = file_result.get("modality", "text");
				
				HBoxContainer *file_hbox = memnew(HBoxContainer);
				search_vbox->add_child(file_hbox);
				
				// File link button
				Button *file_link = memnew(Button);
				file_link->set_text(file_path);
				file_link->set_flat(true);
				file_link->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
				file_link->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				file_link->add_theme_icon_override("icon", get_theme_icon(SNAME("File"), SNAME("EditorIcons")));
				file_link->connect("pressed", callable_mp(this, &AIChatDock::_on_tool_file_link_pressed).bind(file_path));
				file_hbox->add_child(file_link);
				
				// Similarity score
				Label *score_label = memnew(Label);
				score_label->set_text(String::num(similarity, 3) + " (" + modality + ")");
				score_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
				score_label->set_custom_minimum_size(Size2(120, 0));
				file_hbox->add_child(score_label);
			}
		}
		
		// Central files section
		if (central_files.size() > 0) {
			search_vbox->add_child(memnew(HSeparator));
			
			Label *central_header = memnew(Label);
			central_header->set_text("⭐ Central Files (" + String::num_int64(central_files.size()) + ")");
			central_header->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
			search_vbox->add_child(central_header);
			
			for (int i = 0; i < central_files.size(); i++) {
				Dictionary central_file = central_files[i];
				String file_path = central_file.get("file_path", "");
				float centrality = central_file.get("centrality", 0.0);
				
				HBoxContainer *central_hbox = memnew(HBoxContainer);
				search_vbox->add_child(central_hbox);
				
				// File link button
				Button *file_link = memnew(Button);
				file_link->set_text(file_path);
				file_link->set_flat(true);
				file_link->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
				file_link->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				file_link->add_theme_icon_override("icon", get_theme_icon(SNAME("File"), SNAME("EditorIcons")));
				file_link->connect("pressed", callable_mp(this, &AIChatDock::_on_tool_file_link_pressed).bind(file_path));
				central_hbox->add_child(file_link);
				
				// Centrality score
				Label *centrality_label = memnew(Label);
				centrality_label->set_text("Centrality: " + String::num(centrality, 3));
				centrality_label->add_theme_color_override("font_color", get_theme_color(SNAME("warning_color"), SNAME("Editor")));
				centrality_label->set_custom_minimum_size(Size2(120, 0));
				central_hbox->add_child(centrality_label);
			}
		}
		
		// Graph summary section
		if (!graph_summary.is_empty()) {
			search_vbox->add_child(memnew(HSeparator));
			
			Label *graph_header = memnew(Label);
			graph_header->set_text("🔗 Project Graph Summary");
			graph_header->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
			search_vbox->add_child(graph_header);
			
			int total_files = graph_summary.get("total_files", 0);
			int total_connections = graph_summary.get("total_connections", 0);
			
			Label *summary_label = memnew(Label);
			summary_label->set_text("Files: " + String::num_int64(total_files) + " • Connections: " + String::num_int64(total_connections));
			summary_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.8));
			search_vbox->add_child(summary_label);
		}
		
	} else {
		// Default case - show formatted JSON output with better styling
		VBoxContainer *json_container = memnew(VBoxContainer);
		p_content_vbox->add_child(json_container);
		
		Label *json_header = memnew(Label);
		json_header->set_text("Tool Result Data:");
		json_header->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
		json_header->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
		json_container->add_child(json_header);
		
		RichTextLabel *content_label = memnew(RichTextLabel);
		content_label->add_theme_font_override("normal_font", get_theme_font(SNAME("source"), SNAME("EditorFonts")));
		content_label->set_text(json->stringify(p_result, "  "));
		content_label->set_fit_content(true);
		content_label->set_selection_enabled(true);
		content_label->set_custom_minimum_size(Size2(0, 150));
		json_container->add_child(content_label);
	}
}

void AIChatDock::_rebuild_conversation_ui(const Vector<ChatMessage> &p_messages) {
	// Build a mapping of tool call IDs to their results for efficient lookup
	HashMap<String, ChatMessage> tool_results;
	for (int i = 0; i < p_messages.size(); i++) {
		const ChatMessage &msg = p_messages[i];
		if (msg.role == "tool" && !msg.tool_call_id.is_empty()) {
			tool_results[msg.tool_call_id] = msg;
		}
	}
	
	// First pass: Create all non-tool messages (this includes assistant messages with tool_calls)
	for (int i = 0; i < p_messages.size(); i++) {
		const ChatMessage &msg = p_messages[i];
		if (msg.role != "tool") {
			_create_message_bubble(msg, i);
		}
	}
	
	// Second pass: Apply tool results to their corresponding placeholders
	for (int i = 0; i < p_messages.size(); i++) {
		const ChatMessage &msg = p_messages[i];
		if (msg.role == "tool" && !msg.tool_call_id.is_empty()) {
			// Use a small delay to ensure the UI has been updated from the first pass
			call_deferred("_apply_tool_result_deferred", msg.tool_call_id, msg.name, msg.content, msg.tool_results);
		}
	}
}

void AIChatDock::_apply_tool_result_deferred(const String &p_tool_call_id, const String &p_tool_name, const String &p_content, const Array &p_tool_results) {
	// Find the placeholder for this tool call ID
	if (chat_container == nullptr) {
		return;
	}
	
	PanelContainer *placeholder = Object::cast_to<PanelContainer>(chat_container->find_child("tool_placeholder_" + p_tool_call_id, true, false));
	if (!placeholder) {
		print_line("AI Chat: Warning - Could not find tool placeholder for ID: " + p_tool_call_id);
		return;
	}

	// Clear the "loading" text or any existing content immediately
	while (placeholder->get_child_count() > 0) {
		Node *child = placeholder->get_child(0);
		placeholder->remove_child(child);
		child->queue_free();
	}

	// Parse the tool result from JSON content
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(p_content);
	Dictionary result;
	if (err == OK) {
		result = json->get_data();
	} else {
		// Fallback if JSON parsing fails
		result["success"] = false;
		result["message"] = "Failed to parse tool result: " + p_content;
	}

	// Extract arguments if they were stored
	Dictionary args;
	if (p_tool_results.size() > 1) {
		args = p_tool_results[1]; // Args are stored as second element
	}

	// Create the tool result UI
	VBoxContainer *tool_container = memnew(VBoxContainer);
	placeholder->add_child(tool_container);

	Button *toggle_button = memnew(Button);
	
	// Show success/failure status in the button
	bool success = result.get("success", false);
	String message = result.get("message", "");
	String status_text = success ? "SUCCESS" : "ERROR";
	toggle_button->set_text(status_text + " - " + p_tool_name + ": " + message);
	
	toggle_button->set_flat(false);
	toggle_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toggle_button->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	toggle_button->add_theme_icon_override("icon", get_theme_icon(success ? SNAME("StatusSuccess") : SNAME("StatusError"), SNAME("EditorIcons")));
	toggle_button->add_theme_color_override("font_color", success ? get_theme_color(SNAME("success_color"), SNAME("Editor")) : get_theme_color(SNAME("error_color"), SNAME("Editor")));
	tool_container->add_child(toggle_button);

	PanelContainer *content_panel = memnew(PanelContainer);
	content_panel->set_visible(false); // Collapsed by default
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

	Label *status_label = memnew(Label);
	status_label->set_text(success ? "Tool Succeeded" : "Tool Failed");
	status_label->add_theme_color_override("font_color", success ? get_theme_color(SNAME("success_color"), SNAME("Editor")) : get_theme_color(SNAME("error_color"), SNAME("Editor")));
	status_label->add_theme_icon_override("icon", get_theme_icon(success ? SNAME("StatusSuccess") : SNAME("StatusError"), SNAME("EditorIcons")));
	header_hbox->add_child(status_label);

	content_vbox->add_child(memnew(HSeparator));

	// Create specific UI based on the tool that was called
	_create_tool_specific_ui(content_vbox, p_tool_name, result, success, args);
}

void AIChatDock::_on_tool_output_toggled(Control *p_content) {
	p_content->set_visible(!p_content->is_visible());
}

void AIChatDock::_send_chat_request() {
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	
	// For large conversation histories, process in chunks to prevent UI blocking
	if (chat_history.size() > 50) {
		print_line("AI Chat: Large conversation detected (" + String::num_int64(chat_history.size()) + " messages), processing in chunks");
		call_deferred("_send_chat_request_chunked", 0);
		return;
	}
	
	// Build the messages array for the API
	Array messages;
	messages.resize(chat_history.size());
	
	// Process messages efficiently for smaller conversations
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
					
					// Add image ID as text so AI knows how to reference it
					Dictionary text_part;
					text_part["type"] = "text";
					text_part["text"] = "\n*[Image ID: " + file.name + "]*";
					content_array.push_back(text_part);
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
			// Handle assistant messages with attached files (like generated images)
			if (msg.role == "assistant" && !msg.attached_files.is_empty()) {
				// Check if we have any images  
				bool has_images = false;
				for (const AttachedFile &file : msg.attached_files) {
					if (file.is_image) {
						has_images = true;
						break;
					}
				}
				
				if (has_images) {
								// For assistant messages with images, we need to structure them for OpenAI
			// Since OpenAI doesn't support assistant messages with image content,
			// we'll include the images as a special marker in the content
			String content_with_images = msg.content;
			content_with_images += "\n\n**Generated Images:**";
			
			for (const AttachedFile &file : msg.attached_files) {
				if (file.is_image) {
					content_with_images += "\n- Image ID: `" + file.name + "`";
				}
			}
					
					api_msg["content"] = content_with_images;
					
					// Also store image data in a custom field for the backend to understand
					Array images_data;
					for (const AttachedFile &file : msg.attached_files) {
						if (file.is_image) {
							Dictionary image_info;
							image_info["name"] = file.name;
							image_info["mime_type"] = file.mime_type;
							image_info["base64_data"] = file.base64_data;
							image_info["original_size"] = Vector2(file.original_size.x, file.original_size.y);
							images_data.push_back(image_info);
						}
					}
					if (!images_data.is_empty()) {
						api_msg["images"] = images_data;
					}
				} else {
					api_msg["content"] = msg.content;
				}
			} else {
				api_msg["content"] = msg.content;
			}
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

	// Store messages and route to finalize method
	_chunked_messages = messages;
	call_deferred("_finalize_chat_request");
}

void AIChatDock::_send_chat_request_chunked(int p_start_index) {
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	
	// Process messages in chunks of 10 to avoid blocking UI
	const int CHUNK_SIZE = 10;
	int end_index = MIN(p_start_index + CHUNK_SIZE, chat_history.size());
	
	// If this is the first chunk, initialize the messages array
	if (p_start_index == 0) {
		// Store the work in progress
		_chunked_messages.clear();
		_chunked_messages.resize(chat_history.size());
	}
	
	// Process this chunk
	for (int i = p_start_index; i < end_index; i++) {
		const ChatMessage &msg = chat_history[i];
		Dictionary api_msg = _build_api_message(msg);
		_chunked_messages[i] = api_msg;
	}
	
	print_line("AI Chat: Processed message chunk " + String::num_int64(p_start_index) + "-" + String::num_int64(end_index-1) + " of " + String::num_int64(chat_history.size()));
	
	// If more chunks to process, defer the next chunk
	if (end_index < chat_history.size()) {
		call_deferred("_send_chat_request_chunked", end_index);
		return;
	}
	
	// All chunks processed, now send the request
	call_deferred("_finalize_chat_request");
}

Dictionary AIChatDock::_build_api_message(const ChatMessage &p_msg) {
	Dictionary api_msg;
	api_msg["role"] = p_msg.role;
	
	// For user messages with attached files, handle images and text differently
	if (p_msg.role == "user" && !p_msg.attached_files.is_empty()) {
		// Check if we have any images
		bool has_images = false;
		for (const AttachedFile &file : p_msg.attached_files) {
			if (file.is_image) {
				has_images = true;
				break;
			}
		}
		
		if (has_images) {
			// Use the new multi-modal format for images
			Array content_array;
			
			// Add text content
			if (!p_msg.content.is_empty()) {
				Dictionary text_part;
				text_part["type"] = "text";
				text_part["text"] = p_msg.content;
				content_array.push_back(text_part);
			}
			
					// Add images and text files
		for (const AttachedFile &file : p_msg.attached_files) {
			if (file.is_image) {
				Dictionary image_part;
				image_part["type"] = "image_url";
				Dictionary image_url;
				image_url["url"] = "data:" + file.mime_type + ";base64," + file.base64_data;
				image_part["image_url"] = image_url;
				content_array.push_back(image_part);
				
				// Add image ID as text so AI knows how to reference it
				Dictionary text_part;
				text_part["type"] = "text";
				text_part["text"] = "\n*[Image ID: " + file.name + "]*";
				content_array.push_back(text_part);
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
			String combined_content = p_msg.content;
			combined_content += "\n\n**Attached Files:**\n";
			
			for (const AttachedFile &file : p_msg.attached_files) {
				combined_content += "\n### " + file.name + " (" + file.path + ")\n";
				combined_content += "```\n" + file.content + "\n```\n";
			}
			
			api_msg["content"] = combined_content;
		}
	} else {
		// Handle assistant messages with attached files (like generated images)
		if (p_msg.role == "assistant" && !p_msg.attached_files.is_empty()) {
			// Check if we have any images  
			bool has_images = false;
			for (const AttachedFile &file : p_msg.attached_files) {
				if (file.is_image) {
					has_images = true;
					break;
				}
			}
			
			if (has_images) {
						// For assistant messages with images, we need to structure them for OpenAI
		// Since OpenAI doesn't support assistant messages with image content,
		// we'll include the images as a special marker in the content
		String content_with_images = p_msg.content;
		content_with_images += "\n\n**Generated Images:**";
		
		for (const AttachedFile &file : p_msg.attached_files) {
			if (file.is_image) {
				content_with_images += "\n- Image ID: `" + file.name + "`";
			}
		}
				
				api_msg["content"] = content_with_images;
				
				// Also store image data in a custom field for the backend to understand
				Array images_data;
				for (const AttachedFile &file : p_msg.attached_files) {
					if (file.is_image) {
						Dictionary image_info;
						image_info["name"] = file.name;
						image_info["mime_type"] = file.mime_type;
						image_info["base64_data"] = file.base64_data;
						image_info["original_size"] = Vector2(file.original_size.x, file.original_size.y);
						images_data.push_back(image_info);
					}
				}
				if (!images_data.is_empty()) {
					api_msg["images"] = images_data;
				}
			} else {
				api_msg["content"] = p_msg.content;
			}
		} else {
			api_msg["content"] = p_msg.content;
		}
	}
	
	// Save attached files for ALL message types (user, assistant, etc.)
	if (!p_msg.attached_files.is_empty()) {
		Array attached_files_array;
		for (const AttachedFile &file : p_msg.attached_files) {
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
		api_msg["attached_files"] = attached_files_array;
	}
	
	// Save tool calls and tool data
	if (p_msg.role == "assistant" && !p_msg.tool_calls.is_empty()) {
		api_msg["tool_calls"] = p_msg.tool_calls;
	}
	if (p_msg.role == "tool") {
		api_msg["tool_call_id"] = p_msg.tool_call_id;
		api_msg["name"] = p_msg.name;
	}
	
	// Save tool results
	if (!p_msg.tool_results.is_empty()) {
		api_msg["tool_results"] = p_msg.tool_results;
	}
	
	// Save timestamp
	if (!p_msg.timestamp.is_empty()) {
		api_msg["timestamp"] = p_msg.timestamp;
	}
	
	return api_msg;
}

void AIChatDock::_finalize_chat_request() {
	// Build final request data
	Dictionary request_data;
	request_data["messages"] = _chunked_messages;

	// Debug logs for OpenAI messages have been quieted to reduce console noise.

	Ref<JSON> json;
	json.instantiate();
	String request_body = json->stringify(request_data);

	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	
	// Add authentication headers
	if (!auth_token.is_empty()) {
		headers.push_back("Authorization: Bearer " + auth_token);
	}
	headers.push_back("X-User-ID: " + current_user_id);
	headers.push_back("X-Machine-ID: " + get_machine_id());

	// Setup HTTPClient for streaming
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
	print_line("AI Chat: Attempting to connect to " + host + ":" + String::num_int64(port) + path);
	Error err = http_client->connect_to_host(host, port, (use_ssl ? TLSOptions::client_unsafe(Ref<X509Certificate>()) : Ref<TLSOptions>()));
	if (err != OK) {
		_add_message_to_chat("system", "Failed to connect to backend: " + host + ":" + String::num_int64(port) + " (Error: " + String::num_int64(err) + ")");
		is_waiting_for_response = false;
		_update_ui_state();
		return;
	}

	http_status = STATUS_CONNECTING;
	pending_request_path = path;
	pending_request_headers = headers;
	pending_request_body = request_body_data;

	// Clear response buffer for new request
	response_buffer.clear();

	set_process(true);
	
	// Clear the chunked messages to free memory
	_chunked_messages.clear();
}

void AIChatDock::_update_ui_state() {
	// Handle send button state
	send_button->set_disabled(input_field->get_text().strip_edges().is_empty() || is_waiting_for_response);
	input_field->set_editable(!is_waiting_for_response);

	// Handle stop button state
	if (is_waiting_for_response) {
		// Show stop button and hide/disable send button during request
		send_button->set_visible(false);
		stop_button->set_visible(true);
		bool should_disable_stop = current_request_id.is_empty();
		stop_button->set_disabled(should_disable_stop); // Only enable if we have a request ID
		
		print_line("AI Chat: UI State - waiting for response, stop button visible=" + String(stop_button->is_visible() ? "true" : "false") + ", disabled=" + String(should_disable_stop ? "true" : "false") + ", request_id='" + current_request_id + "'");
		
		// Also disable new conversation button during processing
		if (new_conversation_button) {
			new_conversation_button->set_disabled(true);
		}
	} else {
		// Show send button and hide stop button when not waiting
		send_button->set_visible(true);
		send_button->set_text("Send");
		stop_button->set_visible(false);
		
		print_line("AI Chat: UI State - not waiting, send button visible, stop button hidden");
		
		if (new_conversation_button) {
			new_conversation_button->set_disabled(false);
		}
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
	// Safety check for empty strings
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
	_queue_delayed_save();
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
		_queue_delayed_save();

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
			
			// Load attached files (skip heavy base64 data during initial load for performance)
			Array attached_files_array = msg_dict.get("attached_files", Array());
			for (int k = 0; k < attached_files_array.size(); k++) {
				Dictionary file_dict = attached_files_array[k];
				AIChatDock::AttachedFile file;
				file.path = file_dict.get("path", "");
				file.name = file_dict.get("name", "");
				file.content = file_dict.get("content", "");
				file.is_image = file_dict.get("is_image", false);
				file.mime_type = file_dict.get("mime_type", "");
				// Load base64_data only when conversation is being displayed
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
	// Don't save immediately - defer for better UI responsiveness
	
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

void AIChatDock::_create_new_conversation_instant() {
	// Instant version - no save operation to avoid blocking UI
	AIChatDock::Conversation new_conv;
	new_conv.id = _generate_conversation_id();
	new_conv.title = "New Conversation";
	new_conv.created_timestamp = _get_timestamp();
	new_conv.last_modified_timestamp = _get_timestamp();
	
	conversations.push_back(new_conv);
	current_conversation_index = conversations.size() - 1;
	// Note: Save is deferred in caller
	
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
	
	// Rebuild UI from conversation messages with proper tool call handling
	const Vector<AIChatDock::ChatMessage> &messages = conversations[p_index].messages;
	_rebuild_conversation_ui(messages);
	
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
	// PERFORMANCE OPTIMIZATION: Instant conversation creation with deferred save
	_create_new_conversation_instant();
	_update_conversation_dropdown();
	
	// Use delayed save to avoid UI blocking
	_queue_delayed_save();
}

void AIChatDock::_build_hierarchy_tree_item(Tree *p_tree, TreeItem *p_parent, const Dictionary &p_node_data) {
	if (p_node_data.is_empty()) {
		return;
	}
	
	TreeItem *item = p_tree->create_item(p_parent);
	String name = p_node_data.get("name", "Unknown");
	String type = p_node_data.get("type", "");
	
	if (!type.is_empty()) {
		item->set_text(0, name + " (" + type + ")");
	} else {
		item->set_text(0, name);
	}
	
	item->set_icon(0, get_theme_icon(SNAME("Node"), SNAME("EditorIcons")));
	
	// Recursively add children if they exist
	Array children = p_node_data.get("children", Array());
	for (int i = 0; i < children.size(); i++) {
		Dictionary child = children[i];
		_build_hierarchy_tree_item(p_tree, item, child);
	}
}

AIChatDock::AIChatDock() {
	set_name("AI Chat");
	
	// Enable drag and drop for the dock
	set_drag_forwarding(Callable(), callable_mp(this, &AIChatDock::can_drop_data_fw), callable_mp(this, &AIChatDock::drop_data_fw));

	// HTTP client for streaming API calls
	http_client = HTTPClient::create();
	
	// Timer for deferred saving
	save_timer = memnew(Timer);
	save_timer->set_one_shot(true);
	add_child(save_timer);
	save_timer->connect("timeout", callable_mp(this, &AIChatDock::_execute_delayed_save));
	
	// Initialize mutex for background saving
	save_mutex = memnew(Mutex);
	
	// HTTP request for stop requests (non-streaming)
	stop_http_request = memnew(HTTPRequest);
	add_child(stop_http_request);
	stop_http_request->connect("request_completed", callable_mp(this, &AIChatDock::_on_stop_request_completed));

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
	
	// Initialize embedding system
	call_deferred("_initialize_embedding_system");
}


void AIChatDock::_on_diff_accepted(const String &p_path, const String &p_content) {
    // Changes are now applied directly to script editor
    // No need to write to disk immediately - user can save when ready
    print_line("Diff accepted for: " + p_path);
}

void AIChatDock::send_error_message(const String &p_error_text) {
	String formatted_message = "Please help fix this error:\n\n" + p_error_text;
	
	// Set the message in the input field
	input_field->set_text(formatted_message);
	
	// Show the AI Chat dock if it's not visible
	// This will make the dock visible in the editor
	set_visible(true);
	
	// Focus on the AI Chat dock to bring user's attention
	input_field->grab_focus();
	
	// Automatically send the message
	call_deferred("_on_send_button_pressed");
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
	print_line("AI Chat: _handle_generated_image called with ID: " + p_id + ", data length: " + String::num_int64(p_base64_data.length()));
	
	if (p_base64_data.is_empty()) {
		print_line("AI Chat: _handle_generated_image - base64 data is empty, aborting");
		return;
	}
	
	// Defer image display to next frame to avoid UI race conditions during streaming
	print_line("AI Chat: _handle_generated_image - calling deferred _display_generated_image_deferred");
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
	
	// Safely find the last assistant message bubble without creating new ones
	PanelContainer *bubble_panel = nullptr;
	if (chat_container) {
		print_line("AI Chat: Searching for assistant message bubble, total children: " + String::num_int64(chat_container->get_child_count()));
		// Look for the last panel container (which should be our assistant message)
		for (int i = chat_container->get_child_count() - 1; i >= 0; i--) {
			Node *child = chat_container->get_child(i);
			print_line("AI Chat: Child " + String::num_int64(i) + " type: " + child->get_class());
			PanelContainer *panel = Object::cast_to<PanelContainer>(child);
			if (panel) {
				print_line("AI Chat: Found PanelContainer at index " + String::num_int64(i));
				bubble_panel = panel;
				break;
			}
		}
	}
	
	if (!bubble_panel) {
		print_line("AI Chat: Could not find assistant message bubble for generated image");
		return;
	} else {
		print_line("AI Chat: Successfully found bubble panel for image display");
	}

	
	// Find the VBoxContainer inside the message bubble
	VBoxContainer *message_vbox = nullptr;
	print_line("AI Chat: Searching for VBoxContainer in bubble panel, children count: " + String::num_int64(bubble_panel->get_child_count()));
	for (int i = 0; i < bubble_panel->get_child_count(); i++) {
		Node *child = bubble_panel->get_child(i);
		print_line("AI Chat: Bubble child " + String::num_int64(i) + " type: " + child->get_class());
		message_vbox = Object::cast_to<VBoxContainer>(child);
		if (message_vbox) {
			print_line("AI Chat: Found VBoxContainer at index " + String::num_int64(i));
			break;
		}
	}
	
	if (!message_vbox) {
		print_line("AI Chat: Could not find VBoxContainer in message bubble - aborting image display");
		return;
	}
	
	// Find and clear the tool placeholder containing "⚡ Calling tool..." text
	print_line("AI Chat: Searching for tool placeholder in message vbox, children count: " + String::num_int64(message_vbox->get_child_count()));
	bool found_placeholder = false;
	for (int i = 0; i < message_vbox->get_child_count(); i++) {
		Node *child = message_vbox->get_child(i);
		print_line("AI Chat: VBox child " + String::num_int64(i) + " type: " + child->get_class() + " name: " + child->get_name());
		
		// Look for tool placeholder panels
		PanelContainer *panel = Object::cast_to<PanelContainer>(child);
		if (panel && String(panel->get_name()).begins_with("tool_placeholder_")) {
			print_line("AI Chat: Found tool placeholder panel: " + panel->get_name());
			// Clear the tool placeholder content and replace with success message
			while (panel->get_child_count() > 0) {
				Node *panel_child = panel->get_child(0);
				panel->remove_child(panel_child);
				panel_child->queue_free();
			}
			
			// Add success message
			Label *success_label = memnew(Label);
			success_label->set_text("✓ Generated image");
			success_label->add_theme_color_override("font_color", get_theme_color(SNAME("success_color"), SNAME("Editor")));
			success_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
			panel->add_child(success_label);
			
			found_placeholder = true;
			break;
		}
		
		// Also check for RichTextLabel (fallback for other cases)
		RichTextLabel *label = Object::cast_to<RichTextLabel>(child);
		if (label && label->get_text().contains("Calling tool")) {
			print_line("AI Chat: Found RichTextLabel with tool text, updating");
			label->clear();
			label->append_text("✓ Generated image\n\n");
			found_placeholder = true;
			break;
		}
	}
	
	if (!found_placeholder) {
		print_line("AI Chat: No tool placeholder found to clear - this might be okay for some flows");
	}
	
	// Create image display container
	print_line("AI Chat: Creating image display container");
	PanelContainer *image_panel = memnew(PanelContainer);
	message_vbox->add_child(image_panel);
	print_line("AI Chat: Added image panel to message vbox");
	
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
	header_label->set_text("✨ Generated Image");
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
	
	// Add spacer to push save button to the right
	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	info_container->add_child(spacer);
	
	// Save button for generated images
	Button *save_button = memnew(Button);
	save_button->set_text("Save");
	save_button->set_flat(true);
	save_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Save"), SNAME("EditorIcons")));
	save_button->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	save_button->add_theme_color_override("icon_normal_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	save_button->set_tooltip_text("Save this image to your project");
	save_button->connect("pressed", callable_mp(this, &AIChatDock::_on_save_image_pressed).bind(p_base64_data, "png"));
	info_container->add_child(save_button);
	
	// Store the generated image in the current message
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	print_line("AI Chat: Attempting to save image to chat history, total messages: " + String::num_int64(chat_history.size()));
	
	if (!chat_history.is_empty()) {
		ChatMessage &last_msg = chat_history.write[chat_history.size() - 1];
		print_line("AI Chat: Last message role: " + last_msg.role + ", content: '" + last_msg.content.substr(0, 50) + "...'");
		print_line("AI Chat: Last message current attached files count: " + String::num_int64(last_msg.attached_files.size()));
		
		if (last_msg.role == "assistant") {
					// Add the generated image as an attachment for persistence
		AIChatDock::AttachedFile generated_file;
		generated_file.path = "generated://" + p_id;
		// Create proper unique ID for generated images
		generated_file.name = "gen_img_" + String::num_int64(OS::get_singleton()->get_ticks_msec());
		generated_file.is_image = true;
		generated_file.mime_type = "image/png";
			generated_file.base64_data = p_base64_data;
			generated_file.original_size = original_size;
			generated_file.display_size = display_size;
			generated_file.was_downsampled = (display_size != original_size);
					last_msg.attached_files.push_back(generated_file);
		print_line("AI Chat: Successfully added generated image ID: " + generated_file.name + " to assistant message");
		} else {
			print_line("AI Chat: Cannot save image - last message is not from assistant (role: " + last_msg.role + ")");
		}
	} else {
		print_line("AI Chat: Cannot save image - chat history is empty");
	}
	
	// Update conversation and scroll
	if (current_conversation_index >= 0) {
		conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
		_queue_delayed_save();
	}
	
	print_line("AI Chat: Image display complete, forcing UI refresh");
	
	// Force UI update to show the newly added image immediately
	if (bubble_panel) {
		bubble_panel->queue_redraw();
		print_line("AI Chat: Queued bubble_panel redraw");
	}
	if (chat_container) {
		chat_container->queue_redraw();
		print_line("AI Chat: Queued chat_container redraw");
	}
	// Also update the main dock
	queue_redraw();
	print_line("AI Chat: Queued main dock redraw");
	
	call_deferred("_scroll_to_bottom");
	print_line("AI Chat: _display_generated_image_deferred completed successfully");
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

void AIChatDock::_display_image_unified(VBoxContainer *p_container, const String &p_base64_data, const Dictionary &p_metadata) {
	if (!p_container || p_base64_data.is_empty()) {
		return;
	}
	
	// Decode base64 to image
	Vector<uint8_t> image_data = CoreBind::Marshalls::get_singleton()->base64_to_raw(p_base64_data);
	if (image_data.size() == 0) {
		print_line("AI Chat: Failed to decode image data");
		return;
	}
	
	// Create image from data
	Ref<Image> display_image = memnew(Image);
	Error err = display_image->load_png_from_buffer(image_data);
	if (err != OK) {
		// Try JPEG if PNG fails
		err = display_image->load_jpg_from_buffer(image_data);
		if (err != OK) {
			print_line("AI Chat: Failed to load image");
			return;
		}
	}
	
	if (display_image->is_empty()) {
		print_line("AI Chat: Image is empty");
		return;
	}
	
	// Create unified image display container
	VBoxContainer *image_container = memnew(VBoxContainer);
	p_container->add_child(image_container);
	
	// Extract metadata with defaults
	String title = p_metadata.get("prompt", p_metadata.get("name", "Image"));
	String model = p_metadata.get("model", "");
	String file_path = p_metadata.get("path", "");
	bool is_generated = file_path.begins_with("generated://");
	int max_display_size = is_generated ? 200 : 150; // Generated images slightly larger
	
	// Add image title/info
	HBoxContainer *info_container = memnew(HBoxContainer);
	image_container->add_child(info_container);
	
	HBoxContainer *title_container = memnew(HBoxContainer);
	info_container->add_child(title_container);
	
	Label *icon = memnew(Label);
	icon->add_theme_icon_override("icon", get_theme_icon(SNAME("Image"), SNAME("EditorIcons")));
	title_container->add_child(icon);
	
	Label *title_label = memnew(Label);
	title_label->set_text(title);
	title_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
	title_label->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	title_container->add_child(title_label);
	
	// Resize image for display
	Vector2i original_size = Vector2i(display_image->get_width(), display_image->get_height());
	Vector2i display_size = _calculate_downsampled_size(original_size, max_display_size);
	
	if (display_size != original_size) {
		display_image->resize(display_size.x, display_size.y, Image::INTERPOLATE_LANCZOS);
	}
	
	// Create texture and display
	Ref<ImageTexture> image_texture = ImageTexture::create_from_image(display_image);
	
	TextureRect *image_display = memnew(TextureRect);
	image_display->set_texture(image_texture);
	image_display->set_expand_mode(TextureRect::EXPAND_FIT_WIDTH_PROPORTIONAL);
	image_display->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	image_display->set_custom_minimum_size(Size2(display_size.x, display_size.y));
	image_container->add_child(image_display);
	
	// Add technical details and controls
	HBoxContainer *details_container = memnew(HBoxContainer);
	image_container->add_child(details_container);
	
	// Size info
	Label *size_label = memnew(Label);
	size_label->set_text(String::num_int64(original_size.x) + "×" + String::num_int64(original_size.y));
	size_label->add_theme_font_size_override("font_size", 10);
	size_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
	details_container->add_child(size_label);
	
	// Model info (for generated images)
	if (!model.is_empty()) {
		Label *model_label = memnew(Label);
		model_label->set_text(" • " + model);
		model_label->add_theme_font_size_override("font_size", 10);
		model_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(1, 1, 1, 0.7));
		details_container->add_child(model_label);
	}
	
	// File path button (for attached images)
	if (!file_path.is_empty() && !is_generated) {
		Button *file_link = memnew(Button);
		file_link->set_text(" • " + file_path.get_file());
		file_link->set_flat(true);
		file_link->add_theme_font_size_override("font_size", 10);
		file_link->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
		file_link->set_tooltip_text("Click to open: " + file_path);
		file_link->connect("pressed", callable_mp(this, &AIChatDock::_on_tool_file_link_pressed).bind(file_path));
		details_container->add_child(file_link);
	}
	
	// Add spacer to push save button to the right
	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	details_container->add_child(spacer);
	
	// Save button
	Button *save_button = memnew(Button);
	save_button->set_text("Save to...");
	save_button->set_flat(true);
	save_button->add_theme_icon_override("icon", get_theme_icon(SNAME("Save"), SNAME("EditorIcons")));
	save_button->add_theme_color_override("font_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	save_button->add_theme_color_override("icon_normal_color", get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	save_button->set_tooltip_text("Save this image to your project");
	save_button->connect("pressed", callable_mp(this, &AIChatDock::_on_save_image_pressed).bind(p_base64_data, "png"));
	details_container->add_child(save_button);
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

void AIChatDock::_on_save_image_location_selected(const String &p_file_path) {
	if (p_file_path.is_empty() || pending_save_image_data.is_empty()) {
		return;
	}
	
	String save_path = p_file_path;
	
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
	
	// Show success notification to user
	if (EditorNode::get_singleton()) {
		EditorNode::get_singleton()->show_warning("Image saved successfully to: " + save_path.get_file());
	}
	
	// Clear pending data
	pending_save_image_data = "";
	pending_save_image_format = "";
}

void AIChatDock::_update_tool_placeholder_status(const String &p_tool_id, const String &p_tool_name, const String &p_status) {
	if (chat_container == nullptr) {
		return;
	}
	
	// Find the placeholder for this tool
	PanelContainer *placeholder = Object::cast_to<PanelContainer>(chat_container->find_child("tool_placeholder_" + p_tool_id, true, false));
	
	// If no placeholder exists, create one for backend-only tools like image generation
	if (!placeholder && p_status == "starting") {
		// Create a new message bubble for backend-only tools that don't have placeholders
		_create_backend_tool_placeholder(p_tool_id, p_tool_name);
		placeholder = Object::cast_to<PanelContainer>(chat_container->find_child("tool_placeholder_" + p_tool_id, true, false));
	}
	
	if (!placeholder) {
		return;
	}
	
	// Find the label in the placeholder and update its text
	HBoxContainer *tool_hbox = Object::cast_to<HBoxContainer>(placeholder->get_child(0));
	if (tool_hbox) {
		Label *tool_label = Object::cast_to<Label>(tool_hbox->get_child(0));
		if (tool_label) {
			if (p_status == "starting") {
				tool_label->set_text("⚡ Executing: " + p_tool_name + "...");
				tool_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(0.2, 0.8, 1.0, 1.0)); // Blue color for executing
			}
		}
	}
}

void AIChatDock::_create_assistant_message_for_backend_tool(const String &p_tool_name) {
	// Create a proper assistant message bubble that can later receive content
	_add_message_to_chat("assistant", "⚡ Executing: " + p_tool_name + "...");
}

void AIChatDock::_create_assistant_message_with_tool_placeholder(const String &p_tool_name, const String &p_tool_id) {
	// Add to chat history first
	AIChatDock::ChatMessage msg;
	msg.role = "assistant";
	msg.content = "⚡ Calling tool: " + p_tool_name + "...";
	msg.timestamp = _get_timestamp();
	
	Vector<AIChatDock::ChatMessage> &chat_history = _get_current_chat_history();
	chat_history.push_back(msg);
	
	// Create an assistant message bubble
	PanelContainer *message_panel = memnew(PanelContainer);
	
	// Add spacing before the message for cleaner layout
	if (chat_container->get_child_count() > 0) {
		Control *spacer = memnew(Control);
		spacer->set_custom_minimum_size(Size2(0, 8)); // 8px gap between messages
		chat_container->add_child(spacer);
	}
	
	chat_container->add_child(message_panel);
	message_panel->set_visible(true); // Make it visible immediately

	// Assistant message styling
	Ref<StyleBoxFlat> panel_style = memnew(StyleBoxFlat);
	panel_style->set_content_margin_all(12);
	panel_style->set_corner_radius_all(8);
	panel_style->set_bg_color(get_theme_color(SNAME("dark_color_2"), SNAME("Editor")));
	panel_style->set_border_width_all(1);
	panel_style->set_border_color(get_theme_color(SNAME("dark_color_3"), SNAME("Editor")));
	message_panel->add_theme_style_override("panel", panel_style);

	// Content container
	VBoxContainer *message_vbox = memnew(VBoxContainer);
	message_panel->add_child(message_vbox);

	// Role label
	Label *role_label = memnew(Label);
	role_label->add_theme_font_override("font", get_theme_font(SNAME("bold"), SNAME("EditorFonts")));
	role_label->set_text("Assistant");
	role_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")));
	message_vbox->add_child(role_label);

	// Create tool call placeholder
	PanelContainer *placeholder = memnew(PanelContainer);
	placeholder->set_name("tool_placeholder_" + p_tool_id);
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
	tool_label->set_text("⚡ Calling tool: " + p_tool_name + "...");
	tool_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(0.2, 0.8, 1.0, 1.0)); // Blue color for executing
	tool_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Tools"), SNAME("EditorIcons")));
	tool_hbox->add_child(tool_label);

	// Update conversation timestamp and save
	if (current_conversation_index >= 0) {
		conversations.write[current_conversation_index].last_modified_timestamp = _get_timestamp();
		_queue_delayed_save();
	}

	// Auto-scroll to bottom
	call_deferred("_scroll_to_bottom");
}

void AIChatDock::_create_backend_tool_placeholder(const String &p_tool_id, const String &p_tool_name) {
	if (chat_container == nullptr) {
		return;
	}
	
	// Create a new message bubble for backend tools
	PanelContainer *bubble_panel = memnew(PanelContainer);
	bubble_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	
	Ref<StyleBoxFlat> bubble_style = memnew(StyleBoxFlat);
	bubble_style->set_bg_color(get_theme_color(SNAME("base_color"), SNAME("Editor")) * Color(0.9, 1.1, 0.9, 1.0)); // Slightly greenish for assistant
	bubble_style->set_content_margin_all(15);
	bubble_style->set_border_width_all(1);
	bubble_style->set_border_color(get_theme_color(SNAME("dark_color_2"), SNAME("Editor")));
	bubble_style->set_corner_radius_all(10);
	bubble_panel->add_theme_style_override("panel", bubble_style);
	
	VBoxContainer *message_vbox = memnew(VBoxContainer);
	bubble_panel->add_child(message_vbox);
	
	// Create tool placeholder
	PanelContainer *placeholder = memnew(PanelContainer);
	placeholder->set_name("tool_placeholder_" + p_tool_id);
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
	tool_label->set_text("⚡ Executing: " + p_tool_name + "...");
	tool_label->add_theme_color_override("font_color", get_theme_color(SNAME("font_color"), SNAME("Editor")) * Color(0.2, 0.8, 1.0, 1.0));
	tool_label->add_theme_icon_override("icon", get_theme_icon(SNAME("Tools"), SNAME("EditorIcons")));
	tool_hbox->add_child(tool_label);
	
	chat_container->add_child(bubble_panel);
	call_deferred("_scroll_to_bottom");
}

bool AIChatDock::_is_label_descendant_of_node(Node *p_label, Node *p_node) {
	if (p_label == nullptr || p_node == nullptr) {
		return false;
	}
	
	// Direct match
	if (p_label == p_node) {
		return true;
	}
	
	// Check if p_label is a child of p_node
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		if (child == p_label) {
			return true;
		}
		
		// Recursively check descendants
		if (_is_label_descendant_of_node(p_label, child)) {
			return true;
		}
	}
	
	return false;
}

// Attachment helper methods

void AIChatDock::_attach_scene_node(Node *p_node) {
	if (!p_node) {
		return;
	}
	
	// Check if node is already attached
	String node_path_str = String(p_node->get_path());
	for (const AttachedFile &existing_file : current_attached_files) {
		if (existing_file.is_node && existing_file.node_path == p_node->get_path()) {
			return; // Already attached
		}
	}
	
	AIChatDock::AttachedFile attached_file;
	attached_file.path = node_path_str;
	attached_file.name = String(p_node->get_name()) + " (" + String(p_node->get_class()) + ")";
	attached_file.content = _get_node_info_string(p_node);
	attached_file.is_node = true;
	attached_file.node_path = p_node->get_path();
	attached_file.node_type = p_node->get_class();
	attached_file.mime_type = "application/godot-node";
	
	current_attached_files.push_back(attached_file);
	_update_attached_files_display();
}

void AIChatDock::_attach_current_script() {
	ScriptEditor *script_editor = EditorInterface::get_singleton()->get_script_editor();
	if (!script_editor) {
		return;
	}
	
	ScriptTextEditor *text_editor = Object::cast_to<ScriptTextEditor>(script_editor->get_current_editor());
	if (!text_editor) {
		return;
	}
	
	Ref<Script> script = text_editor->get_edited_resource();
	if (script.is_null()) {
		return;
	}
	
	String script_path = script->get_path();
	if (script_path.is_empty()) {
		return;
	}
	
	// Check if script is already attached
	for (const AttachedFile &existing_file : current_attached_files) {
		if (existing_file.path == script_path) {
			return; // Already attached
		}
	}
	
	AIChatDock::AttachedFile attached_file;
	attached_file.path = script_path;
	attached_file.name = script_path.get_file();
	attached_file.content = text_editor->get_code_editor()->get_text_editor()->get_text();
	attached_file.is_image = false;
	attached_file.mime_type = _get_mime_type_from_extension(script_path);
	
	current_attached_files.push_back(attached_file);
	_update_attached_files_display();
}

void AIChatDock::_populate_scene_tree_recursive(Node *p_node, TreeItem *p_parent) {
	if (!p_node || !p_parent) {
		return;
	}
	
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		TreeItem *item = scene_tree->create_item(p_parent);
		item->set_text(0, String(child->get_name()) + " (" + String(child->get_class()) + ")");
		item->set_metadata(0, child->get_path());
		
		// Set appropriate icon based on node type
		if (child->is_class("Node2D")) {
			item->set_icon(0, get_theme_icon(SNAME("Node2D"), SNAME("EditorIcons")));
		} else if (child->is_class("Node3D")) {
			item->set_icon(0, get_theme_icon(SNAME("Node3D"), SNAME("EditorIcons")));
		} else if (child->is_class("Control")) {
			item->set_icon(0, get_theme_icon(SNAME("Control"), SNAME("EditorIcons")));
		} else if (child->is_class("CanvasItem")) {
			item->set_icon(0, get_theme_icon(SNAME("CanvasItem"), SNAME("EditorIcons")));
		} else {
			item->set_icon(0, get_theme_icon(SNAME("Node"), SNAME("EditorIcons")));
		}
		
		// Recursively populate children
		if (child->get_child_count() > 0) {
			_populate_scene_tree_recursive(child, item);
		}
	}
}

String AIChatDock::_get_node_info_string(Node *p_node) {
	if (!p_node) {
		return "";
	}
	
	String info = "Node Information:\n";
	info += "Name: " + p_node->get_name() + "\n";
	info += "Type: " + p_node->get_class() + "\n";
	info += "Path: " + String(p_node->get_path()) + "\n";
	
	// Add position information for spatial nodes
	if (p_node->is_class("Node2D")) {
		Node2D *node2d = Object::cast_to<Node2D>(p_node);
		if (node2d) {
			info += "Position: " + String(node2d->get_position()) + "\n";
			info += "Rotation: " + String::num(node2d->get_rotation()) + "\n";
			info += "Scale: " + String(node2d->get_scale()) + "\n";
		}
	} else if (p_node->is_class("Node3D")) {
		Node3D *node3d = Object::cast_to<Node3D>(p_node);
		if (node3d) {
			info += "Position: " + String(node3d->get_position()) + "\n";
			info += "Rotation: " + String(node3d->get_rotation()) + "\n";
			info += "Scale: " + String(node3d->get_scale()) + "\n";
		}
	}
	
	// Add basic properties
	List<PropertyInfo> properties;
	p_node->get_property_list(&properties);
	
	info += "\nKey Properties:\n";
	int prop_count = 0;
	for (const PropertyInfo &prop : properties) {
		if (prop_count >= 10) break; // Limit to first 10 properties
		if (prop.usage & PROPERTY_USAGE_EDITOR && prop.type != Variant::OBJECT) {
			Variant value = p_node->get(prop.name);
			info += "  " + prop.name + ": " + String(value) + "\n";
			prop_count++;
		}
	}
	
	// Add children count
	if (p_node->get_child_count() > 0) {
		info += "\nChildren: " + String::num(p_node->get_child_count()) + " nodes\n";
	}
	
	return info;
}

String AIChatDock::get_machine_id() const {
	// Generate a unique machine ID based on hardware characteristics
	String machine_id = OS::get_singleton()->get_unique_id();
	if (machine_id.is_empty()) {
		// Fallback to processor name + OS name
		machine_id = OS::get_singleton()->get_processor_name() + "_" + OS::get_singleton()->get_name();
		machine_id = machine_id.replace(" ", "_").replace("(", "").replace(")", "");
	}
	return machine_id;
}

AIChatDock::~AIChatDock() {
	// Wait for any background save to complete
	if (save_thread_busy && save_thread) {
		save_thread->wait_to_finish();
		memdelete(save_thread);
	}
	
	// Clean up mutex
	if (save_mutex) {
		memdelete(save_mutex);
	}
}

