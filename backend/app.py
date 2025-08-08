from flask import Flask, request, Response, jsonify, redirect, session
import openai
import json
import os
from dotenv import load_dotenv
import base64
from PIL import Image
import io
import requests
from threading import Lock
import uuid
import time
import tempfile
from multimodal_embedding_manager import MultimodalEmbeddingManager
from auth_manager import AuthManager

# Load environment variables from .env file
load_dotenv()

# --- Global State & Configuration ---


# Stop mechanism for streaming requests
stop_requests_lock = Lock()
ACTIVE_REQUESTS = {}  # request_id -> {"stop": False, "timestamp": time.time()}

def cleanup_old_requests():
    """Clean up requests older than 5 minutes to prevent memory leaks"""
    current_time = time.time()
    with stop_requests_lock:
        to_remove = []
        for req_id, data in ACTIVE_REQUESTS.items():
            if current_time - data["timestamp"] > 300:  # 5 minutes
                to_remove.append(req_id)
        for req_id in to_remove:
            del ACTIVE_REQUESTS[req_id]

app = Flask(__name__)
app.secret_key = os.getenv('FLASK_SECRET_KEY', os.urandom(24))

# Get OpenAI API key from environment variable
api_key = os.getenv('OPENAI_API_KEY')
if not api_key:
    raise ValueError("OPENAI_API_KEY environment variable is required")

client = openai.OpenAI(api_key=api_key)

# Model configuration
MODEL = "gpt-4o"  # Using GPT-4o which supports function calling

# Initialize Authentication Manager
auth_manager = AuthManager()

# Initialize Multimodal Embedding Manager
GCP_BUCKET_NAME = os.getenv('GCP_BUCKET_NAME', 'godot-ai-embeddings')
GCP_PROJECT_ID = os.getenv('GCP_PROJECT_ID')
if not GCP_PROJECT_ID:
    raise ValueError("GCP_PROJECT_ID environment variable is required")

# User-specific embedding managers (cached)
embedding_managers = {}

def get_embedding_manager(user_id: str, project_root: str):
    """Get or create user-specific embedding manager instance"""
    cache_key = f"{user_id}:{project_root}"
    if cache_key not in embedding_managers:
        embedding_managers[cache_key] = MultimodalEmbeddingManager(
            client, GCP_BUCKET_NAME, project_root, user_id, GCP_PROJECT_ID
        )
    return embedding_managers[cache_key]

def verify_authentication():
    """Verify user authentication from request (with dev mode bypass)"""
    # DEV MODE: Allow bypass for testing
    if os.getenv('DEV_MODE', 'false').lower() == 'true':
        # Check headers first, then JSON
        user_id = request.headers.get('X-User-ID')
        if not user_id:
            data = request.json if request.json else {}
            user_id = data.get('user_id')
        if user_id:
            print(f"🧪 DEV MODE: Bypassing auth for user {user_id}")
            return {
                "id": user_id,
                "name": "Test User",
                "email": "test@example.com",
                "provider": "dev_mode"
            }, None, None
    
    auth_header = request.headers.get('Authorization', '')
    machine_id = request.headers.get('X-Machine-ID') or (request.json.get('machine_id') if request.json else None)
    
    if not machine_id:
        return None, {"error": "machine_id required", "success": False}, 401
    
    if auth_header.startswith('Bearer '):
        token = auth_header[7:]
        user = auth_manager.verify_session(machine_id, token)
        if user:
            return user, None, None
    
    return None, {"error": "Authentication required", "success": False}, 401

# Image handling will use OpenAI's native ID system - no local registry needed

# --- Helper Functions ---

# --- Asset Processing Function ---
def process_asset_internal(arguments: dict) -> dict:
    """Process assets using various AI and image processing techniques"""
    try:
        operation = arguments.get('operation', '')
        input_path = arguments.get('input_path', '')
        
        if not operation:
            return {"success": False, "error": "No operation specified"}
        
        # For now, return a placeholder for asset processing operations
        # This would be expanded to include actual asset processing logic
        operations = {
            'remove_background': 'Background removal using AI',
            'auto_crop': 'Intelligent sprite boundary detection',
            'generate_spritesheet': 'Automatic sprite sheet generation',
            'style_transfer': 'Apply consistent art style',
            'batch_process': 'Process multiple assets',
            'classify': 'Classify asset types',
            'create_variants': 'Generate asset variations'
        }
        
        if operation not in operations:
            return {"success": False, "error": f"Unknown operation: {operation}"}
        
        return {
            "success": True,
            "message": f"Asset processing '{operation}' completed",
            "operation": operation,
            "description": operations[operation],
            "input_path": input_path
        }
            
    except Exception as e:
        return {"success": False, "error": f"Asset processing failed: {str(e)}"}

