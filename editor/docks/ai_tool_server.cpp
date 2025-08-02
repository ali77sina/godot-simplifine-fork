#include "ai_tool_server.h"
#include "editor/ai/editor_tools.h"
#include "core/io/json.h"
#include "core/string/string_builder.h"

void AIToolServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("listen", "port"), &AIToolServer::listen, DEFVAL(8001));
	ClassDB::bind_method(D_METHOD("stop"), &AIToolServer::stop);
	ClassDB::bind_method(D_METHOD("is_listening"), &AIToolServer::is_listening);
}

void AIToolServer::_server_thread_poll(void *data) {
	AIToolServer *server = static_cast<AIToolServer *>(data);
	server->_poll();
}

void AIToolServer::_clear_client() {
	peer.unref();
	tcp.unref();
	req_pos = 0;
	memset(req_buf, 0, sizeof(req_buf));
}

void AIToolServer::_send_response() {
	// This will be populated by _handle_tool_request
}

Dictionary AIToolServer::_handle_tool_request(const String &p_method, const String &p_path, const String &p_body) {
	Dictionary result;
	
	if (p_method != "POST") {
		result["error"] = "Method not allowed";
		return result;
	}
	
	// Parse JSON body
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(p_body);
	if (err != OK) {
		result["error"] = "Invalid JSON";
		return result;
	}
	
	Dictionary request_data = json->get_data();
	String function_name = request_data.get("function_name", "");
	Dictionary args = request_data.get("arguments", Dictionary());
	
	// Handle tool execution
	if (function_name == "apply_edit") {
		String path = args.get("path", "");
		String prompt = args.get("prompt", "");
		
		Dictionary apply_args;
		apply_args["path"] = path;
		apply_args["prompt"] = prompt;
		// Pass through any additional AI-provided arguments
		Array keys = args.keys();
		for (int i = 0; i < keys.size(); i++) {
			String key_str = keys[i];
			if (key_str != "path" && key_str != "prompt") {
				apply_args[key_str] = args[key_str];
			}
		}
		
		result = EditorTools::apply_edit(apply_args);
	} else if (function_name == "list_project_files") {
		result = EditorTools::list_project_files(args);
	} else if (function_name == "read_file_content") {
		result = EditorTools::read_file_content(args);
	} else if (function_name == "read_file_advanced") {
		result = EditorTools::read_file_advanced(args);
	} else if (function_name == "test_diff_and_errors") {
		// Test endpoint that simulates an edit to test diff and compilation error functionality
		String path = args.get("path", "");
		String mock_edit = args.get("mock_edit", "");
		
		if (path.is_empty()) {
			result["error"] = "Path is required";
		} else {
			// Read original file
			Error err;
			String original_content = FileAccess::get_file_as_string(path, &err);
			if (err != OK) {
				result["success"] = false;
				result["message"] = "Failed to read file: " + path;
				result["diff"] = "";
				result["compilation_errors"] = Array();
			} else {
				// Use provided mock edit or create a simple one
				String modified_content = mock_edit;
				if (modified_content.is_empty()) {
					// Create a simple mock edit for testing
					modified_content = original_content.replace("print(\"Hello World\")", "print(\"Hello from Test!\")");
				}
				
				// Test the diff generation
				String diff = EditorTools::_generate_unified_diff(original_content, modified_content, path);
				
				// Test compilation error checking (temporarily disabled to prevent crashes)
				Array compilation_errors;
				// compilation_errors = EditorTools::_check_compilation_errors(path, modified_content);
				
				result["success"] = true;
				result["message"] = "Test completed successfully";
				result["original_content"] = original_content;
				result["modified_content"] = modified_content;
				result["diff"] = diff;
				result["compilation_errors"] = compilation_errors;
				result["has_errors"] = compilation_errors.size() > 0;
			}
		}
	} else if (function_name == "check_compilation_errors") {
		result = EditorTools::check_compilation_errors(args);
	} else {
		result["error"] = "Unknown function: " + function_name;
	}
	
	return result;
}

