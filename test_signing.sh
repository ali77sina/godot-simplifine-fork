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