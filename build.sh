#!/bin/bash
set -e

CC65DIR="$(pwd)/../cc65"
PATH="$CC65DIR/bin:$PATH"

# Clean previous build artifacts
rm -rf build

echo "=== NES 2048 Build ==="

# Step 1: Create build directory
echo "[1/6] Creating build directory..."
mkdir -p build

# Step 2: Assemble CHR data
echo "[2/6] Assembling CHR..."
ca65 -t nes -o build/chr.o src/chr.s

# Step 3: Compile game.c → .s → .o
echo "[3/6] Compiling game.c..."
cc65 -t nes -O -I src -o build/game.s src/game.c
ca65 -t nes -o build/game.o build/game.s

# Step 4: Compile main.c → .s → .o
echo "[4/6] Compiling main.c..."
cc65 -t nes -O -I src -o build/main.s src/main.c
ca65 -t nes -o build/main.o build/main.s

# Step 5: Assemble crt0.s → .o
echo "[5/6] Assembling crt0.s..."
ca65 -t nes -o build/crt0.o neslib/crt0.s
echo "      Assembling crt0_ext.s..."
ca65 -t nes -o build/crt0_ext.o src/crt0_ext.s

# Step 6: Link
echo "[6/6] Linking rom.nes..."
ld65 -C src/nes.cfg -o build/fc2048.nes build/crt0.o build/crt0_ext.o build/main.o build/game.o build/chr.o "$CC65DIR/lib/nes.lib"

echo "=== Build complete: build/fc2048.nes ==="
ls -la build/fc2048.nes