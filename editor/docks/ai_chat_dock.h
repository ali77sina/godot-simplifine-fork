/**************************************************************************/
/*  ai_chat_dock.h                                                        */
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

#pragma once

#include "core/io/http_client.h"
#include "core/io/image.h"
#include "scene/gui/box_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/dialogs.h"
#include "diff_viewer.h"
#include "ai_tool_server.h"
#include "common.h"

class Button;
class ConfigFile;
class EditorFileDialog;
class EditorTools;
class HTTPClient;
class Node;
class OptionButton;
class RichTextLabel;
class ScrollContainer;
class StyleBoxFlat;
class TextEdit;
class DiffViewer;
class ScriptEditor;
class ScriptTextEditor;
class HFlowContainer;

class AIChatDock : public VBoxContainer {
	GDCLASS(AIChatDock, VBoxContainer);

private:
	DiffViewer *diff_viewer;
	Ref<AIToolServer> tool_server;
	// Helper to find RichTextLabel recursively.
	static RichTextLabel *find_rich_text_label_in_children(Node *p_node) {
		if (!p_node) {
			return nullptr;
		}
		RichTextLabel *rt = Object::cast_to<RichTextLabel>(p_node);
		if (rt) {
			return rt;
		}
		for (int i = 0; i < p_node->get_child_count(); i++) {
			RichTextLabel *rt = find_rich_text_label_in_children(p_node->get_child(i));
			if (rt) {
				return rt;
			}
		}
		return nullptr;
	}

	enum RequestStatus {
		STATUS_IDLE,
		STATUS_CONNECTING,
		STATUS_REQUESTING,
		STATUS_BODY,
		STATUS_DONE,
	};

	struct AttachedFile {
		String path;
		String name;
		String content;
		bool is_image = false;
		String mime_type;
		String base64_data; // For images encoded for API
		Vector2i original_size = Vector2i(0, 0);
		Vector2i display_size = Vector2i(0, 0);
		bool was_downsampled = false;
		// Node support
		bool is_node = false;
		NodePath node_path;
		String node_type;
	};

	struct ChatMessage {
		String role; // "user", "assistant", or "tool"
		String content;
		String timestamp;
		// For assistant tool calls.
		Array tool_calls;
		// For tool responses.
		String tool_call_id;
		String name;
		// For attached files
		Vector<AttachedFile> attached_files;
		// For storing tool execution results (like generated images)
		Array tool_results;
	};

	struct Conversation {
		String id;
		String title;
		String created_timestamp;
		String last_modified_timestamp;
		Vector<ChatMessage> messages;
	};

	ScrollContainer *chat_scroll = nullptr;
	VBoxContainer *chat_container = nullptr;
	OptionButton *model_dropdown = nullptr;
	OptionButton *conversation_history_dropdown = nullptr;
	Button *new_conversation_button = nullptr;
	TextEdit *input_field = nullptr;
	Button *send_button = nullptr;
	Button *attach_button = nullptr;
	HFlowContainer *attached_files_container = nullptr;
	EditorFileDialog *file_dialog = nullptr;
	EditorFileDialog *save_image_dialog = nullptr;
	AcceptDialog *image_warning_dialog = nullptr;
	
	// For saving images
	String pending_save_image_data; // Base64 image data to save
	String pending_save_image_format; // "png" or "jpg"

	Ref<HTTPClient> http_client;
	RequestStatus http_status = STATUS_IDLE;
	String pending_request_path;
	PackedStringArray pending_request_headers;
	PackedByteArray pending_request_body;
	RichTextLabel *current_assistant_message_label = nullptr;
	String response_buffer;

	Vector<Conversation> conversations;
	int current_conversation_index = -1;
	Vector<AttachedFile> current_attached_files;
	String conversations_file_path;
	String api_key;
	String api_endpoint = "http://localhost:8000/chat";
	String model = "gpt-4o";

	bool is_waiting_for_response = false;

	void _on_send_button_pressed();
	void _on_input_text_changed();
	void _on_input_field_gui_input(const Ref<InputEvent> &p_event);
	void _on_model_selected(int p_index);
	void _on_tool_output_toggled(Control *p_content);
	void _on_attach_button_pressed();
	void _on_files_selected(const Vector<String> &p_files);
	void _on_remove_attachment(const String &p_path);
	void _on_conversation_selected(int p_index);
	void _on_new_conversation_pressed();
	void _on_save_image_pressed(const String &p_base64_data, const String &p_format);
	void _on_save_image_location_selected(const Vector<String> &p_files);
	void _handle_response_chunk(const PackedByteArray &p_chunk);
	void _process_ndjson_line(const String &p_line);
	void _execute_tool_calls(const Array &p_tool_calls);
	RichTextLabel *_get_or_create_current_assistant_message_label();
	void _create_tool_call_bubbles(const Array &p_tool_calls);

	void _add_message_to_chat(const String &p_role, const String &p_content, const Array &p_tool_calls = Array());
	void _add_tool_response_to_chat(const String &p_tool_call_id, const String &p_name, const Dictionary &p_args, const Dictionary &p_result);
	void _send_chat_request();
	void _update_ui_state();
	void _create_message_bubble(const AIChatDock::ChatMessage &p_message);
	void _update_attached_files_display();
	void _clear_attachments();
	String _get_timestamp();
	void _scroll_to_bottom();
	
	// Conversation management
	void _load_conversations();
	void _save_conversations();
	void _create_new_conversation();
	void _switch_to_conversation(int p_index);
	void _update_conversation_dropdown();
	String _generate_conversation_id();
	String _generate_conversation_title(const Vector<AIChatDock::ChatMessage> &p_messages);
	Vector<AIChatDock::ChatMessage> &_get_current_chat_history();

	void _on_tool_file_link_pressed(const String &p_path);

	void _save_layout_to_config(Ref<ConfigFile> p_layout, const String &p_section) const;
	void _load_layout_from_config(Ref<ConfigFile> p_layout, const String &p_section);

	// Markdown to BBCode conversion
	String _markdown_to_bbcode(const String &p_markdown);
	String _process_inline_markdown(String p_line);

	void _on_diff_accepted(const String &p_path, const String &p_content);

	// Image processing methods
	bool _is_image_file(const String &p_path);
	String _get_mime_type_from_extension(const String &p_path);
	bool _process_image_attachment(AttachedFile &p_file);
	Vector2i _calculate_downsampled_size(const Vector2i &p_original, int p_max_dimension = 1024);
	void _show_image_warning_dialog(const String &p_filename, const Vector2i &p_original, const Vector2i &p_new_size);
	
	// Image generation handling
	void _handle_generated_image(const String &p_base64_data, const String &p_id);
	void _display_generated_image_deferred(const String &p_base64_data, const String &p_id);
	void _display_generated_image_in_tool_result(VBoxContainer *p_container, const String &p_base64_data, const Dictionary &p_data);
	
	// Drag and drop support
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
	bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
	void drop_data(const Point2 &p_point, const Variant &p_data) override;
	void _attach_dragged_files(const Vector<String> &p_files);
	void _attach_external_files(const Vector<String> &p_files);
	void _attach_dragged_nodes(const Array &p_nodes);
	String _get_file_type_icon(const AttachedFile &p_file);

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void clear_chat_history();
	void clear_current_conversation();
	void set_api_key(const String &p_api_key);
	void set_api_endpoint(const String &p_endpoint);
	void set_model(const String &p_model);

	AIChatDock();
}; 