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
#include "scene/gui/box_container.h"
#include "diff_viewer.h"
#include "common.h"

class Button;
class ConfigFile;
class EditorTools;
class HTTPClient;
class Node;
class OptionButton;
class RichTextLabel;
class ScrollContainer;
class StyleBoxFlat;
class TextEdit;
class DiffViewer;

class AIChatDock : public VBoxContainer {
	GDCLASS(AIChatDock, VBoxContainer);

private:
	DiffViewer *diff_viewer;
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

	struct ChatMessage {
		String role; // "user", "assistant", or "tool"
		String content;
		String timestamp;
		// For assistant tool calls.
		Array tool_calls;
		// For tool responses.
		String tool_call_id;
		String name;
	};

	ScrollContainer *chat_scroll = nullptr;
	VBoxContainer *chat_container = nullptr;
	OptionButton *model_dropdown = nullptr;
	TextEdit *input_field = nullptr;
	Button *send_button = nullptr;

	Ref<HTTPClient> http_client;
	RequestStatus http_status = STATUS_IDLE;
	String pending_request_path;
	PackedStringArray pending_request_headers;
	PackedByteArray pending_request_body;
	RichTextLabel *current_assistant_message_label = nullptr;
	String response_buffer;

	Vector<ChatMessage> chat_history;
	String api_key;
	String api_endpoint = "http://localhost:8000/chat";
	String model = "gpt-4o";

	bool is_waiting_for_response = false;

	void _on_send_button_pressed();
	void _on_input_text_changed();
	void _on_input_field_gui_input(const Ref<InputEvent> &p_event);
	void _on_model_selected(int p_index);
	void _on_tool_output_toggled(Control *p_content);
	void _handle_response_chunk(const PackedByteArray &p_chunk);
	void _process_ndjson_line(const String &p_line);
	void _execute_tool_calls(const Array &p_tool_calls);
	RichTextLabel *_get_or_create_current_assistant_message_label();
	void _create_tool_call_bubbles(const Array &p_tool_calls);

	void _add_message_to_chat(const String &p_role, const String &p_content, const Array &p_tool_calls = Array());
	void _add_tool_response_to_chat(const String &p_tool_call_id, const String &p_name, const Dictionary &p_args, const Dictionary &p_result);
	void _send_chat_request();
	void _update_ui_state();
	void _create_message_bubble(const ChatMessage &p_message);
	String _get_timestamp();
	void _scroll_to_bottom();

	void _on_tool_file_link_pressed(const String &p_path);

	void _save_layout_to_config(Ref<ConfigFile> p_layout, const String &p_section) const;
	void _load_layout_from_config(Ref<ConfigFile> p_layout, const String &p_section);

	// Markdown to BBCode conversion
	String _markdown_to_bbcode(const String &p_markdown);
	String _process_inline_markdown(String p_line);

	void _on_diff_accepted(const String &p_path, const String &p_content);

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void clear_chat_history();
	void set_api_key(const String &p_api_key);
	void set_api_endpoint(const String &p_endpoint);
	void set_model(const String &p_model);

	AIChatDock();
}; 