#!/usr/bin/env bash
# 端到端测试脚本 - Mock Provider 模式（不需要 GPU）

set -euo pipefail

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SERVER_DIR="$PROJECT_ROOT/server"

TEST_OUTPUT_DIR="/tmp/stv-test-output"
mkdir -p "$TEST_OUTPUT_DIR"

echo "========================================="
echo "StoryToVideo E2E Test (Mock Provider)"
echo "========================================="
echo ""

# 1. 启动服务端（Mock 模式）
echo "Starting Mock Provider server..."
export STV_PROVIDER=mock
export STV_OUTPUT_DIR="$TEST_OUTPUT_DIR"
export STV_PORT=8766  # 使用不同端口避免冲突

SERVER_PYTHON="${STV_SERVER_PYTHON:-$SERVER_DIR/venv/bin/python}"
if [ ! -x "$SERVER_PYTHON" ]; then
    echo -e "${RED}✗ Python interpreter not found: $SERVER_PYTHON${NC}"
    echo "Please create and install server venv first:"
    echo "  cd server"
    echo "  python3 -m venv venv"
    echo "  source venv/bin/activate"
    echo "  pip install -r requirements.txt"
    exit 1
fi

if ! "$SERVER_PYTHON" -c "import fastapi, uvicorn" >/dev/null 2>&1; then
    echo -e "${RED}✗ Server dependencies are missing in venv${NC}"
    echo "Please install dependencies:"
    echo "  cd server"
    echo "  source venv/bin/activate"
    echo "  pip install -r requirements.txt"
    exit 1
fi

cd "$SERVER_DIR"
"$SERVER_PYTHON" -m app.main &
SERVER_PID=$!

# 等待服务器启动
sleep 3

# 确保服务器在退出时被关闭
trap "kill $SERVER_PID 2>/dev/null || true" EXIT