# --- Dynamic Image Operation Function ---
def image_operation_internal(arguments: dict, conversation_messages: list = None) -> dict:
    """Dynamic image generation or editing using OpenAI Images API.

    Behavior:
    - If no image IDs provided: generate a new image from prompt.
    - If one or more image IDs provided: edit the first matching image using the prompt.
    The conversation can carry prior images with unique `name` and base64 data which
    the model can reference via the `images` array in `arguments`.
    """

    try:
        description = arguments.get('description', '')
        style = arguments.get('style', '')
        image_ids = arguments.get('images', []) or []
        size = arguments.get('size', '1024x1024')  # optional

        if not description:
            return {"success": False, "error": "No description provided for image operation"}

        prompt_text = description
        if style:
            prompt_text += f", {style} style"

        # Gather available images from prior conversation messages
        available_images = {}
        if conversation_messages:
            for msg in conversation_messages:
                if isinstance(msg, dict) and 'images' in msg:
                    for img in msg['images']:
                        if img.get('base64_data') and img.get('name'):
                            available_images[img['name']] = img

        selected_images = []
        for img_id in image_ids:
            if img_id in available_images:
                selected_images.append(available_images[img_id])
                print(f"IMAGE_OP: Selected input image '{img_id}'")
            else:
                print(f"IMAGE_OP: Warning - requested image '{img_id}' not found in conversation context")

        # If no images selected, do text-to-image generation
        if not selected_images:
            print("IMAGE_OP: Generating new image from prompt using Images API")
            gen = client.images.generate(model="gpt-image-1", prompt=prompt_text, size=size)
            if not gen.data or not getattr(gen.data[0], 'b64_json', None):
                return {"success": False, "error": "Image generation returned no data"}

            image_base64 = gen.data[0].b64_json
            return {
                "success": True,
                "image_data": image_base64,
                "prompt": description,
                "style": style,
                "format": "png",
                "input_images": 0,
                "requested_images": len(image_ids)
            }

        # If images provided, do an edit on the first one
        print("IMAGE_OP: Performing edit on provided image using Images API")
        first_img = selected_images[0]
        try:
            img_bytes = base64.b64decode(first_img['base64_data'])
        except Exception as decode_err:
            return {"success": False, "error": f"Failed to decode input image '{first_img.get('name','unknown')}': {decode_err}"}

        # Re-encode to PNG to ensure a valid image mimetype and structure
        try:
            pil_image = Image.open(io.BytesIO(img_bytes))
        except Exception as pil_err:
            return {"success": False, "error": f"Failed to load input image: {pil_err}"}

        temp_path = None
        try:
            with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
                temp_path = tmp.name
                pil_image.save(tmp, format="PNG")

            with open(temp_path, "rb") as img_fh:
                # Prefer images.edits if available in SDK; otherwise fall back to images.edit
                images_api = getattr(client, 'images')
                if hasattr(images_api, 'edits'):
                    edit = images_api.edits(model="gpt-image-1", image=img_fh, prompt=prompt_text, size=size)
                else:
                    # Older SDKs
                    edit = images_api.edit(model="gpt-image-1", image=img_fh, prompt=prompt_text, size=size)
        finally:
            if temp_path and os.path.exists(temp_path):
                try:
                    os.unlink(temp_path)
                except Exception:
                    pass

        if not edit.data or not getattr(edit.data[0], 'b64_json', None):
            return {"success": False, "error": "Image edit returned no data"}

        image_base64 = edit.data[0].b64_json
        return {
            "success": True,
            "image_data": image_base64,
            "prompt": description,
            "style": style,
            "format": "png",
            "input_images": 1,
            "requested_images": len(image_ids)
        }

    except Exception as e:
        print(f"IMAGE_OP ERROR: {str(e)}")
        return {"success": False, "error": f"Image operation failed: {str(e)}"}

# --- Search Across Project Function ---
def search_across_project_internal(arguments: dict, current_user: dict = None) -> dict:
    """Execute search across project using the embedding system"""
    try:
        query = arguments.get('query', '')
        if not query:
            return {"success": False, "error": "Query parameter is required"}
        
        # Get parameters
        include_graph = arguments.get('include_graph', True)
        max_results = arguments.get('max_results', 5)
        modality_filter = arguments.get('modality_filter')
        
        # Get authentication from current request context or use provided user
        if current_user is None:
            user, error_response, status_code = verify_authentication()
            if error_response:
                return {"success": False, "error": "Authentication required"}
        else:
            user = current_user
        
        # Get project root from arguments or use a default
        project_root = arguments.get('project_root', '/Users/alikavoosi/Desktop/3d-design/game-from-scratch-project/new-game-project')
        
        # Get user-specific embedding manager
        em = get_embedding_manager(user['id'], project_root)
        
        if include_graph:
            # Enhanced search with graph context
            results = em.search_with_graph_context(query, max_results, include_connected=True)
            
            # Format for tool response
            formatted_results = {
                "similar_files": [
                    {
                        "file_path": path,
                        "similarity": float(score),
                        "modality": modality,
                        "connections": results['connected_files'].get(path, {})
                    }
                    for path, score, modality in results['similarity_results']
                ],
                "central_files": [
                    {
                        "file_path": path,
                        "centrality": float(score)
                    }
                    for path, score in results['central_files']
                ],
                "graph_summary": results['graph_summary']
            }
        else:
            # Regular similarity search
            similarity_results = em.search_similar_files(query, max_results, modality_filter)
            formatted_results = {
                "similar_files": [
                    {
                        "file_path": path,
                        "similarity": float(score),
                        "modality": modality
                    }
                    for path, score, modality in similarity_results
                ],
                "central_files": [],
                "graph_summary": {}
            }
        
        return {
            "success": True,
            "query": query,
            "results": formatted_results,
            "include_graph": include_graph,
            "file_count": len(formatted_results["similar_files"]),
            "message": f"Found {len(formatted_results['similar_files'])} relevant files for query: {query}"
        }
        
    except Exception as e:
        print(f"SEARCH_PROJECT_INTERNAL_ERROR: {e}")
        return {"success": False, "error": f"Search failed: {str(e)}"}

# --- Note: Script generation now handled by dedicated /generate_script endpoint ---

# --- Tool Execution Function ---
def execute_godot_tool(function_name: str, arguments: dict) -> dict:
    """Execute backend-specific tools"""
    if function_name == "image_operation":
        return image_operation_internal(arguments)
    elif function_name == "asset_processor":
        return process_asset_internal(arguments)
    elif function_name == "search_across_project":
        return search_across_project_internal(arguments)
    else:
        # This shouldn't happen if we filter correctly
        print(f"WARNING: Unknown backend tool called: {function_name}")

    return {"success": False, "error": f"Unknown backend tool called: {function_name}"}

