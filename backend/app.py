from flask import Flask, request, Response, jsonify
from openai import OpenAI
import os
from dotenv import load_dotenv
import json
import requests
import base64
import binascii

# Load environment variables from .env file
load_dotenv()

app = Flask(__name__)

# --- OpenAI API Configuration ---
api_key = os.getenv("OPENAI_API_KEY")
if not api_key:
    raise ValueError("OPENAI_API_KEY environment variable not set. Please create a .env file and add it.")

client = OpenAI(api_key=api_key)
MODEL = "gpt-4o"

def handle_apply_edit_internal(arguments: dict) -> dict:
    """Handle apply_edit tool internally by reading file and calling /apply endpoint"""
    try:
        file_path = arguments.get('path')
        prompt = arguments.get('prompt')
        
        if not file_path or not prompt:
            return {"error": "Missing required arguments: path and prompt"}
        
        # First, try to read the file from Godot tool server
        try:
            read_response = requests.post('http://localhost:8001/execute_tool', 
                                        json={
                                            'function_name': 'read_file_content',
                                            'arguments': {'path': file_path}
                                        }, 
                                        timeout=30)
            
            if read_response.status_code != 200:
                return {"error": f"Failed to read file: {read_response.text}"}
                
            read_result = read_response.json()
            if not read_result.get('success'):
                return {"error": f"Failed to read file: {read_result.get('message', 'Unknown error')}"}
                
            file_content = read_result.get('content', '')
            
        except Exception as e:
            return {"error": f"Failed to read file from Godot: {str(e)}"}
        
        # Call our internal /apply endpoint
        try:
            apply_response = requests.post('http://localhost:8000/apply', 
                                         json={
                                             'file_name': file_path,
                                             'file_content': file_content,
                                             'prompt': prompt,
                                             'tool_arguments': arguments
                                         }, 
                                         timeout=120)
            
            if apply_response.status_code != 200:
                return {"error": f"Apply endpoint failed: {apply_response.text}"}
                
            apply_result = apply_response.json()
            new_content = apply_result.get('edited_content')
            
            if not new_content:
                return {"error": "No edited content received from apply endpoint"}
                
        except Exception as e:
            return {"error": f"Failed to call apply endpoint: {str(e)}"}
        
        # Write the new content back to Godot
        try:
            # Note: This would need a write_file tool in Godot, or we return the content for Godot to handle
            return {
                "success": True,
                "message": f"File {file_path} edited successfully", 
                "edited_content": new_content,
                "original_length": len(file_content),
                "new_length": len(new_content)
            }
            
        except Exception as e:
            return {"error": f"Failed to write file: {str(e)}"}
            
    except Exception as e:
        return {"error": f"Internal apply_edit error: {str(e)}"}

# --- Image Generation Function ---
def generate_image_internal(arguments: dict) -> dict:
    """Generate an image using OpenAI's DALL-E API"""
    try:
        prompt = arguments.get('prompt', '')
        style = arguments.get('style', 'digital art')
        
        if not prompt:
            return {"success": False, "error": "No prompt provided for image generation"}
        
        # Enhance the prompt with style if specified
        enhanced_prompt = f"{prompt}, {style} style" if style and style != "digital art" else prompt
        
        print(f"DALL-E: Generating image with prompt: '{enhanced_prompt}'")
        
        # Call OpenAI's image generation API
        response = client.images.generate(
            model="dall-e-3",
            prompt=enhanced_prompt,
            size="1024x1024",
            quality="standard",
            response_format="b64_json",
            n=1
        )
        
        if response.data and len(response.data) > 0:
            image_data = response.data[0]
            base64_image = image_data.b64_json
            
            print(f"DALL-E: Successfully generated image ({len(base64_image)} chars base64)")
            
            return {
                "success": True,
                "message": f"Generated image: {enhanced_prompt}",
                "image_data": base64_image,
                "prompt": enhanced_prompt,
                "model": "dall-e-3",
                "size": "1024x1024"
            }
        else:
            return {"success": False, "error": "No image data returned from DALL-E"}
            
    except Exception as e:
        print(f"DALL-E ERROR: {e}")
        return {"success": False, "error": f"Image generation failed: {str(e)}"}

