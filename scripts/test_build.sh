#!/usr/bin/env bash
# 构建测试脚本 - 验证代码可以编译

set -euo pipefail

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "========================================="
echo "StoryToVideo Build Test"
echo "========================================="
echo ""

cd "$PROJECT_ROOT"

# 1. 检查构建目录
BUILD_DIR="$PROJECT_ROOT/build-test"
if [ "${STV_CLEAN_BUILD:-0}" = "1" ]; then
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

# 2. 配置（不构建 App，避免 Qt 依赖）
echo "Configuring CMake (without Qt app)..."
cd "$BUILD_DIR"
cmake .. \
    -DSTV_BUILD_APP=OFF \
    -DSTV_BUILD_TESTS=ON \
    -DSTV_USE_SYSTEM_SPDLOG=OFF \
    -DCMAKE_BUILD_TYPE=Debug

if [ $? -ne 0 ]; then
    echo -e "${RED}✗ CMake configuration failed${NC}"
    exit 1
fi
echo -e "${GREEN}✓ CMake configuration succeeded${NC}"
echo ""

# 3. 构建
echo "Building..."
cmake --build . -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Build succeeded${NC}"
echo ""

# 4. 运行单元测试
echo "Running unit tests..."
ctest --output-on-failure

if [ $? -ne 0 ]; then
    echo -e "${RED}✗ Tests failed${NC}"
    exit 1
fi
echo -e "${GREEN}✓ All tests passed${NC}"
echo ""

echo "========================================="
echo -e "${GREEN}Build test complete!${NC}"
echo "Build directory: $BUILD_DIR"
echo "========================================="