# --- Individual Tool Definitions (Original 22 Tools) ---
godot_tools = [
    {
        "type": "function",
        "function": {
            "name": "get_scene_info",
            "description": "Get information about the current scene including root node and structure",
            "parameters": {
                "type": "object",
                "properties": {},
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_all_nodes",
            "description": "Get all nodes in the current scene with their information",
            "parameters": {
                "type": "object",
                "properties": {},
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "search_nodes_by_type",
            "description": "Search for nodes by their type (e.g., 'Node2D', 'Button', 'CharacterBody2D')",
            "parameters": {
                "type": "object",
                "properties": {
                    "type": {
                        "type": "string",
                        "description": "The node type to search for"
                    }
                },
                "required": ["type"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_editor_selection",
            "description": "Get currently selected nodes in the editor",
            "parameters": {
                "type": "object",
                "properties": {},
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_node_properties",
            "description": "Get properties of a specific node",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Path to the node"
                    }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "save_scene",
            "description": "Save the current scene",
            "parameters": {
                "type": "object",
                "properties": {},
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "create_node",
            "description": "Create a new node in the scene",
            "parameters": {
                "type": "object",
                "properties": {
                    "type": {
                        "type": "string",
                        "description": "Type of node to create (e.g., 'Node2D', 'Button', 'CharacterBody2D')"
                    },
                    "name": {
                        "type": "string",
                        "description": "Name for the new node"
                    },
                    "parent": {
                        "type": "string",
                        "description": "Parent node path (optional)"
                    }
                },
                "required": ["type", "name"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "delete_node",
            "description": "Delete a node from the scene",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Path to the node to delete"
                    }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "set_node_property",
            "description": "Set a property on a node",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Path to the node"
                    },
                    "property": {
                        "type": "string",
                        "description": "Property name to set"
                    },
                    "value": {
                        "description": "Value to set for the property"
                    }
                },
                "required": ["path", "property", "value"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "move_node",
            "description": "Move a node to a different parent",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Path to the node to move"
                    },
                    "new_parent": {
                        "type": "string",
                        "description": "Path to the new parent node"
                    }
                },
                "required": ["path", "new_parent"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "call_node_method",
            "description": "Call a method on a node",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Path to the node"
                    },
                    "method": {
                        "type": "string",
                        "description": "Method name to call"
                    },
                    "method_args": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "Arguments for the method call"
                    }
                },
                "required": ["path", "method"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_available_classes",
            "description": "Get list of all available node classes in Godot",
            "parameters": {
                "type": "object",
                "properties": {},
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_node_script",
            "description": "Get the script attached to a node",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Path to the node"
                    }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "attach_script",
            "description": "Attach a script to a node",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Path to the node"
                    },
                    "script_path": {
                        "type": "string",
                        "description": "Path to the script file"
                    }
                },
                "required": ["path", "script_path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "manage_scene",
            "description": "Manage scene operations (open, create, save, instantiate)",
            "parameters": {
                "type": "object",
                "properties": {
                    "operation": {
                        "type": "string",
                        "enum": ["open", "create_new", "save_as", "instantiate"],
                        "description": "Scene operation to perform"
                    },
                    "path": {
                        "type": "string",
                        "description": "Scene file path"
                    },
                    "parent_node": {
                        "type": "string",
                        "description": "Parent node for instantiate operations"
                    }
                },
                "required": ["operation"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "add_collision_shape",
            "description": "Add a collision shape to a physics body node",
            "parameters": {
                "type": "object",
                "properties": {
                    "node_path": {
                        "type": "string",
                        "description": "Path to the physics body node"
                    },
                    "shape_type": {
                        "type": "string",
                        "enum": ["rectangle", "circle", "capsule"],
                        "description": "Type of collision shape to create"
                    }
                },
                "required": ["node_path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "list_project_files",
            "description": "List files and directories in the current directory (like ls/dir command). Shows immediate contents only unless recursive=true is explicitly needed.",
            "parameters": {
                "type": "object",
                "properties": {
                    "dir": {
                        "type": "string",
                        "description": "Directory to list (default: res:// root). Use this to navigate into subdirectories like 'res://scripts' or 'res://assets'."
                    },
                    "filter": {
                        "type": "string",
                        "description": "File filter (e.g., '*.gd', '*.tscn')"
                    },
                    "recursive": {
                        "type": "boolean",
                        "description": "If true, list ALL files in the entire project tree (use sparingly, only when you need a complete overview). Default false shows only current directory contents.",
                        "default": False
                    },
                    "full_paths": {
                        "type": "boolean",
                        "description": "If true, return full paths (recommended for navigation)",
                        "default": True
                    }
                },
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "search_project_files",
            "description": "Search for files and directories matching a pattern across the entire project",
            "parameters": {
                "type": "object",
                "properties": {
                    "pattern": {
                        "type": "string",
                        "description": "Search pattern (supports wildcards like '*.gd' or partial names like 'player')"
                    },
                    "dir": {
                        "type": "string",
                        "description": "Base directory to search from (default: res://)"
                    },
                    "case_sensitive": {
                        "type": "boolean",
                        "description": "Whether search should be case sensitive",
                        "default": False
                    }
                },
                "required": ["pattern"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "read_file_content",
            "description": "Read the contents of a file",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "File path to read"
                    }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "read_file_advanced",
            "description": "Read specific lines from a file",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "File path to read"
                    },
                    "start_line": {
                        "type": "integer",
                        "description": "Starting line number (1-indexed)"
                    },
                    "end_line": {
                        "type": "integer",
                        "description": "Ending line number (inclusive)"
                    }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "apply_edit",
            "description": "Apply AI-powered edits to a file",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "File path to edit"
                    },
                    "prompt": {
                        "type": "string",
                        "description": "Description of the edit to apply"
                    }
                },
                "required": ["path", "prompt"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "check_compilation_errors",
            "description": "Check for compilation errors in script files. Can check a specific file or all script files in the project.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Script file path to check (not required if check_all is true)"
                    },
                    "check_all": {
                        "type": "boolean",
                        "description": "If true, check all script files in the project instead of a specific file",
                        "default": False
                    }
                },
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "image_operation",
            "description": "Dynamic image generation and editing. Specify which images to use as inputs, or leave empty for pure text generation. Perfect for: generating new images, editing specific uploaded images, combining multiple images, or modifying existing images.",
            "parameters": {
                "type": "object",
                "properties": {
                    "description": {
                        "type": "string",
                        "description": "Detailed description of the desired image or modification."
                    },
                    "images": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "Array of image identifiers to use as inputs. Leave empty [] for pure text generation. Include specific image IDs when you want to edit/combine existing images from the conversation."
                    },
                    "style": {
                        "type": "string",
                        "description": "Art style (e.g., 'realistic', 'anime', 'pixel_art', 'cartoon', 'photographic')"
                    }
                },
                "required": ["description"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "create_script_file",
            "description": "Create a new script file with AI-generated content. If no scene is loaded and you get scene errors, use manage_scene tool first to create a new scene.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "File path where to create the script (e.g., 'res://scripts/player.gd')"
                    },
                    "script_type": {
                        "type": "string",
                        "description": "Type of script to generate (e.g., 'player_controller', 'enemy_ai', 'weapon_system')"
                    },
                    "node_type": {
                        "type": "string", 
                        "description": "Target node type (e.g., 'CharacterBody2D', 'RigidBody2D', 'Area2D')"
                    },
                    "description": {
                        "type": "string",
                        "description": "Detailed description of what the script should do"
                    }
                },
                "required": ["path", "script_type", "description"]
            }
        }
    },
    {
        "type": "function", 
        "function": {
            "name": "delete_file_safe",
            "description": "Safely delete a file with confirmation",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "File path to delete"
                    },
                    "backup": {
                        "type": "boolean", 
                        "description": "Whether to create backup before deletion",
                        "default": True
                    }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "edit_file_with_diff",
            "description": "Edit a file and show before/after diff in results",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "File path to edit"
                    },
                    "prompt": {
                        "type": "string", 
                        "description": "Description of the edit to apply"
                    },
                    "show_full_diff": {
                        "type": "boolean",
                        "description": "Whether to show complete file diff or just changes",
                        "default": False
                    }
                },
                "required": ["path", "prompt"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "search_across_project",
            "description": "Search for relevant files across the entire project using semantic similarity and relationship context. Perfect for understanding how systems work, finding related code, or discovering project structure. Includes graph relationships showing how files connect.",
            "parameters": {
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "Search query describing what you're looking for (e.g., 'player movement', 'UI system', 'audio handling')"
                    },
                    "include_graph": {
                        "type": "boolean",
                        "description": "Include relationship graph context showing how files connect",
                        "default": True
                    },
                    "max_results": {
                        "type": "integer",
                        "description": "Maximum number of similar files to return",
                        "default": 5
                    },
                    "modality_filter": {
                        "type": "string",
                        "description": "Filter by file type: 'text' (scripts/scenes), 'image', 'audio', or null for all types"
                    }
                },
                "required": ["query"]
            }
        }
    }
]


