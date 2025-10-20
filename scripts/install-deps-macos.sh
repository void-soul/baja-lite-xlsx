#!/bin/bash

echo "============================================"
echo "Baja-XLSX Dependencies Installation (macOS)"
echo "============================================"
echo ""

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "[ERROR] Homebrew not found"
    echo "Please install Homebrew first:"
    echo ""
    echo '  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
    echo ""
    exit 1
fi

echo "[1/3] Updating Homebrew..."
brew update

echo ""
echo "[2/3] Installing xlnt..."
brew install xlnt

if [ $? -ne 0 ]; then
    echo "[ERROR] Failed to install xlnt"
    exit 1
fi

echo ""
echo "[3/3] Installing libzip..."
brew install libzip

if [ $? -ne 0 ]; then
    echo "[ERROR] Failed to install libzip"
    exit 1
fi

echo ""
echo "============================================"
echo "Dependencies installed successfully!"
echo "============================================"
echo ""
echo "You can now build the module with:"
echo "  npm install"
echo ""


