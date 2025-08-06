"""
Authentication Manager for Google/GitHub OAuth
Handles user authentication and session management
"""

import os
import json
import secrets
from typing import Dict, Optional
from datetime import datetime, timedelta
from google.auth.transport.requests import Request
from google.oauth2 import id_token
from google_auth_oauthlib.flow import Flow
import requests
import pickle
import threading

class AuthManager:
    """Manages OAuth authentication for Google and GitHub"""
    
    def __init__(self):
        # OAuth configurations from environment
        self.google_client_id = os.getenv('GOOGLE_CLIENT_ID')
        self.google_client_secret = os.getenv('GOOGLE_CLIENT_SECRET')
        self.github_client_id = os.getenv('GITHUB_CLIENT_ID')
        self.github_client_secret = os.getenv('GITHUB_CLIENT_SECRET')
        self.redirect_uri = os.getenv('OAUTH_REDIRECT_URI', 'http://127.0.0.1:8000/auth/callback')
        
        # Session persistence
        self.sessions_file = os.path.join(os.getcwd(), '.auth_sessions.pkl')
        self.sessions_lock = threading.Lock()
        
        # Session storage (in production, use Redis or database)
        self.active_sessions: Dict[str, Dict] = {}
        self.pending_auth: Dict[str, Dict] = {}
        
        # Load existing sessions from disk
        self._load_sessions()
        
        # Google OAuth flow
        if self.google_client_id and self.google_client_secret:
            self.google_flow = Flow.from_client_config(
                {
                    "web": {
                        "client_id": self.google_client_id,
                        "client_secret": self.google_client_secret,
                        "auth_uri": "https://accounts.google.com/o/oauth2/auth",
                        "token_uri": "https://oauth2.googleapis.com/token",
                        "redirect_uris": [self.redirect_uri]
                    }
                },
                scopes=[
                    'openid', 
                    'https://www.googleapis.com/auth/userinfo.email',
                    'https://www.googleapis.com/auth/userinfo.profile'
                ]
            )
            self.google_flow.redirect_uri = self.redirect_uri
    
    def generate_session_token(self) -> str:
        """Generate a secure session token"""
        return secrets.token_urlsafe(32)
    
    def get_google_auth_url(self, machine_id: str) -> str:
        """Get Google OAuth authorization URL"""
        if not hasattr(self, 'google_flow'):
            raise ValueError("Google OAuth not configured")
        
        # Store machine_id for callback
        state = secrets.token_urlsafe(16)
        self.pending_auth[state] = {
            'machine_id': machine_id,
            'provider': 'google',
            'created_at': datetime.now()
        }
        
        auth_url, _ = self.google_flow.authorization_url(
            access_type='offline',
            include_granted_scopes='true',
            state=state
        )
        
        return auth_url
    
    def get_github_auth_url(self, machine_id: str) -> str:
        """Get GitHub OAuth authorization URL"""
        if not self.github_client_id:
            raise ValueError("GitHub OAuth not configured")
        
        state = secrets.token_urlsafe(16)
        self.pending_auth[state] = {
            'machine_id': machine_id,
            'provider': 'github',
            'created_at': datetime.now()
        }
        
        auth_url = (
            f"https://github.com/login/oauth/authorize"
            f"?client_id={self.github_client_id}"
            f"&redirect_uri={self.redirect_uri}"
            f"&scope=user:email"
            f"&state={state}"
        )
        
        return auth_url
    
    def handle_google_callback(self, state: str, code: str) -> Dict:
        """Handle Google OAuth callback"""
        if state not in self.pending_auth:
            raise ValueError("Invalid state parameter")
        
        pending = self.pending_auth.pop(state)
        machine_id = pending['machine_id']
        
        try:
            # Exchange code for token
            self.google_flow.fetch_token(code=code)
            
            # Get user info
            credentials = self.google_flow.credentials
            request = Request()
            id_info = id_token.verify_oauth2_token(
                credentials.id_token, request, self.google_client_id
            )
            
            user_data = {
                'id': id_info['sub'],
                'email': id_info['email'],
                'name': id_info.get('name', id_info['email']),
                'provider': 'google',
                'avatar_url': id_info.get('picture', '')
            }
            
            # Create session
            session_token = self.generate_session_token()
            self.active_sessions[f"{machine_id}:{user_data['id']}"] = {
                'user': user_data,
                'token': session_token,
                'machine_id': machine_id,
                'created_at': datetime.now(),
                'expires_at': datetime.now() + timedelta(days=30)
            }
            
            # Save sessions to disk
            self._save_sessions()
            
            return {
                'success': True,
                'user': user_data,
                'token': session_token
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }
    
    def handle_github_callback(self, state: str, code: str) -> Dict:
        """Handle GitHub OAuth callback"""
        if state not in self.pending_auth:
            raise ValueError("Invalid state parameter")
        
        pending = self.pending_auth.pop(state)
        machine_id = pending['machine_id']
        
        try:
            # Exchange code for access token
            token_response = requests.post(
                'https://github.com/login/oauth/access_token',
                data={
                    'client_id': self.github_client_id,
                    'client_secret': self.github_client_secret,
                    'code': code
                },
                headers={'Accept': 'application/json'}
            )
            
            token_data = token_response.json()
            access_token = token_data.get('access_token')
            
            if not access_token:
                raise ValueError("Failed to get access token")
            
            # Get user info
            user_response = requests.get(
                'https://api.github.com/user',
                headers={
                    'Authorization': f'token {access_token}',
                    'Accept': 'application/json'
                }
            )
            
            user_info = user_response.json()
            
            # Get user email if not public
            email = user_info.get('email')
            if not email:
                email_response = requests.get(
                    'https://api.github.com/user/emails',
                    headers={
                        'Authorization': f'token {access_token}',
                        'Accept': 'application/json'
                    }
                )
                emails = email_response.json()
                primary_email = next((e['email'] for e in emails if e['primary']), None)
                email = primary_email or f"{user_info['login']}@github.local"
            
            user_data = {
                'id': str(user_info['id']),
                'email': email,
                'name': user_info.get('name') or user_info['login'],
                'provider': 'github',
                'avatar_url': user_info.get('avatar_url', '')
            }
            
            # Create session
            session_token = self.generate_session_token()
            self.active_sessions[f"{machine_id}:{user_data['id']}"] = {
                'user': user_data,
                'token': session_token,
                'machine_id': machine_id,
                'created_at': datetime.now(),
                'expires_at': datetime.now() + timedelta(days=30)
            }
            
            # Save sessions to disk
            self._save_sessions()
            
            return {
                'success': True,
                'user': user_data,
                'token': session_token
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e)
            }
    
    def verify_session(self, machine_id: str, token: str) -> Optional[Dict]:
        """Verify and return user session"""
        try:
            # Clean up expired sessions
            self._cleanup_expired_sessions()
            
            # Find session by machine_id and token
            for session_key, session_data in self.active_sessions.items():
                if (session_data['machine_id'] == machine_id and 
                    session_data['token'] == token and
                    session_data['expires_at'] > datetime.now()):
                    return session_data['user']
            
            return None
            
        except Exception as e:
            print(f"Error verifying session: {e}")
            return None
    
    def get_user_by_machine_id(self, machine_id: str) -> Optional[Dict]:
        """Get authenticated user for a machine"""
        try:
            self._cleanup_expired_sessions()
            
            for session_key, session_data in self.active_sessions.items():
                if (session_data['machine_id'] == machine_id and
                    session_data['expires_at'] > datetime.now()):
                    return {
                        'user': session_data['user'],
                        'token': session_data['token']
                    }
            
            return None
            
        except Exception as e:
            print(f"Error getting user by machine ID: {e}")
            return None
    
    def logout_user(self, machine_id: str, user_id: str = None) -> bool:
        """Logout user from machine"""
        try:
            sessions_changed = False
            if user_id:
                session_key = f"{machine_id}:{user_id}"
                if session_key in self.active_sessions:
                    del self.active_sessions[session_key]
                    sessions_changed = True
            else:
                # Logout all sessions for this machine
                keys_to_remove = [
                    key for key, session in self.active_sessions.items()
                    if session['machine_id'] == machine_id
                ]
                for key in keys_to_remove:
                    del self.active_sessions[key]
                sessions_changed = len(keys_to_remove) > 0
            
            # Save sessions to disk if changed
            if sessions_changed:
                self._save_sessions()
                
            return sessions_changed
            
        except Exception as e:
            print(f"Error logging out user: {e}")
            return False
    
    def _cleanup_expired_sessions(self):
        """Remove expired sessions"""
        try:
            current_time = datetime.now()
            expired_keys = [
                key for key, session in self.active_sessions.items()
                if session['expires_at'] <= current_time
            ]
            
            sessions_changed = False
            for key in expired_keys:
                del self.active_sessions[key]
                sessions_changed = True
                
            # Also cleanup old pending auth
            expired_pending = [
                state for state, data in self.pending_auth.items()
                if (current_time - data['created_at']).total_seconds() > 3600  # 1 hour
            ]
            
            for state in expired_pending:
                del self.pending_auth[state]
            
            # Save sessions to disk if any were removed
            if sessions_changed:
                self._save_sessions()
                
        except Exception as e:
            print(f"Error cleaning up sessions: {e}")
    
    def get_session_stats(self) -> Dict:
        """Get authentication statistics"""
        self._cleanup_expired_sessions()
        
        providers = {}
        for session in self.active_sessions.values():
            provider = session['user']['provider']
            providers[provider] = providers.get(provider, 0) + 1
        
        return {
            'active_sessions': len(self.active_sessions),
            'pending_auth': len(self.pending_auth),
            'providers': providers
        }
    
    def _load_sessions(self):
        """Load persistent sessions from disk"""
        try:
            if os.path.exists(self.sessions_file):
                with open(self.sessions_file, 'rb') as f:
                    data = pickle.load(f)
                    # Only load non-expired sessions
                    current_time = datetime.now()
                    for session_key, session_data in data.items():
                        # Handle both old format (string dates) and new format (datetime objects)
                        expires_at = session_data.get('expires_at')
                        if isinstance(expires_at, str):
                            # Convert string to datetime for backward compatibility
                            try:
                                expires_at = datetime.fromisoformat(expires_at)
                                session_data['expires_at'] = expires_at
                            except:
                                continue  # Skip invalid dates
                        
                        if expires_at and expires_at > current_time:
                            self.active_sessions[session_key] = session_data
                    
                    print(f"Loaded {len(self.active_sessions)} active sessions from disk")
        except Exception as e:
            print(f"Error loading sessions: {e}")
            self.active_sessions = {}
    
    def _save_sessions(self):
        """Save persistent sessions to disk"""
        try:
            with self.sessions_lock:
                # Only save non-expired sessions
                current_time = datetime.now()
                sessions_to_save = {}
                for session_key, session_data in self.active_sessions.items():
                    if session_data.get('expires_at') and session_data['expires_at'] > current_time:
                        sessions_to_save[session_key] = session_data
                
                with open(self.sessions_file, 'wb') as f:
                    pickle.dump(sessions_to_save, f)
        except Exception as e:
            print(f"Error saving sessions: {e}")