#!/bin/bash

echo "============================================"
echo "Baja-XLSX Dependencies Installation (Linux)"
echo "============================================"
echo ""

# Detect distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    echo "[ERROR] Cannot detect Linux distribution"
    exit 1
fi

echo "[1/4] Installing build tools..."
case $DISTRO in
    ubuntu|debian)
        sudo apt-get update
        sudo apt-get install -y build-essential python3 cmake git libzip-dev
        ;;
    fedora|rhel|centos)
        sudo dnf install -y gcc-c++ python3 cmake git libzip-devel
        ;;
    arch|manjaro)
        sudo pacman -Sy --noconfirm base-devel python cmake git libzip
        ;;
    *)
        echo "[WARNING] Unsupported distribution: $DISTRO"
        echo "Please install manually: build-essential, python3, cmake, git, libzip-dev"
        ;;
esac

if [ $? -ne 0 ]; then
    echo "[ERROR] Failed to install build tools"
    exit 1
fi

echo ""
echo "[2/4] Checking for xlnt..."
if ! ldconfig -p | grep -q libxlnt; then
    echo "xlnt not found. Installing from source..."
    
    cd /tmp
    if [ ! -d "xlnt" ]; then
        git clone https://github.com/tfussell/xlnt.git
    fi
    
    cd xlnt
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc)
    sudo make install
    
    if [ $? -ne 0 ]; then
        echo "[ERROR] Failed to build xlnt"
        exit 1
    fi
else
    echo "xlnt already installed"
fi

echo ""
echo "[3/4] Running ldconfig..."
sudo ldconfig

echo ""
echo "[4/4] Verifying installation..."
if ldconfig -p | grep -q libxlnt && ldconfig -p | grep -q libzip; then
    echo "✓ All dependencies verified"
else
    echo "[WARNING] Some dependencies may not be properly installed"
fi

echo ""
echo "============================================"
echo "Dependencies installed successfully!"
echo "============================================"
echo ""
echo "You can now build the module with:"
echo "  npm install"
echo ""