@app.route('/stop', methods=['POST'])
def stop_chat():
    """Stop a streaming chat request"""
    data = request.json
    request_id = data.get('request_id')
    
    if not request_id:
        return jsonify({"error": "No request_id provided"}), 400
    
    with stop_requests_lock:
        if request_id in ACTIVE_REQUESTS:
            ACTIVE_REQUESTS[request_id]["stop"] = True
            print(f"STOP_REQUEST: Marked request {request_id} for stopping")
            return jsonify({"success": True, "message": "Stop signal sent"})
        else:
            print(f"STOP_REQUEST: Request {request_id} not found in active requests")
            return jsonify({"success": False, "message": "Request not found or already completed"}), 404

@app.route('/chat', methods=['POST'])
def chat():
    """
    Main chat endpoint that handles the full conversation flow:
    1. Receives messages from Godot
    2. Calls OpenAI API 
    3. Executes any tool calls
    4. Streams the final response back to Godot
    """
    # Verify authentication
    user, error_response, status_code = verify_authentication()
    if error_response:
        return error_response, status_code
    
    data = request.json
    messages = data.get('messages', [])

    if not messages:
        return jsonify({"error": "No messages provided"}), 400

    # Generate unique request ID and register it
    request_id = str(uuid.uuid4())
    with stop_requests_lock:
        ACTIVE_REQUESTS[request_id] = {"stop": False, "timestamp": time.time()}
    
    # Clean up old requests
    cleanup_old_requests()
    
    print(f"CHAT_START: New request {request_id} registered")

    def generate_stream():
        def check_stop():
            """Check if this request should be stopped"""
            with stop_requests_lock:
                return ACTIVE_REQUESTS.get(request_id, {}).get("stop", False)
        
        try:
            # Send request_id first so frontend can use it for stop requests
            yield json.dumps({"request_id": request_id, "status": "started"}) + '\n'
            
            # Filter out any None or invalid messages from the start
            conversation_messages = []
            for msg in messages:
                if msg is not None and isinstance(msg, dict) and msg.get('role'):
                    conversation_messages.append(msg)
                else:
                    print(f"STARTUP_FILTER: Skipping invalid message: {type(msg)} = {msg}")
                
                # Check for stop even during initial processing
                if check_stop():
                    print(f"STOP_DETECTED: Request {request_id} stopped during message filtering")
                    yield json.dumps({"status": "stopped", "message": "Request stopped"}) + '\n'
                    return

            while True:  # Loop to handle tool calling and responses
                # Check for stop before each major operation
                if check_stop():
                    print(f"STOP_DETECTED: Request {request_id} stopped before OpenAI call")
                    yield json.dumps({"status": "stopped", "message": "Request stopped"}) + '\n'
                    return
                
                print(f"CONVERSATION_LOOP: Starting OpenAI call with {len(conversation_messages)} messages")
                if conversation_messages:
                    last_msg = conversation_messages[-1]
                    if last_msg and isinstance(last_msg, dict):
                        print(f"CONVERSATION_LOOP: Last message: {last_msg.get('role', 'unknown')}")
                    else:
                        print(f"CONVERSATION_LOOP: Last message is invalid: {type(last_msg)}")
                    
                # Debug logs for OpenAI messages have been quieted to reduce console noise.
                
                # Clean messages for OpenAI (preserve vision content, remove custom fields)
                openai_messages = []
                for msg in conversation_messages:
                    if msg is None:
                        # print(f"CLEAN_MESSAGES: Skipping None message")
                        continue
                    if not isinstance(msg, dict):
                        # print(f"CLEAN_MESSAGES: Skipping non-dict message: {type(msg)}")
                        continue
                        
                    clean_msg = {
                        'role': msg['role'],
                        'content': msg.get('content') # Use .get for safety
                    }
                    # Include standard OpenAI fields
                    if 'tool_calls' in msg:
                        # Ensure tool calls have required 'type' field for OpenAI
                        fixed_tool_calls = []
                        for tool_call in msg['tool_calls']:
                            if isinstance(tool_call, dict):
                                fixed_tool_call = tool_call.copy()
                                # Add 'type' field if missing
                                if 'type' not in fixed_tool_call:
                                    fixed_tool_call['type'] = 'function'
                                fixed_tool_calls.append(fixed_tool_call)
                        clean_msg['tool_calls'] = fixed_tool_calls
                    if 'tool_call_id' in msg:
                        clean_msg['tool_call_id'] = msg['tool_call_id']
                    if 'name' in msg:
                        clean_msg['name'] = msg['name']
                    
                    # Remove custom frontend fields that may contain large data
                    # Keep only standard OpenAI message format
                    openai_messages.append(clean_msg)
                
                # Debug: Check total token usage
                total_chars = sum(len(str(msg.get('content', ''))) for msg in openai_messages)
                print(f"OPENAI_PREP: Sending {len(openai_messages)} messages to OpenAI, total chars: {total_chars}")
                if total_chars > 100000:
                    print(f"OPENAI_PREP: WARNING - Very large message content ({total_chars} chars), may hit token limits!")
                
                response = client.chat.completions.create(
                    model=MODEL,
                    messages=openai_messages,
                    tools=godot_tools,
                    tool_choice="auto",
                    stream=True
                )

                full_text_response = ""
                tool_call_aggregator = {}
                tool_ids = {}
                current_tool_index = None
                chunk_count = 0
                
                for chunk in response:
                    # Check for stop during streaming - this is critical for mid-stream stopping
                    if check_stop():
                        print(f"STOP_DETECTED: Request {request_id} stopped during streaming")
                        yield json.dumps({"status": "stopped", "message": "Request stopped during streaming"}) + '\n'
                        return
                    
                    chunk_count += 1
                    if chunk.choices and chunk.choices[0].delta:
                        delta = chunk.choices[0].delta
                        
                        # Handle streaming text content
                        if delta.content:
                            full_text_response += delta.content
                            yield json.dumps({
                                "content_delta": delta.content,
                                "status": "streaming"
                            }) + '\n'
                        
                        # Handle tool calls
                        if delta.tool_calls:
                            for tool_call in delta.tool_calls:
                                index = tool_call.index
                                current_tool_index = index
                                
                                if index not in tool_call_aggregator:
                                    tool_call_aggregator[index] = {
                                        "name": "",
                                        "arguments": ""
                                    }
                                    tool_ids[index] = tool_call.id or f"call_{index}"
                                
                                if tool_call.function:
                                    if tool_call.function.name:
                                        tool_call_aggregator[index]["name"] = tool_call.function.name
                                    if tool_call.function.arguments:
                                        tool_call_aggregator[index]["arguments"] += tool_call.function.arguments
                
                print(f"RESPONSE_DEBUG: Processed {chunk_count} chunks, text_length: {len(full_text_response)}, tools: {len(tool_call_aggregator)}")
                if tool_call_aggregator:
                    print(f"RESPONSE_DEBUG: Tool calls: {[f['name'] for f in tool_call_aggregator.values()]}")
                if not full_text_response and not tool_call_aggregator:
                    print("RESPONSE_DEBUG: WARNING - OpenAI responded with NO content and NO tool calls!")

                # Now that we've processed all chunks, handle the results

                # --- Backend-Only Tool Execution (Image Generation + Search) ---
                backend_tools_detected = [func.get("name") for func in tool_call_aggregator.values() if func.get("name") in ["image_operation", "search_across_project"]]
                print(f"BACKEND_DETECTION: Found {len(backend_tools_detected)} backend tools: {backend_tools_detected}")
                
                if any(func.get("name") in ["image_operation", "search_across_project"] for func in tool_call_aggregator.values()):
                    # This is a backend-only tool call, so we will execute it,
                    # add the results to the conversation, and loop again for the AI's final response.
                    
                    # We need the original tool call data to append to the history
                    original_tool_calls_for_history = []
                    
                    # Execute image operation
                    tool_results_for_history = []
                    
                    for i, func in tool_call_aggregator.items():
                        tool_id = tool_ids[i]
                        original_tool_calls_for_history.append({
                            "id": tool_id,
                            "type": "function",
                            "function": {"name": func["name"], "arguments": func["arguments"]},
                        })
                        
                        if func["name"] == "image_operation":
                            # Check for stop before tool execution
                            if check_stop():
                                print(f"STOP_DETECTED: Request {request_id} stopped before tool execution")
                                yield json.dumps({"status": "stopped", "message": "Request stopped before tool execution"}) + '\n'
                                return
                            
                            yield json.dumps({"tool_starting": "image_operation", "tool_id": tool_id, "status": "tool_starting"}) + '\n'
                            try:
                                arguments = json.loads(func["arguments"])
                            except json.JSONDecodeError:
                                arguments = {}
                            
                            # AI now intelligently specifies which images to use via the 'images' parameter

                            # Execute the operation with conversation context
                            image_result = image_operation_internal(arguments, conversation_messages)
                            
                            # Check for stop after tool execution
                            if check_stop():
                                print(f"STOP_DETECTED: Request {request_id} stopped after tool execution")
                                yield json.dumps({"status": "stopped", "message": "Request stopped after tool execution"}) + '\n'
                                return
                            
                            # Yield result to frontend immediately
                            yield json.dumps({"tool_executed": "image_operation", "tool_result": image_result, "status": "tool_completed"}) + '\n'
                            
                            # Prepare tool result for conversation history (exclude massive image data)
                            tool_result_for_openai = {
                                "success": image_result.get("success"),
                                "description": image_result.get("description"),
                                "style": image_result.get("style"),
                                "input_images": image_result.get("input_images", 0),
                                "requested_images": image_result.get("requested_images", 0)
                            }
                            # Exclude the massive 'image_data' field to save tokens
                            
                            tool_results_for_history.append({
                                "tool_call_id": tool_id,
                                "role": "tool",
                                "name": "image_operation",
                                "content": json.dumps(tool_result_for_openai),
                            })
                        
                        elif func["name"] == "search_across_project":
                            # Check for stop before tool execution
                            if check_stop():
                                print(f"STOP_DETECTED: Request {request_id} stopped before tool execution")
                                yield json.dumps({"status": "stopped", "message": "Request stopped before tool execution"}) + '\n'
                                return
                            
                            yield json.dumps({"tool_starting": "search_across_project", "tool_id": tool_id, "status": "tool_starting"}) + '\n'
                            try:
                                arguments = json.loads(func["arguments"])
                            except json.JSONDecodeError:
                                arguments = {}
                            
                            # Execute the search operation (pass current user context)
                            search_result = search_across_project_internal(arguments, current_user=user)
                            
                            # Check for stop after tool execution
                            if check_stop():
                                print(f"STOP_DETECTED: Request {request_id} stopped after tool execution")
                                yield json.dumps({"status": "stopped", "message": "Request stopped after tool execution"}) + '\n'
                                return
                            
                            # Yield result to frontend immediately
                            yield json.dumps({"tool_executed": "search_across_project", "tool_result": search_result, "tool_call_id": tool_id, "status": "tool_completed"}) + '\n'
                            
                            # Prepare tool result for conversation history
                            tool_result_for_openai = {
                                "success": search_result.get("success"),
                                "query": search_result.get("query"),
                                "file_count": search_result.get("file_count", 0),
                                "message": search_result.get("message"),
                                "similar_files": search_result.get("results", {}).get("similar_files", [])[:3]  # Limit to first 3 for token efficiency
                            }
                            
                            tool_results_for_history.append({
                                "tool_call_id": tool_id,
                                "role": "tool",
                                "name": "search_across_project",
                                "content": json.dumps(tool_result_for_openai),
                            })
                
                    # Add the assistant's decision to call the tool to history
                    assistant_message = {"role": "assistant", "content": None, "tool_calls": original_tool_calls_for_history}
                    conversation_messages.append(assistant_message)
                    print(f"CONVERSATION_ADD: Added assistant message with tool calls")
                    
                    # Add the results of the tool call to history
                    for tool_result in tool_results_for_history:
                        if tool_result is None:
                            print(f"CONVERSATION_ADD: ERROR - Attempting to add None tool result!")
                            continue
                        conversation_messages.append(tool_result)
                                                    # print(f"CONVERSATION_ADD: Added tool result: {tool_result.get('name', 'unknown')}")

                    # Now, loop again to get the final text response from the AI
                    # print("CONVERSATION_LOOP: Backend tool executed. Continuing loop for final AI response.")
                    continue

                # --- Frontend Tool Calls & Final Text Responses ---
                
                print(f"FRONTEND_PROCESSING: Reached frontend tool processing. tool_call_aggregator has {len(tool_call_aggregator)} tools")
                
                # If we get here, it means no backend tools were called.
                # It's either a final text response or tool calls for the frontend.
                
                # Append assistant message (will include tool calls if any)
                assistant_message = {
                    "role": "assistant",
                    "content": full_text_response if full_text_response else None,
                }

                if tool_call_aggregator:
                    print(f"FRONTEND_PROCESSING: Processing {len(tool_call_aggregator)} frontend tool calls")
                    # Prepare tool calls for both history and frontend
                    tool_calls_for_history = []
                    tool_calls_for_frontend = []
                    for i, func in tool_call_aggregator.items():
                        tool_id = tool_ids[i]
                        print(f"FRONTEND_PROCESSING: Processing tool {func['name']} with id {tool_id}")
                        tool_calls_for_history.append({
                            "id": tool_id,
                            "type": "function",
                            "function": {"name": func["name"], "arguments": func["arguments"]},
                        })
                        tool_calls_for_frontend.append({
                            "id": tool_id,
                            "function": {
                                "name": func["name"],
                                "arguments": func["arguments"]
                            }
                        })
                    
                    assistant_message["tool_calls"] = tool_calls_for_history
                    conversation_messages.append(assistant_message)
                    print(f"CONVERSATION_ADD: Added frontend assistant message with {len(tool_calls_for_history)} tool calls")
                    
                    print(f"FRONTEND_PROCESSING: Sending {len(tool_calls_for_frontend)} tool calls to frontend")
                    # Yield tool calls to the frontend in the format it expects
                    frontend_response = {
                        "status": "executing_tools",
                        "assistant_message": {
                            "role": "assistant",
                            "content": full_text_response or None,
                            "tool_calls": tool_calls_for_frontend
                        }
                    }
                    yield json.dumps(frontend_response) + '\n'
                    print(f"FRONTEND_PROCESSING: Tool calls sent, breaking from loop")
                    break  # Exit loop after sending tools to frontend

                # If no tools, it's a final text response. Append and break.
                print(f"FRONTEND_PROCESSING: No tools detected, treating as final text response")
                conversation_messages.append(assistant_message)
                print(f"CONVERSATION_ADD: Added final text response message")
                yield json.dumps({"status": "completed"}) + '\n'
                break # Exit loop
        
        except Exception as e:
            print(f"ERROR: Exception in stream generation: {e}")
            yield json.dumps({"error": str(e), "status": "error"}) + '\n'
        
        finally:
            # Clean up this request from active requests
            with stop_requests_lock:
                if request_id in ACTIVE_REQUESTS:
                    del ACTIVE_REQUESTS[request_id]
                    print(f"CLEANUP: Removed request {request_id} from active requests")

    return Response(generate_stream(), mimetype='application/x-ndjson')