# --- Godot Tool Execution ---
def execute_godot_tool(function_name: str, arguments: dict) -> dict:
    """Execute a Godot tool by sending HTTP request to Godot tool server or internal endpoint"""
    
    print(f"TOOL_EXECUTION_DEBUG: Executing tool '{function_name}' with args: {arguments}")
    
    # Special handling for apply_edit - use internal endpoint
    if function_name == "apply_edit":
        print(f"TOOL_EXECUTION_DEBUG: Using internal apply_edit handler")
        return handle_apply_edit_internal(arguments)
    
    # Special handling for image generation - use DALL-E API
    if function_name == "generate_image":
        print(f"TOOL_EXECUTION_DEBUG: Using internal image generation handler")
        return generate_image_internal(arguments)
    
    print(f"TOOL_EXECUTION_DEBUG: Sending '{function_name}' to Godot tool server")
    
    try:
        # Send request to Godot tool server for other tools
        response = requests.post('http://localhost:8001/execute_tool', 
                               json={
                                   'function_name': function_name,
                                   'arguments': arguments
                               }, 
                               timeout=30)
        
        if response.status_code == 200:
            return response.json()
        else:
            return {"error": f"Tool server returned status {response.status_code}: {response.text}"}
            
    except requests.exceptions.Timeout:
        return {"error": f"Tool {function_name} timed out"}
    except requests.exceptions.ConnectionError:
        return {"error": "Could not connect to Godot tool server. Make sure it's running on port 8001"}
    except Exception as e:
        return {"error": f"Failed to execute tool {function_name}: {str(e)}"}

