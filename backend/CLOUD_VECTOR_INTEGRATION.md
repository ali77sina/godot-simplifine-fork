# Cloud Vector Search Integration Guide

## Architecture Overview

The system uses Google Cloud Platform services for scalable, cloud-based vector search:

1. **Godot Editor** (User's machine) → Sends files to index
2. **Cloud Run** (Flask backend) → Processes requests  
3. **Vertex AI** → Generates state-of-the-art embeddings
4. **BigQuery** → Stores and searches vectors with native support

## Key Features

- **User Isolation**: Each user's projects are isolated by `user_id`
- **Project Separation**: Multiple projects tracked by `project_id`
- **SOTA Embeddings**: Uses Vertex AI's `textembedding-gecko@003` model
- **Scalable Storage**: BigQuery handles billions of vectors efficiently
- **Fast Search**: Native vector similarity search in BigQuery

## Adding an Index Button to Godot

### 1. Add UI Button in `ai_chat_dock.cpp`

```cpp
// In _ready() or UI setup function:
Button *index_button = memnew(Button);
index_button->set_text("Index Project");
index_button->set_tooltip_text("Index all project files for AI search");
index_button->connect("pressed", callable_mp(this, &AIChatDock::_on_index_button_pressed));
toolbar->add_child(index_button);
```

### 2. Add Handler Function

```cpp
void AIChatDock::_on_index_button_pressed() {
    if (!_is_user_authenticated()) {
        _show_message("Please log in first to use AI features", MESSAGE_TYPE_ERROR);
        return;
    }
    
    // Show progress
    _set_embedding_status("Indexing project...", true);
    
    // Send index request
    Dictionary payload;
    payload["action"] = "index_project";
    payload["project_root"] = _get_project_root_path();
    payload["force_reindex"] = false;  // Or add checkbox for force reindex
    
    _send_embedding_request("/embed", payload);
}
```

### 3. The Request Flow

When the button is clicked:

1. **Frontend sends**:
```json
POST https://gamechat.simplifine.com/embed
{
    "action": "index_project",
    "project_root": "/path/to/godot/project",
    "force_reindex": false
}
```

2. **Backend processes**:
- Walks through project directory
- Filters files (skips binaries, large files)
- Chunks text intelligently (by functions, sections)
- Generates embeddings via Vertex AI
- Stores in BigQuery with user isolation

3. **Response**:
```json
{
    "success": true,
    "action": "index_project",
    "stats": {
        "total": 150,
        "indexed": 45,
        "skipped": 105
    },
    "project_id": "abc123..."
}
```

## Search Integration

The existing search functionality automatically uses the indexed data:

```cpp
// When user types in AI chat
void AIChatDock::_on_user_query(const String &query) {
    // AI can now search project files
    // The search_across_project tool will query BigQuery
}
```

## Authentication Flow

The system requires authentication to ensure user isolation:

1. User logs in via OAuth (Google/GitHub)
2. Backend tracks `user_id` from OAuth
3. All embeddings are stored with `user_id` + `project_id`
4. Searches only return results for authenticated user

## Performance Optimizations

1. **Incremental Updates**: Only changed files are re-indexed
2. **Smart Chunking**: Code files chunked by functions/classes
3. **Batch Processing**: Embeddings generated in batches
4. **Caching**: Project ID cached to avoid rehashing

## Security

- User data isolated by OAuth identity
- No cross-user data access possible
- Project paths stored as relative paths
- Content truncated to reasonable sizes

## Monitoring

Check indexing status:
```cpp
void AIChatDock::_check_index_status() {
    Dictionary payload;
    payload["action"] = "status";
    payload["project_root"] = _get_project_root_path();
    
    _send_embedding_request("/embed", payload);
}
```

Response:
```json
{
    "success": true,
    "stats": {
        "files_indexed": 45,
        "total_chunks": 234,
        "last_indexed": 1703001234.5,
        "storage": "BigQuery",
        "embedding_model": "textembedding-gecko@003"
    }
}
```

## Cost Considerations

- **Vertex AI**: ~$0.00002 per 1K characters embedded
- **BigQuery Storage**: ~$0.02 per GB per month
- **BigQuery Queries**: ~$5 per TB scanned

For a typical Godot project:
- Initial indexing: ~$0.10-$0.50
- Storage: ~$0.01/month
- Searches: Negligible

## Future Enhancements

1. **Real-time Updates**: Watch filesystem for automatic reindexing
2. **Partial Indexing**: Index only changed files via file watchers
3. **Custom Embeddings**: Support for game-specific embeddings
4. **Shared Indexes**: Team collaboration features
5. **Version Control**: Track embeddings across git commits