@app.route('/generate_script', methods=['POST'])
def generate_script():
    """Generate script content using AI"""
    data = request.json
    script_type = data.get('script_type', '')
    node_type = data.get('node_type', 'Node') 
    description = data.get('description', '')
    
    print(f"GENERATE_SCRIPT: Received request for {script_type} script")
    
    if not script_type or not description:
        return jsonify({"error": "Missing script_type or description"}), 400
    
    # Generate script using AI
    script_prompt = f"""
    Create a GDScript for a {node_type} that serves as a {script_type}.
    
    Requirements: {description}
    
    CRITICAL REQUIREMENTS:
    - Return ONLY raw GDScript code
    - NO markdown formatting (no ```, no ```gdscript, no ```gd)
    - NO explanations or comments outside the code
    - Use GODOT 4 syntax: "extends RefCounted" (NOT "extends Reference")
    - Use GODOT 4 syntax: "extends Node" (NOT "extends KinematicBody2D")
    - Use GODOT 4 syntax: "extends CharacterBody2D" (NOT "extends KinematicBody2D")
    - Use GODOT 4 syntax: "extends RigidBody2D" (NOT "extends RigidBody2D")
    - Ensure proper GDScript syntax for Godot 4.x
    - Start directly with "extends" or class declaration
    
    Example format:
    extends RefCounted
    
    func my_function():
        pass
    """
    
    try:
        response = client.chat.completions.create(
            model=MODEL,
            messages=[{"role": "user", "content": script_prompt}]
        )
        
        script_content = response.choices[0].message.content
        
        # Clean up any markdown wrappers that might have leaked through
        script_content = script_content.strip()
        
        # Remove markdown code blocks if they exist
        if script_content.startswith('```'):
            lines = script_content.split('\n')
            # Remove first line if it's a code block marker
            if lines[0].startswith('```'):
                lines = lines[1:]
            # Remove last line if it's a closing code block marker
            if lines and lines[-1].strip() == '```':
                lines = lines[:-1]
            script_content = '\n'.join(lines)
        
        # Remove any remaining ``` markers
        script_content = script_content.replace('```gdscript', '').replace('```gd', '').replace('```', '')
        script_content = script_content.strip()
        
        print(f"GENERATE_SCRIPT: Cleaned script content (first 200 chars): {script_content[:200]}")
        
        return jsonify({
            "success": True,
            "script_content": script_content,
            "script_type": script_type,
            "node_type": node_type
        })
        
    except Exception as e:
        print(f"GENERATE_SCRIPT_ERROR: {e}")
        return jsonify({
            "error": str(e),
            "success": False
        }), 500

