#!/bin/bash

# macOS Code Signing Setup Script for Godot
# This script helps configure your Godot export for proper macOS distribution

echo "🍎 macOS Code Signing Setup for Godot"
echo "======================================"

# Check if we have the Developer ID certificate
echo "Checking for Developer ID certificate..."
CERT_NAME=$(security find-identity -v -p codesigning | grep "Developer ID Application" | sed 's/.*"\(.*\)".*/\1/')

if [ -z "$CERT_NAME" ]; then
    echo "❌ No Developer ID certificate found!"
    echo "Please install your Developer ID certificate from Apple Developer portal"
    exit 1
else
    echo "✅ Found certificate: $CERT_NAME"
fi

# Extract team ID from certificate
TEAM_ID=$(echo "$CERT_NAME" | grep -o '([A-Z0-9]\{10\})' | tr -d '()')
echo "📋 Team ID: $TEAM_ID"

# Check codesign availability
if ! command -v codesign &> /dev/null; then
    echo "❌ codesign tool not found! Please install Xcode Command Line Tools"
    exit 1
else
    echo "✅ codesign tool available"
fi

# Check notarytool availability  
if ! command -v xcrun &> /dev/null || ! xcrun notarytool --help &> /dev/null; then
    echo "⚠️  notarytool not available - notarization will be skipped"
    NOTARY_AVAILABLE=false
else
    echo "✅ notarytool available for notarization"
    NOTARY_AVAILABLE=true
fi

echo ""
echo "📝 Godot Export Configuration:"
echo "==============================="
echo "For your Godot macOS export preset, use these settings:"
echo ""
echo "Code Signing:"
echo "  - Code Signing Tool: 'codesign' (option 3)"
echo "  - Code Signing Identity: '$CERT_NAME'"
echo "  - Apple Team ID: '$TEAM_ID'"
echo ""
echo "Distribution:"
echo "  - Distribution Type: 'Distribution' (for outside Mac App Store)"
echo "  - Bundle Identifier: com.simplifine.your-app-name"
echo ""

if [ "$NOTARY_AVAILABLE" = true ]; then
    echo "Notarization:"
    echo "  - Notarization Tool: 'notarytool'"
    echo "  - You'll need to set up Apple ID credentials for notarization"
    echo ""
fi

echo "🔧 Next Steps:"
echo "=============="
echo "1. Open Godot Editor"
echo "2. Go to Project > Export"
echo "3. Add/Edit macOS export preset"
echo "4. Configure the settings shown above"
echo "5. Test export with a simple project"
echo ""

# Create a simple test script for manual verification
cat > test_signing.sh << 'EOF'
#!/bin/bash

# Test script to verify code signing works
if [ $# -eq 0 ]; then
    echo "Usage: ./test_signing.sh <path-to-your-app.app>"
    exit 1
fi

APP_PATH="$1"

echo "Testing code signature for: $APP_PATH"
echo "======================================="

# Verify the signature
codesign -v -v "$APP_PATH"
if [ $? -eq 0 ]; then
    echo "✅ Code signature is valid"
else
    echo "❌ Code signature verification failed"
    exit 1
fi

# Check what certificate was used
echo ""
echo "Certificate details:"
codesign -d -v "$APP_PATH"

# Check if it's properly signed for distribution
echo ""
echo "Gatekeeper assessment:"
spctl -a -v "$APP_PATH"
if [ $? -eq 0 ]; then
    echo "✅ App will be accepted by Gatekeeper"
else
    echo "⚠️  App may be rejected by Gatekeeper"
fi
EOF

chmod +x test_signing.sh

echo "📋 Created test_signing.sh script to verify your signed apps"
echo ""
echo "🚀 Ready to export! Your certificate is properly installed."
echo "   After exporting from Godot, run: ./test_signing.sh YourApp.app"