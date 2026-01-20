#!/bin/bash
# ==============================================================================
# VT-2R - EMU AUDIO
# ビルドスクリプト (macOS)
# ==============================================================================

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "========================================"
echo "  VT-2R - Build Script"
echo "  EMU AUDIO"
echo "========================================"
echo ""

# ビルドタイプ
BUILD_TYPE=${1:-Release}
echo "Build Type: $BUILD_TYPE"
echo ""

# ビルドディレクトリ作成
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# CMake設定
# Try local bundled cmake first, then fall back to system cmake
LOCAL_CMAKE="$PROJECT_DIR/cmake-3.28.1-macos-universal/CMake.app/Contents/bin/cmake"
if [ -f "$LOCAL_CMAKE" ]; then
    CMAKE_BIN="$LOCAL_CMAKE"
else
    CMAKE_BIN="cmake"
fi

echo "[1/3] Configuring with CMake..."
"$CMAKE_BIN" .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"

echo ""

# ビルド実行
echo "[2/3] Building..."
"$CMAKE_BIN" --build . --config "$BUILD_TYPE" -j$(sysctl -n hw.ncpu)

echo ""

# 結果表示
echo "[3/3] Build Complete!"
echo ""
echo "Built plugins:"
find "$BUILD_DIR" -name "*.vst3" -o -name "*.component" -o -name "*.app" 2>/dev/null | head -20

echo ""
echo "========================================"
echo "  Build finished successfully!"
echo "========================================"