@app.route('/predict_code_edit', methods=['POST'])
def predict_code_edit():
    """
    Uses OpenAI's Predicted Outputs feature to edit code files.
    Provides the original file content as prediction and returns the complete edited file.
    """
    data = request.json
    file_content = data.get('file_content', '')
    prompt = data.get('prompt')
    
    print(f"APPLY_EDIT_REQUEST: Received request with prompt: '{prompt}' for file content length: {len(file_content)}")

    if not prompt:
        return jsonify({"error": "Missing 'prompt'"}), 400

    # Simple, clear prompt for file editing
    user_prompt = f"Edit this file according to the request: {prompt}\n\nReturn the complete edited file content."

    try:
        # Simple approach: provide file content in context and ask for complete edited file
        full_prompt = f"""Edit this file according to the request: {prompt}

Original file content:
```
{file_content}
```

Return the complete edited file content (no explanations, just the code):"""

        response = client.chat.completions.create(
            model=MODEL,
            messages=[
                {"role": "user", "content": full_prompt}
            ]
        )

        edited_content = response.choices[0].message.content
        print(f"APPLY_EDIT_MODEL: Received edited file content (length: {len(edited_content)})")
        
        # Return the complete edited file content
        return jsonify({
            "edited_content": edited_content,
            "success": True
        })
        
    except Exception as e:
        print(f"APPLY_EDIT_ERROR: {e}")
        return jsonify({
            "error": str(e),
            "success": False
        }), 500