# --- Godot Tool Definitions (for OpenAI) ---
godot_tools = [
    {
        "type": "function",
        "function": {
            "name": "get_scene_info",
            "description": "Get information about the currently edited scene, including its name and root node.",
            "parameters": { "type": "object", "properties": {} }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_all_nodes",
            "description": "Get a list of all nodes in the current scene.",
            "parameters": { "type": "object", "properties": {} }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "search_nodes_by_type",
            "description": "Search for nodes of a specific type in the current scene.",
            "parameters": {
                "type": "object",
                "properties": {
                    "type": { "type": "string", "description": "The type of node to search for (e.g., 'Node2D', 'Camera3D')." }
                },
                "required": ["type"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_editor_selection",
            "description": "Get the currently selected nodes in the editor.",
            "parameters": { "type": "object", "properties": {} }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_node_properties",
            "description": "Gets the properties of a node in the scene, such as position, rotation, and scale.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The scene path to the node." }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "save_scene",
            "description": "Saves the currently open scene. Fails if the scene has not been saved before (i.e., has no file path).",
            "parameters": { "type": "object", "properties": {} }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_available_classes",
            "description": "Gets a list of all available Godot classes that inherit from Node and can be instantiated.",
            "parameters": { "type": "object", "properties": {} }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "create_node",
            "description": "Create a new node in the scene.",
            "parameters": {
                "type": "object",
                "properties": {
                    "type": { "type": "string", "description": "The type of node to create (e.g., 'Node2D', 'Button')." },
                    "name": { "type": "string", "description": "The name for the new node." },
                    "parent": { "type": "string", "description": "The path to the parent node. If not provided, it will be added to the scene root." }
                },
                "required": ["type", "name"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "delete_node",
            "description": "Delete a node from the scene.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The path to the node to delete." }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "set_node_property",
            "description": "Set a property on a node.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The path to the node." },
                    "property": { "type": "string", "description": "The name of the property to set." },
                    "value": { "description": "The new value for the property. The type should be appropriate for the property." }
                },
                "required": ["path", "property", "value"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "move_node",
            "description": "Move a node to be a child of another node.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The path to the node to move." },
                    "new_parent": { "type": "string", "description": "The path to the new parent node." }
                },
                "required": ["path", "new_parent"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "call_node_method",
            "description": "Calls a method on a specified node with given arguments.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The scene path to the node." },
                    "method": { "type": "string", "description": "The name of the method to call." },
                    "args": { 
                        "type": "array", 
                        "description": "An array of arguments to pass to the method.",
                        "items": {
                            "description": "Method argument (can be string, number, boolean, object, or array)"
                        }
                    }
                },
                "required": ["path", "method"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_node_script",
            "description": "Gets the file path of the script attached to a specified node.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The scene path to the node." }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "attach_script",
            "description": "Attaches an existing script to a specified node.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The scene path to the node." },
                    "script_path": { "type": "string", "description": "The project-relative path to the script file (e.g., 'res://player.gd')." }
                },
                "required": ["path", "script_path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "list_project_files",
            "description": "List files and directories in the project.",
            "parameters": {
                "type": "object",
                "properties": {
                    "dir": { "type": "string", "description": "The directory to list, starting from 'res://'. Defaults to the project root." },
                    "filter": { "type": "string", "description": "An optional filter for file names (e.g., '*.gd', '*.tscn')." }
                }
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "read_file_content",
            "description": "Read the entire content of a file.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The project-relative path to the file." }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "read_file_advanced",
            "description": "Read a specific range of lines from a file.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The project-relative path to the file." },
                    "start_line": { "type": "integer", "description": "The starting line number (1-indexed)." },
                    "end_line": { "type": "integer", "description": "The ending line number (inclusive)." }
                },
                "required": ["path"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "apply_edit",
            "description": "Applies a code edit to a file based on a prompt. This tool uses an AI to predict the change.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": { "type": "string", "description": "The project-relative path to the file to edit." },
                    "prompt": { "type": "string", "description": "A detailed natural language instruction for the code change to be made." }
                },
                "required": ["path", "prompt"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "manage_scene",
            "description": "Manage scenes: create new scenes, save scenes with names, open existing scenes, or instantiate scenes as nodes. The 'operation' parameter must be one of: 'create_new', 'save_as', 'open', or 'instantiate'.",
            "parameters": {
                "type": "object",
                "properties": {
                    "operation": {
                        "type": "string",
                        "enum": ["create_new", "save_as", "open", "instantiate"],
                        "description": "The scene operation to perform"
                    },
                    "path": {
                        "type": "string", 
                        "description": "File path for save_as and open operations"
                    },
                    "scene_path": {
                        "type": "string",
                        "description": "Path to scene file for instantiate operation"
                    },
                    "parent": {
                        "type": "string",
                        "description": "Parent node path for instantiate operation (optional)"
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
            "description": "Add a CollisionShape2D with a basic shape to a physics body node to fix collision warnings.",
            "parameters": {
                "type": "object",
                "properties": {
                    "node_path": {
                        "type": "string",
                        "description": "Path to the physics body node (CharacterBody2D, RigidBody2D, StaticBody2D, Area2D)"
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
            "name": "generalnodeeditor",
            "description": "Versatile tool for modifying node properties, positions, textures, and batch operations in Godot scenes. Supports single nodes or arrays of nodes. Can set position [x,y], scale [x,y], rotation, visibility, textures, and any node property. Returns detailed success/failure feedback for each operation.",
            "parameters": {
                "type": "object",
                "properties": {
                    "node_path": {
                        "type": "string",
                        "description": "Node path or '[path1,path2]' for batch operations. Single path: '/root/Player', Batch: '[/root/Mob1,/root/Mob2]'"
                    },
                    "properties": {
                        "type": "object",
                        "description": "Key-value pairs of properties to modify. Examples: {'position': [100,200], 'scale': [1.5,1.5], 'rotation': 45, 'visible': true}"
                    },
                    "texture_path": {
                        "type": "string",
                        "description": "Optional texture file path for Sprite2D nodes (e.g., 'res://textures/player.png')"
                    }
                },
                "required": ["node_path", "properties"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "check_compilation_errors",
            "description": "Check a GDScript file for compilation errors, syntax errors, or other issues. This helps verify that code changes are valid and will run without errors. Use this AFTER applying edits to ensure code quality.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "The file path to check for compilation errors (e.g., 'res://test.gd' or 'test.gd')"
                    }
                },
                "required": ["path"]
            }
        }
    }
]


@app.route('/chat', methods=['POST'])
def chat():
    """
    Main chat endpoint that handles the full conversation flow:
    1. Receives messages from Godot
    2. Calls OpenAI API 
    3. Executes any tool calls
    4. Streams the final response back to Godot
    """
    data = request.json
    messages = data.get('messages', [])

    if not messages:
        return jsonify({"error": "No messages provided"}), 400

    # Minimal logging - only message count
    # Full debug logging disabled - only apply_edit tool logs enabled

    def generate_stream():
        try:
            conversation_messages = messages.copy()
            
            while True:
                # OpenAI stream started (debug logging disabled)
                
                # Check if this might be an image generation request
                has_image_keywords = False
                has_uploaded_image = False
                
                if len(conversation_messages) >= 1:
                    last_message = conversation_messages[-1]
                    last_content = last_message.get("content", "")
                    
                    # Handle both string content and multi-modal content (array)
                    if isinstance(last_content, list):
                        # Extract text from multi-modal content and check for images
                        text_parts = []
                        for part in last_content:
                            if isinstance(part, dict):
                                if part.get("type") == "text":
                                    text_parts.append(part.get("text", ""))
                                elif part.get("type") == "image_url":
                                    has_uploaded_image = True
                        combined_text = " ".join(text_parts)
                    else:
                        # Simple string content
                        combined_text = str(last_content)
                    
                    has_image_keywords = any(word in combined_text.lower() for word in [
                        "generate", "create", "make", "draw", "image", "picture", "art", "photo", 
                        "illustration", "sketch", "design", "visualize", "render", "add", "edit", "modify"
                    ])
                
                # Setup enhanced tools for function calling (used as fallback or primary)
                enhanced_tools = godot_tools + [{
                    "type": "function",
                    "function": {
                        "name": "generate_image",
                        "description": "ALWAYS use this tool when the user wants to create, modify, or edit images. This includes: 1) Creating new images from descriptions, 2) Adding elements to uploaded images (like 'add an eye', 'make it blue', 'add a hat'), 3) Modifying existing images in any way, 4) Creating variations of uploaded images. When the user uploads an image and asks for changes, you MUST use this tool - analyze what you see in the uploaded image, then generate a new image that combines the original content with the requested modifications. Never say you 'can't edit images directly' - always use this tool instead.",
                        "parameters": {
                            "type": "object",
                            "properties": {
                                "prompt": {
                                    "type": "string",
                                    "description": "A detailed description of the image to generate. When modifying an uploaded image, include both: 1) A description of what you see in the original image, and 2) The specific modifications or additions requested by the user."
                                },
                                "style": {
                                    "type": "string",
                                    "description": "The style of the image (e.g., 'photorealistic', 'cartoon', 'digital art', 'pixel art')",
                                    "default": "digital art"
                                }
                            },
                            "required": ["prompt"]
                        }
                    }
                }]
                
                # Use Responses API for image generation (especially with uploaded images)
                use_responses_api = has_image_keywords or has_uploaded_image
                
                if use_responses_api:
                    print(f"IMAGE_GENERATION: Attempting Responses API (keywords: {has_image_keywords}, uploaded: {has_uploaded_image})")
                    
                    try:
                        # Convert conversation to Responses API format
                        responses_input = []
                        for msg in conversation_messages:
                            if msg["role"] == "user":
                                content = msg.get("content", "")
                                if isinstance(content, list):
                                    # Multi-modal content - convert to responses format
                                    user_input = {"role": "user", "content": []}
                                    for part in content:
                                        if part.get("type") == "text":
                                            text_content = part.get("text", "").strip()
                                            if text_content:  # Only add non-empty text
                                                user_input["content"].append({
                                                    "type": "input_text",
                                                    "text": text_content
                                                })
                                        elif part.get("type") == "image_url":
                                            user_input["content"].append({
                                                "type": "input_image", 
                                                "image_url": part.get("image_url", {}).get("url", "")
                                            })
                                    # Only add if content array is not empty
                                    if user_input["content"]:
                                        responses_input.append(user_input)
                                    else:
                                        print(f"IMAGE_GENERATION: Skipping user message with empty content array")
                                else:
                                    # Simple text content
                                    text_content = str(content).strip()
                                    if text_content:  # Only add non-empty text
                                        responses_input.append({
                                            "role": "user",
                                            "content": [{"type": "input_text", "text": text_content}]
                                        })
                            elif msg["role"] == "assistant":
                                # Include assistant responses for context, but only if they have meaningful content
                                assistant_content = msg.get("content", "")
                                if assistant_content and assistant_content.strip():
                                    responses_input.append({
                                        "role": "assistant", 
                                        "content": assistant_content
                                    })
                                # Skip assistant messages with empty/null content (tool-only messages)
                        
                        print(f"IMAGE_GENERATION: Converted {len(responses_input)} messages for Responses API")
                        for i, inp in enumerate(responses_input):
                            print(f"  [{i}] {inp['role']}: {type(inp.get('content'))} - {str(inp.get('content'))[:100]}...")
                        
                        # Use Responses API with image generation (matching working playground settings)
                        print("IMAGE_GENERATION: Creating Responses API request...")
                        
                        try:
                            response = client.responses.create(
                                model="gpt-4.1",  # Use same model as working playground
                                input=responses_input,
                                text={"format": {"type": "text"}},
                                reasoning={},
                                tools=[{
                                    "type": "image_generation",
                                    "size": "auto",
                                    "quality": "low",  # Use same quality as working playground
                                    "output_format": "png",
                                    "background": "auto",
                                    "moderation": "auto"
                                    # Remove partial_images since we're not streaming
                                }],
                                temperature=1,
                                max_output_tokens=2048,
                                top_p=1,
                                store=True,
                                stream=False  # Try non-streaming first
                            )
                            
                            print(f"IMAGE_GENERATION: Got response type: {type(response)}")
                            print(f"IMAGE_GENERATION: Response attributes: {dir(response)}")
                            
                            # The Responses API returns a .output list containing messages and image_generation_call results
                            if hasattr(response, 'output') and response.output:
                                print(f"IMAGE_GENERATION: Response has {len(response.output)} output items")
                                generated_image_sent = False
                                for out in response.output:
                                    print(f"  Output item type: {out.type}")
                                    if out.type == 'message':
                                        # Assistant text message
                                        if hasattr(out, 'content') and out.content:
                                            # Aggregate content parts
                                            text_parts = []
                                            for c in out.content:
                                                if hasattr(c, 'text') and c.text:
                                                    text_parts.append(c.text)
                                            if text_parts:
                                                full_text = " ".join(text_parts)
                                                yield json.dumps({"content_delta": full_text}) + '\n'
                                    elif out.type == 'image_generation_call':
                                        # Generated image result
                                        if hasattr(out, 'result') and out.result:
                                            image_data = {
                                                "success": True,
                                                "image_data": out.result,  # base64 png
                                                "prompt": getattr(out, 'revised_prompt', 'Generated image'),
                                                "model": "dall-e-3",
                                                "size": getattr(out, 'size', '1024x1024')
                                            }
                                            yield json.dumps({
                                                "tool_executed": "generate_image",
                                                "tool_result": image_data,
                                                "status": "tool_completed"
                                            }) + '\n'
                                            generated_image_sent = True
                                if generated_image_sent:
                                    print("IMAGE_GENERATION: Sent generated image to frontend")
                                else:
                                    print("IMAGE_GENERATION: No image found in output items")
                            else:
                                print("IMAGE_GENERATION: Response has no .output attribute - unhandled format")
                            
                            return
                            
                        except Exception as responses_error:
                            print(f"IMAGE_GENERATION: Non-streaming failed ({responses_error}), trying streaming...")
                            
                            # Try streaming version
                            response = client.responses.create(
                                model="gpt-4.1",
                                input=responses_input,
                                text={"format": {"type": "text"}},
                                reasoning={},
                                tools=[{
                                    "type": "image_generation",
                                    "size": "auto",
                                    "quality": "low",
                                    "output_format": "png",
                                    "background": "auto",
                                    "moderation": "auto"
                                }],
                                temperature=1,
                                max_output_tokens=2048,
                                top_p=1,
                                store=True,
                                stream=True
                            )
                        
                        print("IMAGE_GENERATION: Successfully created Responses API request")
                        
                        # Handle Responses API streaming
                        chunk_count = 0
                        for chunk in response:
                            chunk_count += 1
                            print(f"IMAGE_GENERATION: Received chunk {chunk_count}: {type(chunk)}")
                            print(f"  Chunk attributes: {dir(chunk)}")
                            print(f"  Chunk data: {chunk}")
                            
                            # Process different response types
                            if hasattr(chunk, 'text') and chunk.text:
                                print(f"  Processing text chunk: {chunk.text}")
                                yield json.dumps({"content_delta": chunk.text.delta}) + '\n'
                            elif hasattr(chunk, 'image') and chunk.image:
                                print(f"  Processing image chunk: {chunk.image}")
                                # Handle generated images
                                image_data = {
                                    "success": True,
                                    "image_data": chunk.image.data,  # base64 data
                                    "prompt": getattr(chunk.image, 'prompt', 'Generated image'),
                                    "model": "dall-e-3",
                                    "size": getattr(chunk.image, 'size', '1024x1024')
                                }
                                yield json.dumps({
                                    "tool_executed": "generate_image",
                                    "tool_result": image_data,
                                    "status": "tool_completed"
                                }) + '\n'
                            else:
                                print(f"  Unhandled chunk type - chunk: {chunk}")
                        
                        print(f"IMAGE_GENERATION: Processed {chunk_count} chunks total")
                        
                        # End the conversation after Responses API processing
                        return
                        
                    except Exception as e:
                        print(f"IMAGE_GENERATION: Responses API failed ({e}), falling back to function calling")
                        # Fall back to function calling approach
                        pass

                
                # Add system message for image generation tasks if not present
                if conversation_messages and (has_image_keywords or has_uploaded_image):
                    # Check if first message is already system
                    if not (conversation_messages[0].get("role") == "system"):
                        system_message = {
                            "role": "system",
                            "content": "You have access to a generate_image tool that can create and modify images. When users upload images and ask for modifications (like 'add an eye', 'make it blue', 'change the style'), you should ALWAYS use the generate_image tool. Analyze the uploaded image and create a new image that incorporates both the original content and the requested changes. Never say you 'can't edit images directly' - use the tool instead."
                        }
                        conversation_messages = [system_message] + conversation_messages
                
                # Use more targeted tool choice for image generation scenarios
                if has_image_keywords or has_uploaded_image:
                    tool_choice = {"type": "function", "function": {"name": "generate_image"}}
                    print(f"IMAGE_GENERATION: Using targeted tool choice for generate_image")
                else:
                    tool_choice = "auto"
                
                response_stream = client.chat.completions.create(
                    model=MODEL,
                    messages=conversation_messages,
                    tools=enhanced_tools,
                    tool_choice=tool_choice,
                    stream=True
                )
                use_chat_api = True
                
                # Variables to reconstruct the response
                full_text_response = ""
                tool_call_aggregator = {}  # index -> {"name": "", "arguments": ""}
                tool_ids = {}              # index -> id
                image_generations = []     # For image generation results
                
                # Iterate over the stream from OpenAI
                for chunk in response_stream:
                    if not use_chat_api:
                        # Handle responses API format
                        if hasattr(chunk, 'output') and chunk.output:
                            for output in chunk.output:
                                if output.type == "text" and hasattr(output, 'text') and output.text:
                                    full_text_response += output.text
                                    yield json.dumps({"content_delta": output.text, "status": "streaming"}) + '\n'
                                
                                elif output.type == "function_call":
                                    # Handle function calls (existing Godot tools)
                                    if hasattr(output, 'function_call'):
                                        fc = output.function_call
                                        index = getattr(fc, 'index', len(tool_call_aggregator))
                                        if index not in tool_call_aggregator:
                                            tool_call_aggregator[index] = {"name": "", "arguments": ""}
                                        if hasattr(fc, 'id'):
                                            tool_ids[index] = fc.id
                                        if hasattr(fc, 'name'):
                                            tool_call_aggregator[index]["name"] = fc.name
                                        if hasattr(fc, 'arguments'):
                                            tool_call_aggregator[index]["arguments"] = fc.arguments
                                
                                elif output.type == "image_generation_call":
                                    # Handle image generation
                                    if hasattr(output, 'result'):
                                        image_generations.append({
                                            "type": "image_generation",
                                            "base64_data": output.result,
                                            "id": getattr(output, 'id', f"img_{len(image_generations)}")
                                        })
                                        # Stream the image immediately
                                        yield json.dumps({
                                            "image_generated": {
                                                "base64_data": output.result,
                                                "id": getattr(output, 'id', f"img_{len(image_generations)}")
                                            },
                                            "status": "image_generated"
                                        }) + '\n'
                    else:
                        # Handle chat completions API format (fallback)
                        if hasattr(chunk, 'choices') and chunk.choices:
                            delta = chunk.choices[0].delta

                            # Stream content deltas immediately
                            if hasattr(delta, 'content') and delta.content:
                                full_text_response += delta.content
                                yield json.dumps({"content_delta": delta.content, "status": "streaming"}) + '\n'

                            # Aggregate tool call deltas
                            if hasattr(delta, 'tool_calls') and delta.tool_calls:
                                for tc in delta.tool_calls:
                                    index = tc.index
                                    if index not in tool_call_aggregator:
                                        tool_call_aggregator[index] = {"name": "", "arguments": ""}
                                    if tc.id:
                                        tool_ids[index] = tc.id
                                    if tc.function:
                                        if tc.function.name:
                                            tool_call_aggregator[index]["name"] += tc.function.name
                                        if tc.function.arguments:
                                            tool_call_aggregator[index]["arguments"] += tc.function.arguments
                # --- Stream has finished, decide next step ---
                
                if not tool_call_aggregator and not image_generations:
                    # Case 1: No tool calls or image generations, the conversation turn is complete.
                    if full_text_response:
                        conversation_messages.append({"role": "assistant", "content": full_text_response})
                    yield json.dumps({"status": "completed"}) + '\n'
                    break  # Exit the conversation loop
                
                # Case 1.5: Only image generations (no function calls)
                if image_generations and not tool_call_aggregator:
                    # Add assistant message with generated images to conversation history
                    assistant_message = {
                        "role": "assistant",
                        "content": full_text_response or "I've generated the requested image(s).",
                        "images": image_generations
                    }
                    conversation_messages.append(assistant_message)
                    yield json.dumps({"status": "completed"}) + '\n'
                    break  # Exit the conversation loop
                
                # Case 2: Tool calls were made, so execute them.
                
                # Assemble the full tool calls for OpenAI history
                final_tool_calls = []
                for i, func in sorted(tool_call_aggregator.items()):
                    final_tool_calls.append({
                        "id": tool_ids[i],
                        "type": "function",
                        "function": func
                    })
                
                # Add the complete assistant message to history
                assistant_message = {
                    "role": "assistant",
                    "content": full_text_response or None,
                    "tool_calls": final_tool_calls
                }
                conversation_messages.append(assistant_message)
                
                # Filter out backend-only tools before sending to frontend
                # Only send Godot tools to the frontend for execution
                frontend_tool_calls = []
                for tool_call in final_tool_calls:
                    function_name = tool_call["function"]["name"]
                    if function_name != "generate_image":  # Exclude backend-only tools
                        frontend_tool_calls.append(tool_call)
                
                # Create a frontend-specific assistant message (without backend tools)
                frontend_assistant_message = {
                    "role": "assistant",
                    "content": full_text_response or None,
                    "tool_calls": frontend_tool_calls
                }
                
                # Only send tool execution message if there are frontend tools to execute
                if frontend_tool_calls:
                    print(f"FRONTEND_TOOLS: Sending {len(frontend_tool_calls)} tools to frontend")
                    yield json.dumps({"assistant_message": frontend_assistant_message, "status": "executing_tools"}) + '\n'
                else:
                    print(f"BACKEND_ONLY: All {len(final_tool_calls)} tools are backend-only, not sending to frontend")
                
                # Execute tools and stream results back
                for tool_call in final_tool_calls:
                    function_name = tool_call["function"]["name"]
                    try:
                        arguments = json.loads(tool_call["function"]["arguments"])
                    except json.JSONDecodeError:
                        print(f"ERROR: Failed to decode tool arguments: {tool_call['function']['arguments']}")
                        tool_result = {"error": "Invalid JSON in arguments"}
                    else:
                        # Check if this is a backend-only tool (like image generation)
                        if function_name == "generate_image":
                            print(f"BACKEND_TOOL: Executing '{function_name}' internally")
                            tool_result = generate_image_internal(arguments)
                            print(f"BACKEND_TOOL: '{function_name}' completed: {tool_result.get('success', False)}")
                        else:
                            # For Godot tools, use the normal execution path
                            # Special logging for apply_edit tool
                            if function_name == "apply_edit":
                                print(f"APPLY_EDIT: Executing with arguments: {arguments}")
                            
                            print(f"TOOL_CALL_DEBUG: About to execute tool '{function_name}' with args: {arguments}")
                            tool_result = execute_godot_tool(function_name, arguments)
                            print(f"TOOL_CALL_DEBUG: Tool '{function_name}' returned: {tool_result}")
                            
                            # Log apply_edit results
                            if function_name == "apply_edit":
                                print(f"APPLY_EDIT: Tool result: {tool_result}")
                    
                    # --- (The existing tool result handling logic remains the same) ---
                    # Decode Base64, log, etc.
                    if isinstance(tool_result, dict) and 'content_base64' in tool_result:
                        try:
                            decoded_content = base64.b64decode(tool_result['content_base64']).decode('utf-8')
                            tool_result['content'] = decoded_content
                            del tool_result['content_base64']
                        except Exception as e:
                            tool_result['content'] = f"[Error: Failed to decode file content: {e}]"
                            if 'content_base64' in tool_result:
                                 del tool_result['content_base64']
                    
                    # For image generation, don't include the massive base64 data in the conversation
                    # Only include a summary for OpenAI's context
                    if function_name == "generate_image" and tool_result.get("success"):
                        # Create a lightweight version for OpenAI conversation history
                        lightweight_result = {
                            "success": tool_result.get("success"),
                            "message": tool_result.get("message"),
                            "prompt": tool_result.get("prompt"),
                            "model": tool_result.get("model"),
                            "size": tool_result.get("size"),
                            "image_generated": True
                            # Explicitly exclude image_data to prevent context overflow
                        }
                        tool_result_message = {
                            "tool_call_id": tool_call["id"],
                            "role": "tool",
                            "name": function_name,
                            "content": json.dumps(lightweight_result)
                        }
                    else:
                        # For other tools, include full result
                        tool_result_message = {
                            "tool_call_id": tool_call["id"],
                            "role": "tool",
                            "name": function_name,
                            "content": json.dumps(tool_result)
                        }
                    
                    conversation_messages.append(tool_result_message)
                    
                    yield json.dumps({
                        "tool_executed": function_name,
                        "tool_result": tool_result,
                        "tool_result_message": tool_result_message,
                        "status": "tool_completed"
                    }) + '\n'
                    
                # Continue the loop to send tool results back to OpenAI
                continue
                
        except Exception as e:
            print(f"ERROR: Exception in stream generation: {e}")
            yield json.dumps({"error": str(e), "status": "error"}) + '\n'

    return Response(generate_stream(), mimetype='application/x-ndjson')

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
            "original_length": len(file_content),
            "edited_length": len(edited_content)
        })

    except Exception as e:
        print(f"APPLY_EDIT_ERROR: Exception in /predict_code_edit: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/apply', methods=['POST'])
def apply_edit():
    """
    New endpoint for applying AI-powered edits to files.
    Receives file info and returns edited content.
    """
    data = request.json
    file_name = data.get('file_name')
    file_content = data.get('file_content', '')
    prompt = data.get('prompt')
    tool_arguments = data.get('tool_arguments', {})
    
    print(f"APPLY_EDIT_ENDPOINT: Editing {file_name} with prompt: '{prompt}' (content length: {len(file_content)})")
    
    if not file_name or not prompt:
        return jsonify({"error": "Missing required fields: file_name and prompt"}), 400
    
    # Include file name so AI knows the language/file type
    full_prompt = f"""Edit the file "{file_name}" according to the request: {prompt}

Original file content:
```
{file_content}
```

Return ONLY the complete edited file content with NO markdown formatting, NO ``` wrappers, NO explanations. Just return the raw code exactly as it should appear in the file."""

    try:
        response = client.chat.completions.create(
            model=MODEL,
            messages=[
                {"role": "user", "content": full_prompt}
            ]
        )

        edited_content = response.choices[0].message.content
        print(f"APPLY_EDIT_ENDPOINT: Generated edited content (length: {len(edited_content)})")
        
        # Return the complete edited file content
        return jsonify({
            "edited_content": edited_content,
            "original_length": len(file_content),
            "edited_length": len(edited_content),
            "file_name": file_name
        })

    except Exception as e:
        print(f"APPLY_EDIT_ENDPOINT_ERROR: {e}")
        return jsonify({"error": str(e)}), 500

@app.route('/status', methods=['GET'])
def status():
    """A simple endpoint to check if the server is running."""
    return jsonify({"status": "ok", "message": "Godot AI Backend is running!"})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8000, debug=True) 