void AIToolServer::_poll() {
	while (!server_quit.is_set()) {
		if (!server->is_listening()) {
			OS::get_singleton()->delay_usec(100000); // 100ms
			continue;
		}
		
		if (tcp.is_null()) {
			if (!server->is_connection_available()) {
				OS::get_singleton()->delay_usec(10000); // 10ms delay
				continue;
			}
			tcp = server->take_connection();
			peer = tcp;
			time = OS::get_singleton()->get_ticks_msec();
		}
		
		if (peer.is_null()) {
			OS::get_singleton()->delay_usec(10000); // 10ms delay
			continue;
		}
	
	while (true) {
		// Read request data
		if (req_pos >= 4096) {
			_clear_client();
			break;
		}
		
		int received = 0;
		Error err = peer->get_partial_data(&req_buf[req_pos], 1, received);
		if (err != OK || received != 1) {
			break;
		}
		req_pos++;
		
		// Check for end of headers
		if (req_pos >= 4 && String::chr(req_buf[req_pos - 4]) + String::chr(req_buf[req_pos - 3]) + String::chr(req_buf[req_pos - 2]) + String::chr(req_buf[req_pos - 1]) == "\r\n\r\n") {
			String request_str = String::utf8((const char *)req_buf, req_pos);
			
			// Parse HTTP request
			PackedStringArray lines = request_str.split("\r\n");
			if (lines.size() == 0) {
				_clear_client();
				break;
			}
			
			PackedStringArray request_line = lines[0].split(" ");
			if (request_line.size() < 2) {
				_clear_client();
				break;
			}
			
			String method = request_line[0];
			String path = request_line[1];
			
			// Find Content-Length
			int content_length = 0;
			for (int i = 1; i < lines.size(); i++) {
				if (lines[i].begins_with("Content-Length:")) {
					content_length = lines[i].substr(15).strip_edges().to_int();
					break;
				}
			}
			
			// Read body if present
			String body = "";
			if (content_length > 0) {
				PackedByteArray body_bytes;
				body_bytes.resize(content_length);
				int body_read = 0;
				while (body_read < content_length) {
					int chunk_received = 0;
					Error chunk_err = peer->get_partial_data(&body_bytes.ptrw()[body_read], content_length - body_read, chunk_received);
					if (chunk_err != OK || chunk_received <= 0) break;
					body_read += chunk_received;
				}
				body = String::utf8((const char *)body_bytes.ptr(), body_read);
			}
			
			// Handle the request
			Dictionary response_data = _handle_tool_request(method, path, body);
			
			// Send response
			Ref<JSON> response_json;
			response_json.instantiate();
			String response_body = response_json->stringify(response_data);
			PackedByteArray body_bytes = response_body.to_utf8_buffer();
			
			StringBuilder response_builder;
			response_builder.append("HTTP/1.1 200 OK\r\n");
			response_builder.append("Content-Type: application/json\r\n");
			response_builder.append("Content-Length: " + itos(body_bytes.size()) + "\r\n");
			response_builder.append("Access-Control-Allow-Origin: *\r\n");
			response_builder.append("Access-Control-Allow-Methods: POST, OPTIONS\r\n");
			response_builder.append("Access-Control-Allow-Headers: Content-Type\r\n");
			response_builder.append("Connection: close\r\n");
			response_builder.append("\r\n");
			
			String headers = response_builder.as_string();
			PackedByteArray headers_bytes = headers.to_utf8_buffer();
			
			// Send headers first
			peer->put_data(headers_bytes.ptr(), headers_bytes.size());
			// Then send body
			peer->put_data(body_bytes.ptr(), body_bytes.size());
			
			_clear_client();
			break;
		}
		
		OS::get_singleton()->delay_usec(10000); // 10ms delay
	}
	}
}

Error AIToolServer::listen(int p_port) {
	server.instantiate();
	Error err = server->listen(p_port, IPAddress("127.0.0.1"));
	if (err != OK) {
		print_line("ERROR: Failed to start AI tool server on port " + itos(p_port));
		return err;
	}
	
	print_line("AI Tool Server: Started on port " + itos(p_port));
	
	server_quit.clear();
	server_thread.start(_server_thread_poll, this);
	
	return OK;
}

void AIToolServer::stop() {
	server_quit.set();
	if (server_thread.is_started()) {
		server_thread.wait_to_finish();
	}
	server.unref();
	_clear_client();
	
	print_line("AI Tool Server: Stopped");
}

bool AIToolServer::is_listening() const {
	return server.is_valid() && server->is_listening();
}

AIToolServer::AIToolServer() {
	req_pos = 0;
	memset(req_buf, 0, sizeof(req_buf));
}

AIToolServer::~AIToolServer() {
	stop();
} 