@app.route('/auth/login', methods=['GET'])
def auth_login():
    """Start OAuth authentication process"""
    try:
        machine_id = request.args.get('machine_id')
        provider = request.args.get('provider', 'google')  # Default to Google
        
        if not machine_id:
            return jsonify({"error": "machine_id parameter required"}), 400
        
        if provider == 'google':
            auth_url = auth_manager.get_google_auth_url(machine_id)
        elif provider == 'github':
            auth_url = auth_manager.get_github_auth_url(machine_id)
        else:
            return jsonify({"error": "Unsupported provider"}), 400
        
        return redirect(auth_url)
        
    except Exception as e:
        return jsonify({"error": str(e), "success": False}), 500

@app.route('/auth/callback', methods=['GET'])
@app.route('/api/auth/callback', methods=['GET'])
def auth_callback():
    """Handle OAuth callback"""
    try:
        state = request.args.get('state')
        code = request.args.get('code')
        error = request.args.get('error')
        
        if error:
            return f"<html><body><h1>Authentication Error</h1><p>{error}</p></body></html>", 400
        
        if not state or not code:
            return "<html><body><h1>Authentication Error</h1><p>Missing state or code parameter</p></body></html>", 400
        
        # Determine provider from pending auth
        pending = auth_manager.pending_auth.get(state)
        if not pending:
            return "<html><body><h1>Authentication Error</h1><p>Invalid or expired state</p></body></html>", 400
        
        provider = pending['provider']
        
        print(f"Processing {provider} callback for state: {state}")
        
        if provider == 'google':
            result = auth_manager.handle_google_callback(state, code)
        elif provider == 'github':
            result = auth_manager.handle_github_callback(state, code)
        else:
            return "<html><body><h1>Authentication Error</h1><p>Invalid provider</p></body></html>", 400
        
        print(f"Auth result: {result}")
        
        if result['success']:
            user = result['user']
            return f"""
            <html>
            <body>
                <h1>Authentication Successful!</h1>
                <p>Welcome, {user['name']}!</p>
                <p>You can now close this window and return to Godot.</p>
                <script>window.close();</script>
            </body>
            </html>
            """
        else:
            return f"<html><body><h1>Authentication Failed</h1><p>{result['error']}</p></body></html>", 400
            
    except Exception as e:
        return f"<html><body><h1>Authentication Error</h1><p>{str(e)}</p></body></html>", 500

@app.route('/auth/status', methods=['POST'])
def auth_status():
    """Check authentication status for a machine"""
    try:
        data = request.json
        machine_id = data.get('machine_id')
        
        if not machine_id:
            return jsonify({"error": "machine_id required", "success": False}), 400
        
        user_data = auth_manager.get_user_by_machine_id(machine_id)
        
        if user_data:
            return jsonify({
                "success": True,
                "user": user_data['user'],
                "token": user_data['token']
            })
        else:
            return jsonify({
                "success": False,
                "error": "Not authenticated"
            }), 401
            
    except Exception as e:
        return jsonify({"error": str(e), "success": False}), 500

@app.route('/auth/logout', methods=['POST'])
def auth_logout():
    """Logout user"""
    try:
        data = request.json
        machine_id = data.get('machine_id')
        user_id = data.get('user_id')
        
        if not machine_id:
            return jsonify({"error": "machine_id required", "success": False}), 400
        
        success = auth_manager.logout_user(machine_id, user_id)
        
        return jsonify({
            "success": success,
            "message": "Logged out successfully" if success else "No active session found"
        })
        
    except Exception as e:
        return jsonify({"error": str(e), "success": False}), 500

