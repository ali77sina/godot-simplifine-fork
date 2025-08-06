"""
Multimodal Embedding Manager for Godot Project Files
Handles file indexing, multimodal embedding generation using GCP Vertex AI, and user-isolated storage
"""

import os
import json
import hashlib
import time
from typing import Dict, List, Optional, Tuple, Union
from dataclasses import dataclass, asdict
from datetime import datetime
import numpy as np
import faiss
from google.cloud import storage, aiplatform
from project_graph_manager import ProjectGraphManager
from PIL import Image
import base64
import io

@dataclass
class MultimodalEmbedding:
    """Represents a multimodal embedded file with metadata"""
    file_path: str
    content_hash: str
    embedding: List[float]
    file_type: str
    modality: str  # 'text', 'image', 'audio', 'multimodal'
    last_modified: float
    indexed_at: float
    size: int
    metadata: Dict = None
    
class MultimodalEmbeddingManager:
    """Manages multimodal embeddings for Godot project files with user isolation"""
    
    def __init__(self, openai_client, gcp_bucket_name: str, project_root: str, user_id: str, gcp_project_id: str):
        self.openai_client = openai_client
        self.gcp_bucket_name = gcp_bucket_name
        self.project_root = project_root
        self.user_id = user_id
        self.gcp_project_id = gcp_project_id
        
        # Initialize GCP services
        self.storage_client = storage.Client()
        self.bucket = self.storage_client.bucket(gcp_bucket_name)
        
        # Initialize Vertex AI
        aiplatform.init(project=gcp_project_id)
        
        # In-memory embedding store
        self.embeddings: Dict[str, MultimodalEmbedding] = {}
        self.embedding_dimension = 1536  # OpenAI text-embedding-3-small dimension
        
        # FAISS index for fast similarity search
        self.faiss_index = faiss.IndexFlatIP(self.embedding_dimension)
        self.file_paths_list: List[str] = []
        
        # Project graph manager
        self.graph_manager = ProjectGraphManager(self.project_root, self.user_id)
        
        # File types to index with their modalities
        self.indexable_files = {
            # Text files
            '.gd': 'text', '.cs': 'text', '.js': 'text',
            '.tscn': 'text', '.scn': 'text',
            '.tres': 'text', '.res': 'text',
            '.json': 'text', '.cfg': 'text',
            '.md': 'text', '.txt': 'text',
            '.glsl': 'text', '.shader': 'text',
            
            # Godot specific files
            '.godot': 'text',  # Project files
            '.import': 'text',  # Import files
            '.uid': 'text',     # UID files
            
            # Config/project files (files without extensions)
            'gitignore': 'text',      # .gitignore
            'gitattributes': 'text',  # .gitattributes
            'editorconfig': 'text',   # .editorconfig
            
            # Image files
            '.png': 'image', '.jpg': 'image', '.jpeg': 'image',
            '.gif': 'image', '.bmp': 'image', '.webp': 'image',
            '.svg': 'image', '.tga': 'image', '.exr': 'image',
            
            # Audio files
            '.wav': 'audio', '.ogg': 'audio', '.mp3': 'audio',
            '.aac': 'audio', '.flac': 'audio',
            
            # 3D models (as text for now, could be extended)
            '.obj': 'text', '.fbx': 'text', '.gltf': 'text', '.glb': 'text'
        }
        
        self._load_existing_embeddings()
    
    def _get_user_isolation_hash(self) -> str:
        """Generate unique hash for user isolation"""
        machine_id = os.getenv('MACHINE_ID', 'unknown')
        isolation_string = f"{self.user_id}:{machine_id}:{self.project_root}"
        return hashlib.sha256(isolation_string.encode('utf-8')).hexdigest()[:16]
    
    def _get_content_hash(self, content: Union[str, bytes]) -> str:
        """Generate SHA-256 hash of file content"""
        if isinstance(content, str):
            content = content.encode('utf-8')
        return hashlib.sha256(content).hexdigest()
    
    def _get_file_metadata(self, file_path: str) -> Tuple[Union[str, bytes], float, int]:
        """Get file content, modification time, and size"""
        try:
            modality = self._get_file_modality(file_path)
            
            if modality == 'image':
                with open(file_path, 'rb') as f:
                    content = f.read()
            else:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
            
            stat = os.stat(file_path)
            return content, stat.st_mtime, stat.st_size
        except (UnicodeDecodeError, FileNotFoundError):
            return b"" if modality == 'image' else "", 0, 0
    
    def _get_file_modality(self, file_path: str) -> str:
        """Determine the modality of a file"""
        filename = os.path.basename(file_path)
        _, ext = os.path.splitext(file_path)
        
        # Check extension first
        if ext.lower() in self.indexable_files:
            return self.indexable_files[ext.lower()]
        elif filename.startswith('.') and filename[1:] in self.indexable_files:
            # Handle files like .gitignore, .editorconfig
            return self.indexable_files[filename[1:]]
        else:
            return 'text'  # Default to text
    
    def _should_index_file(self, file_path: str) -> bool:
        """Check if file should be indexed"""
        if not os.path.exists(file_path):
            return False
        
        # Get filename and extension
        filename = os.path.basename(file_path)
        _, ext = os.path.splitext(file_path)
        
        # Check extension first
        if ext.lower() in self.indexable_files:
            pass  # Has supported extension
        elif filename.startswith('.') and filename[1:] in self.indexable_files:
            # Handle files like .gitignore, .editorconfig (check name without the dot)
            pass  # Has supported filename
        else:
            return False
        
        # Skip hidden directories (but allow hidden files like .gitignore)
        path_parts = file_path.split(os.sep)
        for part in path_parts[:-1]:  # Check all parts except the filename
            if part.startswith('.'):
                return False
        
        # Skip build/temp directories
        skip_dirs = {'build', 'temp', '__pycache__', '.godot', '.import'}
        if any(skip_dir in file_path for skip_dir in skip_dirs):
            return False
        
        return True
    
    def _generate_multimodal_embedding(self, content: Union[str, bytes], file_path: str, modality: str) -> Optional[List[float]]:
        """Generate multimodal embedding using GCP Vertex AI"""
        try:
            if modality == 'image':
                return self._generate_image_embedding(content, file_path)
            elif modality == 'text':
                return self._generate_text_embedding(content, file_path)
            elif modality == 'audio':
                # For now, convert audio files to text description
                return self._generate_text_embedding(f"Audio file: {file_path}", file_path)
            else:
                return self._generate_text_embedding(str(content), file_path)
                
        except Exception as e:
            print(f"Error generating multimodal embedding for {file_path}: {e}")
            return None
    
    def _generate_text_embedding(self, content: str, file_path: str) -> Optional[List[float]]:
        """Generate text embedding using OpenAI (with future Vertex AI support)"""
        try:
            # Prepare content with context
            embedding_text = f"File: {file_path}\n\nContent:\n{content}"
            
            # Truncate if too long
            if len(embedding_text) > 8000:
                embedding_text = embedding_text[:8000] + "...[truncated]"
            
            # Use OpenAI for now (Vertex AI multimodal embeddings API is still in preview)
            response = self.openai_client.embeddings.create(
                model="text-embedding-3-small",
                input=embedding_text
            )
            
            return response.data[0].embedding
            
        except Exception as e:
            print(f"Error generating text embedding for {file_path}: {e}")
            return None
    
    def _generate_image_embedding(self, image_data: bytes, file_path: str) -> Optional[List[float]]:
        """Generate true multimodal image embedding using GCP Vertex AI"""
        try:
            # Try GCP Vertex AI multimodal embedding first
            try:
                import base64
                from vertexai.vision_models import MultiModalEmbeddingModel
                
                # Convert image to base64
                image_base64 = base64.b64encode(image_data).decode('utf-8')
                
                # Get the multimodal embedding model
                model = MultiModalEmbeddingModel.from_pretrained("multimodalembedding@001")
                
                # Generate embedding from the image
                embedding_response = model.get_embeddings(
                    image=image_data,
                    contextual_text=f"Game asset from Godot project: {os.path.basename(file_path)}"
                )
                
                # Extract image embedding
                if embedding_response.image_embedding:
                    # Pad or truncate to match our dimension (1536)
                    embedding = embedding_response.image_embedding
                    if len(embedding) > self.embedding_dimension:
                        embedding = embedding[:self.embedding_dimension]
                    elif len(embedding) < self.embedding_dimension:
                        embedding.extend([0.0] * (self.embedding_dimension - len(embedding)))
                    
                    print(f"Generated true multimodal embedding for {file_path}")
                    return embedding
                
            except Exception as vertex_error:
                print(f"Vertex AI multimodal failed for {file_path}: {vertex_error}")
                print("Falling back to image analysis + text embedding...")
            
            # Fallback: Enhanced image analysis + text embedding
            image = Image.open(io.BytesIO(image_data))
            width, height = image.size
            mode = image.mode
            
            # Analyze image content more deeply
            file_extension = os.path.splitext(file_path)[1].lower()
            filename = os.path.basename(file_path)
            
            # Determine likely content type from filename and properties
            content_hints = []
            if any(term in filename.lower() for term in ['player', 'character', 'hero']):
                content_hints.append("character sprite")
            elif any(term in filename.lower() for term in ['enemy', 'monster', 'mob']):
                content_hints.append("enemy sprite")
            elif any(term in filename.lower() for term in ['background', 'bg', 'tile']):
                content_hints.append("background or tile texture")
            elif any(term in filename.lower() for term in ['icon', 'ui', 'button']):
                content_hints.append("UI element")
            elif any(term in filename.lower() for term in ['effect', 'particle', 'fx']):
                content_hints.append("visual effect")
            
            # Create rich descriptive text
            image_description = f"""
            Godot game asset: {filename}
            Image properties: {width}x{height} pixels, {mode} color mode, {file_extension} format
            File size: {len(image_data)} bytes
            Likely content: {', '.join(content_hints) if content_hints else 'game asset'}
            
            This is a visual asset from a Godot game project that can be used for sprites, textures, UI elements, or other game graphics.
            """
            
            # Generate text embedding for the rich description
            response = self.openai_client.embeddings.create(
                model="text-embedding-3-small",
                input=image_description.strip()
            )
            
            return response.data[0].embedding
            
        except Exception as e:
            print(f"Error generating image embedding for {file_path}: {e}")
            return None
    
    def _save_embeddings_to_gcp(self):
        """Save embeddings index to GCP with user isolation"""
        try:
            isolation_hash = self._get_user_isolation_hash()
            
            embeddings_data = {
                'embeddings': {path: asdict(emb) for path, emb in self.embeddings.items()},
                'timestamp': time.time(),
                'project_root': self.project_root,
                'user_id': self.user_id,
                'isolation_hash': isolation_hash
            }
            
            # Save to user-specific path
            blob_name = f"users/{self.user_id}/projects/{isolation_hash}/embeddings.json"
            blob = self.bucket.blob(blob_name)
            blob.upload_from_string(json.dumps(embeddings_data), content_type='application/json')
            
            print(f"Saved {len(self.embeddings)} multimodal embeddings to GCP: {blob_name}")
            
        except Exception as e:
            print(f"Error saving embeddings to GCP: {e}")
    
    def _load_existing_embeddings(self):
        """Load existing embeddings from GCP with user isolation"""
        try:
            isolation_hash = self._get_user_isolation_hash()
            blob_name = f"users/{self.user_id}/projects/{isolation_hash}/embeddings.json"
            blob = self.bucket.blob(blob_name)
            
            if blob.exists():
                data = json.loads(blob.download_as_text())
                
                # Verify user ownership
                if data.get('user_id') != self.user_id:
                    print(f"Warning: Embedding ownership mismatch for user {self.user_id}")
                    return
                
                # Reconstruct embeddings
                for path, emb_dict in data.get('embeddings', {}).items():
                    self.embeddings[path] = MultimodalEmbedding(**emb_dict)
                
                # Rebuild FAISS index
                self._rebuild_faiss_index()
                
                print(f"Loaded {len(self.embeddings)} multimodal embeddings for user {self.user_id}")
            else:
                print(f"No existing embeddings found for user {self.user_id}")
                
        except Exception as e:
            print(f"Error loading embeddings from GCP: {e}")
    
    def _rebuild_faiss_index(self):
        """Rebuild FAISS index from current embeddings"""
        if not self.embeddings:
            return
        
        # Clear existing index
        self.faiss_index = faiss.IndexFlatIP(self.embedding_dimension)
        self.file_paths_list = []
        
        # Add all embeddings to index
        embeddings_matrix = []
        for path, emb in self.embeddings.items():
            embeddings_matrix.append(np.array(emb.embedding, dtype=np.float32))
            self.file_paths_list.append(path)
        
        if embeddings_matrix:
            embeddings_array = np.vstack(embeddings_matrix)
            # Normalize for cosine similarity
            faiss.normalize_L2(embeddings_array)
            self.faiss_index.add(embeddings_array)
    
    def index_file(self, file_path: str) -> bool:
        """Index a single file, return True if indexed/updated"""
        if not self._should_index_file(file_path):
            return False
        
        # Get file metadata
        content, mtime, size = self._get_file_metadata(file_path)
        if not content:
            return False
        
        content_hash = self._get_content_hash(content)
        modality = self._get_file_modality(file_path)
        
        # Check if file needs updating
        existing = self.embeddings.get(file_path)
        if existing and existing.content_hash == content_hash:
            return False
        
        # Generate multimodal embedding
        embedding = self._generate_multimodal_embedding(content, file_path, modality)
        if not embedding:
            return False
        
        # Store embedding with multimodal metadata
        file_embedding = MultimodalEmbedding(
            file_path=file_path,
            content_hash=content_hash,
            embedding=embedding,
            file_type=os.path.splitext(file_path)[1],
            modality=modality,
            last_modified=mtime,
            indexed_at=time.time(),
            size=size,
            metadata={'user_id': self.user_id}
        )
        
        self.embeddings[file_path] = file_embedding
        
        # Update FAISS index
        self._rebuild_faiss_index()
        
        print(f"Indexed ({modality}): {file_path}")
        return True
    
    def search_similar_files(self, query: str, k: int = 5, modality_filter: str = None) -> List[Tuple[str, float, str]]:
        """Search for files similar to query text with optional modality filtering"""
        if not self.embeddings:
            return []
        
        # Generate query embedding (always as text)
        query_embedding = self._generate_text_embedding(query, "query")
        if not query_embedding:
            return []
        
        # Search using FAISS
        query_vector = np.array([query_embedding], dtype=np.float32)
        faiss.normalize_L2(query_vector)
        
        # Get more results to allow for filtering
        search_k = min(k * 3, len(self.file_paths_list))
        scores, indices = self.faiss_index.search(query_vector, search_k)
        
        results = []
        for score, idx in zip(scores[0], indices[0]):
            if idx >= 0 and idx < len(self.file_paths_list):
                file_path = self.file_paths_list[idx]
                embedding = self.embeddings[file_path]
                
                # Apply modality filter if specified
                if modality_filter and embedding.modality != modality_filter:
                    continue
                
                results.append((file_path, float(score), embedding.modality))
                
                if len(results) >= k:
                    break
        
        return results
    
    def get_project_summary(self) -> Dict:
        """Get summary of indexed project with modality breakdown"""
        if not self.embeddings:
            return {'total_files': 0, 'file_types': {}, 'modalities': {}, 'total_size': 0}
        
        file_types = {}
        modalities = {}
        total_size = 0
        
        for emb in self.embeddings.values():
            # Count by file type
            file_type = emb.file_type
            file_types[file_type] = file_types.get(file_type, 0) + 1
            
            # Count by modality
            modality = emb.modality
            modalities[modality] = modalities.get(modality, 0) + 1
            
            total_size += emb.size
        
        return {
            'total_files': len(self.embeddings),
            'file_types': file_types,
            'modalities': modalities,
            'total_size': total_size,
            'last_updated': max(emb.indexed_at for emb in self.embeddings.values()) if self.embeddings else 0,
            'user_id': self.user_id
        }
    
    def remove_file(self, file_path: str) -> bool:
        """Remove file from index"""
        if file_path in self.embeddings:
            del self.embeddings[file_path]
            self._rebuild_faiss_index()
            self._save_embeddings_to_gcp()
            return True
        return False
    
    def index_project(self, force_reindex: bool = False) -> Dict[str, int]:
        """Index all files in the project and build relationship graph"""
        stats = {'indexed': 0, 'skipped': 0, 'errors': 0}
        
        # Collect file contents for graph building
        file_contents = {}
        file_metadata = {}
        
        print(f"🔍 Starting project indexing: {self.project_root}")
        for root, dirs, files in os.walk(self.project_root):
            # Skip certain directories
            dirs[:] = [d for d in dirs if not d.startswith('.') and d not in {'build', 'temp', '__pycache__'}]
            
            for file in files:
                file_path = os.path.join(root, file)
                try:
                    if force_reindex:
                        self.embeddings.pop(file_path, None)
                    
                    # Read file content for graph analysis
                    if self._should_index_file(file_path):
                        modality = self._get_file_modality(file_path)
                        content, mtime, size = self._get_file_metadata(file_path)
                        
                        if content:
                            file_contents[file_path] = content
                            file_metadata[file_path] = {
                                'last_modified': mtime,
                                'size': size,
                                'modality': modality
                            }
                    
                    if self.index_file(file_path):
                        stats['indexed'] += 1
                    else:
                        stats['skipped'] += 1
                except Exception as e:
                    print(f"Error indexing {file_path}: {e}")
                    stats['errors'] += 1
        
        # Build project graph
        if file_contents:
            print("🌐 Building project relationship graph...")
            self.graph_manager.build_project_graph(file_contents, file_metadata)
            stats['graph_nodes'] = self.graph_manager.graph.number_of_nodes()
            stats['graph_edges'] = self.graph_manager.graph.number_of_edges()
        
        # Save to GCP after batch indexing
        if stats['indexed'] > 0:
            self._save_embeddings_to_gcp()
        
        return stats
    
    def update_file(self, file_path: str) -> bool:
        """Update embedding for a specific file"""
        return self.index_file(file_path)
    
    def get_connected_files(self, file_path: str, max_depth: int = 2) -> Dict[str, List[str]]:
        """Get files connected to the given file through project relationships"""
        return self.graph_manager.find_connected_files(file_path, max_depth)
    
    def get_central_files(self, top_k: int = 10) -> List[Tuple[str, float]]:
        """Get the most central/important files in the project"""
        return self.graph_manager.get_central_files(top_k)
    
    def get_graph_summary(self) -> Dict:
        """Get a summary of the project graph structure"""
        return self.graph_manager.get_graph_summary()
    
    def search_with_graph_context(self, query: str, k: int = 5, include_connected: bool = True) -> Dict:
        """Enhanced search that includes graph relationship context"""
        # Get regular similarity search results
        similarity_results = self.search_similar_files(query, k)
        
        if not include_connected or not similarity_results:
            return {
                'similarity_results': similarity_results,
                'connected_files': {},
                'central_files': []
            }
        
        # For each result, find connected files
        connected_files = {}
        for file_path, similarity, modality in similarity_results:
            connections = self.get_connected_files(file_path, max_depth=1)
            if connections:
                connected_files[file_path] = connections
        
        # Get central files for context
        central_files = self.get_central_files(5)
        
        return {
            'similarity_results': similarity_results,
            'connected_files': connected_files,
            'central_files': central_files,
            'graph_summary': self.get_graph_summary()
        }