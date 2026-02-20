"""
Mock Provider - 用于测试和保底运行
快速返回模拟数据，不依赖 GPU/外部服务
"""
import asyncio
import random
import os
from pathlib import Path
from datetime import datetime
from providers.base import BaseProvider
from schemas import (
    StoryboardRequest, StoryboardResponse, Scene,
    ImageGenRequest, ImageGenResponse,
    TtsRequest, TtsResponse,
    ComposeRequest, ComposeResponse
)


class MockProvider(BaseProvider):
    """Mock Provider - 快速返回模拟数据"""
    
    def __init__(self):
        # 确保输出目录存在
        self.output_dir = Path(os.getenv("STV_OUTPUT_DIR", "/tmp/stv-output"))
        self.output_dir.mkdir(parents=True, exist_ok=True)
        print(f"MockProvider initialized, output dir: {self.output_dir}")
    
    async def generate_storyboard(self, request: StoryboardRequest) -> StoryboardResponse:
        """生成模拟分镜脚本"""
        # 模拟延迟
        await asyncio.sleep(0.1)
        
        scenes = []
        duration_per_scene = request.target_duration / request.scene_count
        
        for i in range(request.scene_count):
            scene = Scene(
                scene_number=i,
                narration=f"Scene {i+1}: {request.story_text[:50]}...",
                visual_description=f"A beautiful scene showing {request.story_text[:30]}",
                duration_seconds=duration_per_scene
            )
            scenes.append(scene)
        
        return StoryboardResponse(
            request_id=request.request_id,
            trace_id=request.trace_id,
            scenes=scenes,
            total_duration=request.target_duration
        )
    
    async def generate_image(self, request: ImageGenRequest) -> ImageGenResponse:
        """生成模拟图像（纯色 PNG）"""
        await asyncio.sleep(0.2)
        
        # 生成一个简单的纯色图像
        from PIL import Image
        
        # 使用 prompt 的哈希值生成颜色
        color_hash = hash(request.prompt) % 0xFFFFFF
        r = (color_hash >> 16) & 0xFF
        g = (color_hash >> 8) & 0xFF
        b = color_hash & 0xFF
        
        img = Image.new('RGB', (request.width, request.height), (r, g, b))
        
        # 保存图像
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"mock_image_{request.request_id}_{timestamp}.png"
        image_path = self.output_dir / filename
        img.save(image_path)
        
        seed = request.seed if request.seed is not None else random.randint(0, 2**32-1)
        
        return ImageGenResponse(
            request_id=request.request_id,
            trace_id=request.trace_id,
            image_path=str(image_path),
            width=request.width,
            height=request.height,
            seed=seed
        )
    
    async def generate_speech(self, request: TtsRequest) -> TtsResponse:
        """生成模拟音频（静音 WAV）"""
        await asyncio.sleep(0.15)
        
        # 生成一个简单的静音音频文件
        import wave
        import struct
        
        sample_rate = 22050
        duration = len(request.text) * 0.05  # 粗略估算：每个字符 50ms
        num_samples = int(sample_rate * duration)
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"mock_audio_{request.request_id}_{timestamp}.wav"
        audio_path = self.output_dir / filename
        
        with wave.open(str(audio_path), 'w') as wav_file:
            wav_file.setnchannels(1)  # Mono
            wav_file.setsampwidth(2)  # 16-bit
            wav_file.setframerate(sample_rate)
            
            # 写入静音数据
            for _ in range(num_samples):
                wav_file.writeframes(struct.pack('h', 0))
        
        return TtsResponse(
            request_id=request.request_id,
            trace_id=request.trace_id,
            audio_path=str(audio_path),
            duration_seconds=duration,
            format="wav"
        )
    
    async def compose_video(self, request: ComposeRequest) -> ComposeResponse:
        """生成模拟视频（使用 FFmpeg）"""
        await asyncio.sleep(0.3)
        
        # 计算总时长
        total_duration = sum(scene.duration_seconds for scene in request.scenes)
        
        # 使用 FFmpeg 合成视频
        # 这里使用真实的 FFmpeg，但输入是 mock 的图像和音频
        import subprocess
        
        output_path = Path(request.output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        # 创建一个简单的黑色视频
        # ffmpeg -f lavfi -i color=c=black:s=512x512:d={duration} -c:v libx264 -t {duration} output.mp4
        cmd = [
            "ffmpeg", "-y",
            "-f", "lavfi",
            "-i", f"color=c=black:s=512x512:d={total_duration}",
            "-c:v", "libx264",
            "-pix_fmt", "yuv420p",
            "-t", str(total_duration),
            str(output_path)
        ]
        
        try:
            result = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
                check=True
            )
        except subprocess.CalledProcessError as e:
            print(f"FFmpeg error: {e.stderr.decode()}")
            # 创建一个空文件作为后备
            output_path.touch()
        except FileNotFoundError:
            print("FFmpeg not found, creating empty file")
            output_path.touch()
        
        # 获取文件大小
        size_bytes = output_path.stat().st_size if output_path.exists() else 0
        
        return ComposeResponse(
            request_id=request.request_id,
            trace_id=request.trace_id,
            video_path=str(output_path),
            duration_seconds=total_duration,
            size_bytes=size_bytes
        )
    
    async def cleanup(self):
        """清理资源"""
        print("MockProvider cleanup complete")