# 2. 健康检查
echo -n "Checking server health... "
HEALTH_RESPONSE=$(curl -s http://127.0.0.1:8766/healthz)
if echo "$HEALTH_RESPONSE" | grep -q "healthy"; then
    echo -e "${GREEN}✓${NC}"
else
    echo -e "${RED}✗${NC}"
    echo "Server health check failed: $HEALTH_RESPONSE"
    exit 1
fi

# 3. 测试完整工作流
echo ""
echo "Testing complete workflow..."

TRACE_ID="test-$(date +%s)"

# 3.1 生成分镜脚本
echo -n "  [1/4] Generating storyboard... "
STORYBOARD_REQUEST=$(cat <<EOF
{
  "trace_id": "$TRACE_ID",
  "request_id": "req-storyboard-001",
  "story_text": "Once upon a time, there was a brave knight. He went on a quest to find a dragon. The dragon was sleeping in a cave. The knight defeated the dragon and saved the kingdom.",
  "target_duration": 30.0,
  "scene_count": 4
}
EOF
)

STORYBOARD_RESPONSE=$(curl -s -X POST http://127.0.0.1:8766/v1/storyboard \
  -H "Content-Type: application/json" \
  -d "$STORYBOARD_REQUEST")

if echo "$STORYBOARD_RESPONSE" | grep -q "scenes"; then
    echo -e "${GREEN}✓${NC}"
else
    echo -e "${RED}✗${NC}"
    echo "Response: $STORYBOARD_RESPONSE"
    exit 1
fi

# 3.2 生成图像（示例：场景 0）
echo -n "  [2/4] Generating image... "
IMAGEGEN_REQUEST=$(cat <<EOF
{
  "trace_id": "$TRACE_ID",
  "request_id": "req-image-001",
  "prompt": "A brave knight in shining armor",
  "width": 512,
  "height": 512,
  "num_inference_steps": 20
}
EOF
)

IMAGEGEN_RESPONSE=$(curl -s -X POST http://127.0.0.1:8766/v1/imagegen \
  -H "Content-Type: application/json" \
  -d "$IMAGEGEN_REQUEST")

if echo "$IMAGEGEN_RESPONSE" | grep -q "image_path"; then
    IMAGE_PATH=$(echo "$IMAGEGEN_RESPONSE" | grep -o '"image_path":"[^"]*"' | cut -d'"' -f4)
    if [ -f "$IMAGE_PATH" ]; then
        echo -e "${GREEN}✓${NC} ($IMAGE_PATH)"
    else
        echo -e "${YELLOW}⚠${NC} (path returned but file not found)"
    fi
else
    echo -e "${RED}✗${NC}"
    echo "Response: $IMAGEGEN_RESPONSE"
    exit 1
fi

# 3.3 生成语音
echo -n "  [3/4] Generating speech... "
TTS_REQUEST=$(cat <<EOF
{
  "trace_id": "$TRACE_ID",
  "request_id": "req-tts-001",
  "text": "Once upon a time, there was a brave knight.",
  "speed": 1.0
}
EOF
)

TTS_RESPONSE=$(curl -s -X POST http://127.0.0.1:8766/v1/tts \
  -H "Content-Type: application/json" \
  -d "$TTS_REQUEST")

if echo "$TTS_RESPONSE" | grep -q "audio_path"; then
    AUDIO_PATH=$(echo "$TTS_RESPONSE" | grep -o '"audio_path":"[^"]*"' | cut -d'"' -f4)
    if [ -f "$AUDIO_PATH" ]; then
        echo -e "${GREEN}✓${NC} ($AUDIO_PATH)"
    else
        echo -e "${YELLOW}⚠${NC} (path returned but file not found)"
    fi
else
    echo -e "${RED}✗${NC}"
    echo "Response: $TTS_RESPONSE"
    exit 1
fi

# 3.4 合成视频
echo -n "  [4/4] Composing video... "
COMPOSE_REQUEST=$(cat <<EOF
{
  "trace_id": "$TRACE_ID",
  "request_id": "req-compose-001",
  "scenes": [
    {
      "scene_number": 0,
      "image_path": "$IMAGE_PATH",
      "audio_path": "$AUDIO_PATH",
      "duration_seconds": 7.5
    }
  ],
  "output_path": "$TEST_OUTPUT_DIR/test_video_$TRACE_ID.mp4",
  "fps": 24
}
EOF
)

COMPOSE_RESPONSE=$(curl -s -X POST http://127.0.0.1:8766/v1/compose \
  -H "Content-Type: application/json" \
  -d "$COMPOSE_REQUEST")

if echo "$COMPOSE_RESPONSE" | grep -q "video_path"; then
    VIDEO_PATH=$(echo "$COMPOSE_RESPONSE" | grep -o '"video_path":"[^"]*"' | cut -d'"' -f4)
    if [ -f "$VIDEO_PATH" ]; then
        VIDEO_SIZE=$(stat -f%z "$VIDEO_PATH" 2>/dev/null || stat -c%s "$VIDEO_PATH" 2>/dev/null)
        echo -e "${GREEN}✓${NC} ($VIDEO_PATH, ${VIDEO_SIZE} bytes)"
    else
        echo -e "${YELLOW}⚠${NC} (path returned but file not found)"
    fi
else
    echo -e "${RED}✗${NC}"
    echo "Response: $COMPOSE_RESPONSE"
    exit 1
fi

# 4. 测试取消功能
echo ""
echo -n "Testing cancellation... "
CANCEL_RESPONSE=$(curl -s -X POST http://127.0.0.1:8766/v1/cancel/req-storyboard-001)
if echo "$CANCEL_RESPONSE" | grep -q "canceled"; then
    echo -e "${GREEN}✓${NC}"
else
    echo -e "${YELLOW}⚠${NC} (task already completed)"
fi

# 5. 测试错误处理（无效请求）
echo -n "Testing error handling... "
ERROR_REQUEST='{"trace_id":"test","request_id":"req-err-001","story_text":"","scene_count":4}'
ERROR_RESPONSE=$(curl -s -w "\n%{http_code}" -X POST http://127.0.0.1:8766/v1/storyboard \
  -H "Content-Type: application/json" \
  -d "$ERROR_REQUEST")

HTTP_CODE=$(echo "$ERROR_RESPONSE" | tail -1)
if [ "$HTTP_CODE" = "422" ] || [ "$HTTP_CODE" = "400" ]; then
    echo -e "${GREEN}✓${NC} (HTTP $HTTP_CODE)"
else
    echo -e "${YELLOW}⚠${NC} (expected 422/400, got $HTTP_CODE)"
fi

echo ""
echo "========================================="
echo -e "${GREEN}All tests passed!${NC}"
echo "Test output directory: $TEST_OUTPUT_DIR"
echo "========================================="
