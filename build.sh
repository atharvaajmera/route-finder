#!/bin/bash

# Build script for Linux/macOS

echo "=================================="
echo "Test Centre Allotter - Build Script"
echo "=================================="
echo ""

# Check if g++ is available
echo "Checking for g++..."
if ! command -v g++ &> /dev/null
then
    echo "ERROR: g++ not found!"
    echo "Please install g++ (apt-get install g++ or brew install gcc)"
    exit 1
fi

echo "Found g++ at: $(which g++)"
echo ""

# Navigate to backend directory
cd backend

echo "Compiling backend server..."
echo "Command: g++ -std=c++17 main.cpp -o server -I.. -lcurl -lpthread"
echo ""

# Compile
if g++ -std=c++17 main.cpp -o server -I.. -lcurl -lpthread; then
    echo "Compilation successful!"
    echo ""
    
    # Check if executable was created
    if [ -f "server" ]; then
        echo "✓ server created successfully"
        chmod +x server
        fileSize=$(du -k "server" | cut -f1)
        echo "  Size: ${fileSize} KB"
    else
        echo "ERROR: server not found!"
        cd ..
        exit 1
    fi
else
    echo "COMPILATION FAILED!"
    cd ..
    exit 1
fi

echo ""
echo "=================================="
echo "Build Complete!"
echo "=================================="
echo ""
echo "To run the server:"
echo "  cd backend"
echo "  ./server"
echo ""
echo "Then open frontend/index.html in your browser"
echo ""

cd ..
