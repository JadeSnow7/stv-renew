#!/usr/bin/env bash
# 依赖探测脚本 - 检测 StoryToVideo Renew 所需的运行时和构建依赖

set -uo pipefail

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

PASSED=0
FAILED=0
WARNINGS=0

echo "========================================="
echo "StoryToVideo Renew - Dependency Check"
echo "========================================="
echo ""

check_command() {
    local cmd=$1
    local name=$2
    local install_hint=$3
    
    if command -v "$cmd" &> /dev/null; then
        echo -e "${GREEN}✓ ${name} found${NC}"
        PASSED=$((PASSED + 1))
        return 0
    else
        echo -e "${RED}✗ ${name} not found${NC}"
        echo -e "  Install: ${install_hint}"
        FAILED=$((FAILED + 1))
        return 1
    fi
}

check_library() {
    local name=$1
    local test_cmd=$2
    local install_hint=$3
    
    if eval "$test_cmd" &> /dev/null; then
        echo -e "${GREEN}✓ ${name} found${NC}"
        PASSED=$((PASSED + 1))
        return 0
    else
        echo -e "${RED}✗ ${name} not found${NC}"
        echo -e "  Install: ${install_hint}"
        FAILED=$((FAILED + 1))
        return 1
    fi
}

check_optional() {
    local name=$1
    local test_cmd=$2
    local hint=$3
    
    if eval "$test_cmd" &> /dev/null; then
        echo -e "${GREEN}✓ ${name} available${NC}"
        PASSED=$((PASSED + 1))
        return 0
    else
        echo -e "${YELLOW}⚠ ${name} not available (optional)${NC}"
        echo -e "  Note: ${hint}"
        WARNINGS=$((WARNINGS + 1))
        return 1
    fi
}

echo "--- Build Tools ---"
check_command "cmake" "CMake" "sudo apt install cmake"
check_command "g++" "C++ Compiler" "sudo apt install build-essential"
check_command "python3" "Python 3" "sudo apt install python3"
echo ""

echo "--- Required Libraries ---"
check_library "libcurl" "pkg-config --exists libcurl" "sudo apt install libcurl4-openssl-dev"
echo ""

echo "--- Qt6 (for GUI application) ---"
if check_optional "Qt6::Quick" "pkg-config --exists Qt6Quick" "Required for GUI. Install: sudo apt install qt6-base-dev qt6-declarative-dev"; then
    QT_VERSION=$(pkg-config --modversion Qt6Core 2>/dev/null || echo "unknown")
    echo "  Qt version: ${QT_VERSION}"
fi
echo ""

echo "--- Python Environment ---"
if check_command "python3" "Python 3" "sudo apt install python3"; then
    PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}')
    echo "  Python version: ${PYTHON_VERSION}"
fi

if python3 -c "import venv" &> /dev/null; then
    echo -e "${GREEN}✓ Python venv module found${NC}"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}✗ Python venv module not found${NC}"
    echo -e "  Install: sudo apt install python3-venv"
    FAILED=$((FAILED + 1))
fi
echo ""

echo "--- CUDA & GPU ---"
if check_optional "nvidia-smi" "nvidia-smi" "Required for GPU inference. Install NVIDIA driver."; then
    GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)
    GPU_MEM=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null | head -1)
    DRIVER_VERSION=$(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -1)
    echo "  GPU: ${GPU_NAME}"
    echo "  VRAM: ${GPU_MEM} MiB"
    echo "  Driver: ${DRIVER_VERSION}"
    
    if command -v nvcc &> /dev/null; then
        CUDA_VERSION=$(nvcc --version | grep "release" | awk '{print $5}' | cut -d',' -f1)
        echo -e "${GREEN}✓ CUDA Toolkit: ${CUDA_VERSION}${NC}"
    else
        echo -e "${YELLOW}⚠ CUDA Toolkit (nvcc) not found${NC}"
        echo "  Not required for runtime, but useful for development"
    fi
fi
echo ""

echo "--- FFmpeg with NVENC ---"
if check_command "ffmpeg" "FFmpeg" "sudo apt install ffmpeg"; then
    FFMPEG_VERSION=$(ffmpeg -version 2>&1 | head -1 | awk '{print $3}')
    echo "  FFmpeg version: ${FFMPEG_VERSION}"
    
    if ffmpeg -hide_banner -encoders 2>/dev/null | grep -q "h264_nvenc"; then
        echo -e "${GREEN}✓ h264_nvenc encoder available${NC}"
    else
        echo -e "${YELLOW}⚠ h264_nvenc not available, will fallback to libx264${NC}"
        echo "  For NVENC support, install NVIDIA driver and ffmpeg with NVENC enabled"
        WARNINGS=$((WARNINGS + 1))
    fi
fi
echo ""

echo "========================================="
echo "Summary:"
echo -e "${GREEN}Passed: ${PASSED}${NC}"
if [ $WARNINGS -gt 0 ]; then
    echo -e "${YELLOW}Warnings: ${WARNINGS}${NC}"
fi
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed: ${FAILED}${NC}"
    echo ""
    echo "Some required dependencies are missing."
    echo "You can still build without Qt (set -DSTV_BUILD_APP=OFF)."
    echo "For full functionality, install missing dependencies."
    exit 1
else
    echo ""
    echo "All essential dependencies are available!"
    if [ $WARNINGS -gt 0 ]; then
        echo "Optional components have warnings (see above)."
    fi
    exit 0
fi