@app.route('/embed', methods=['POST'])
def embed_endpoint():
    """
    Multimodal embedding endpoint for managing project file embeddings with user authentication
    
    Actions:
    - index_project: Index all project files
    - index_file: Index specific file
    - remove_file: Remove file from index
    - search: Search for similar files with optional modality filtering
    - status: Get project summary
    - update_file: Update specific file embedding
    """
    try:
        # Verify authentication
        user, error_response, status_code = verify_authentication()
        if error_response:
            return jsonify(error_response), status_code
        
        data = request.json
        action = data.get('action')
        project_root = data.get('project_root')
        
        if not action:
            return jsonify({"error": "No action specified"}), 400
        
        if not project_root:
            return jsonify({"error": "project_root required"}), 400
        
        # Get user-specific embedding manager
        em = get_embedding_manager(user['id'], project_root)
        
        if action == 'index_project':
            force_reindex = data.get('force_reindex', False)
            stats = em.index_project(force_reindex=force_reindex)
            return jsonify({
                "success": True,
                "action": "index_project",
                "stats": stats
            })
        
        elif action == 'index_file':
            file_path = data.get('file_path')
            if not file_path:
                return jsonify({"error": "file_path required for index_file action"}), 400
            
            indexed = em.index_file(file_path)
            return jsonify({
                "success": True,
                "action": "index_file",
                "file_path": file_path,
                "indexed": indexed
            })
        
        elif action == 'update_file':
            file_path = data.get('file_path')
            if not file_path:
                return jsonify({"error": "file_path required for update_file action"}), 400
            
            updated = em.update_file(file_path)
            return jsonify({
                "success": True,
                "action": "update_file",
                "file_path": file_path,
                "updated": updated
            })
        
        elif action == 'remove_file':
            file_path = data.get('file_path')
            if not file_path:
                return jsonify({"error": "file_path required for remove_file action"}), 400
            
            removed = em.remove_file(file_path)
            return jsonify({
                "success": True,
                "action": "remove_file",
                "file_path": file_path,
                "removed": removed
            })
        
        elif action == 'search':
            query = data.get('query')
            if not query:
                return jsonify({"error": "query required for search action"}), 400
            
            k = data.get('k', 5)
            modality_filter = data.get('modality_filter')  # Optional: 'text', 'image', 'audio'
            include_graph = data.get('include_graph', False)  # Include graph relationships
            
            if include_graph:
                # Enhanced search with graph context
                results = em.search_with_graph_context(query, k, include_connected=True)
                return jsonify({
                    "success": True,
                    "action": "search_with_graph",
                    "query": query,
                    "results": results
                })
            else:
                # Regular similarity search
                results = em.search_similar_files(query, k, modality_filter)
                return jsonify({
                    "success": True,
                    "action": "search",
                    "query": query,
                    "modality_filter": modality_filter,
                    "results": [{"file_path": path, "similarity": score, "modality": modality} for path, score, modality in results]
                })
        
        elif action == 'status':
            summary = em.get_project_summary()
            graph_summary = em.get_graph_summary()
            return jsonify({
                "success": True,
                "action": "status",
                "summary": summary,
                "graph": graph_summary
            })
        
        elif action == 'graph_connections':
            file_path = data.get('file_path')
            if not file_path:
                return jsonify({"error": "file_path required for graph_connections action"}), 400
            
            max_depth = data.get('max_depth', 2)
            connections = em.get_connected_files(file_path, max_depth)
            
            return jsonify({
                "success": True,
                "action": "graph_connections",
                "file_path": file_path,
                "max_depth": max_depth,
                "connections": connections
            })
        
        elif action == 'central_files':
            top_k = data.get('top_k', 10)
            central_files = em.get_central_files(top_k)
            
            return jsonify({
                "success": True,
                "action": "central_files",
                "central_files": [{"file_path": path, "centrality": score} for path, score in central_files]
            })
        
        else:
            return jsonify({"error": f"Unknown action: {action}"}), 400
    
    except Exception as e:
        print(f"EMBED_ERROR: {e}")
        return jsonify({
            "error": str(e),
            "success": False
        }), 500

@app.route('/search_project', methods=['POST'])
def search_project():
    """
    Search across project files using semantic similarity and graph context
    Used by the search_across_project tool
    """
    try:
        # Verify authentication (with dev mode bypass)
        user, error_response, status_code = verify_authentication()
        if error_response:
            return jsonify(error_response), status_code
        
        data = request.json
        query = data.get('query')
        project_root = data.get('project_root')
        
        if not query:
            return jsonify({"error": "Query required"}), 400
        if not project_root:
            return jsonify({"error": "project_root required"}), 400
        
        # Get search parameters
        include_graph = data.get('include_graph', True)
        max_results = data.get('max_results', 5)
        modality_filter = data.get('modality_filter')
        
        # Get user-specific embedding manager
        em = get_embedding_manager(user['id'], project_root)
        
        if include_graph:
            # Enhanced search with graph context
            results = em.search_with_graph_context(query, max_results, include_connected=True)
            
            # Format for tool response
            formatted_results = {
                "similar_files": [
                    {
                        "file_path": path,
                        "similarity": float(score),
                        "modality": modality,
                        "connections": results['connected_files'].get(path, {})
                    }
                    for path, score, modality in results['similarity_results']
                ],
                "central_files": [
                    {
                        "file_path": path,
                        "centrality": float(score)
                    }
                    for path, score in results['central_files']
                ],
                "graph_summary": results['graph_summary']
            }
        else:
            # Regular similarity search
            similarity_results = em.search_similar_files(query, max_results, modality_filter)
            formatted_results = {
                "similar_files": [
                    {
                        "file_path": path,
                        "similarity": float(score),
                        "modality": modality
                    }
                    for path, score, modality in similarity_results
                ],
                "central_files": [],
                "graph_summary": {}
            }
        
        return jsonify({
            "success": True,
            "query": query,
            "results": formatted_results,
            "include_graph": include_graph
        })
        
    except Exception as e:
        print(f"SEARCH_PROJECT_ERROR: {e}")
        return jsonify({
            "error": str(e),
            "success": False
        }), 500

@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint for testing"""
    return jsonify({"status": "healthy", "service": "godot-ai-embedding-service"})

if __name__ == '__main__':
    # Use PORT environment variable for Cloud Run, fallback to 8000 for local dev
    port = int(os.environ.get('PORT', 8000))
    app.run(host='0.0.0.0', port=port, debug=False)