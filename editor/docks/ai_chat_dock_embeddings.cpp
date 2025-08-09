/**************************************************************************/
/*  ai_chat_dock_embeddings.cpp                                           */
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

#include "core/config/project_settings.h"
#include "core/templates/hash_set.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/gui/text_edit.h"

// Embedding System Implementation moved from ai_chat_dock.cpp

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
    data["force_reindex"] = true; // Force complete reindex on login for fresh start

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
        "gd", "cs", "js",                   // Scripts
        "tscn", "scn",                     // Scenes
        "tres", "res",                     // Resources
        "json", "cfg",                     // Config files
        "md", "txt",                       // Documentation
        "glsl", "shader",                  // Shaders

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


