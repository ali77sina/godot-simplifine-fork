# Godot AI Chatbot Backend

This backend handles all OpenAI API communication and tool execution for the Godot AI Chatbot plugin.

## Architecture

### New Flow (Fixed)
1. **Godot** manages UI and message storage/display
2. **Backend** handles all OpenAI API communication 
3. **Backend** executes tools by making HTTP requests to Godot tool server
4. **Backend** streams final responses back to Godot

### Key Components

**Backend (Port 8000):**
- Receives message history from Godot
- Calls OpenAI API with tools
- Executes tools via HTTP requests to Godot
- Streams responses back to Godot

**Godot Tool Server (Port 8001):**
- Built into the chatbot dock
- Receives tool execution requests from backend
- Executes GodotTools functions with editor access
- Returns results to backend

## Setup

1. Install Python dependencies:
   ```bash
   cd backend
   pip install -r requirements.txt
   ```

2. Create `.env` file in backend directory:
   ```
   OPENAI_API_KEY=your_openai_api_key_here
   ```

3. Start the backend:
   ```bash
   cd backend
   python app.py
   ```

4. Enable the plugin in Godot - the tool server starts automatically

## Usage

Once both servers are running:
1. Open the AI Assistant dock in Godot
2. Type a message asking the AI to work with your scene
3. The AI can now inspect, create, modify, and delete nodes
4. All tool execution happens through the backend for better reliability

## Troubleshooting

- Make sure both backend (8000) and tool server (8001) are running
- Check the Godot console for tool server status messages
- Backend logs show detailed OpenAI API communication and tool execution 