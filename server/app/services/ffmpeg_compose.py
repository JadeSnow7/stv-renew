"""
FFmpeg 视频合成服务
支持 NVENC 硬件加速，失败时自动回退到 libx264
"""
import subprocess
import asyncio
from pathlib import Path
from typing import List, Optional, Tuple
from dataclasses import dataclass


@dataclass
class SceneAsset:
    """场景资源"""
    image_path: str
    audio_path: str
    duration_seconds: float


class FFmpegComposer:
    """FFmpeg 视频合成器"""
    
    def __init__(self):
        self.encoder_priority = ["h264_nvenc", "libx264"]  # 编码器优先级
        self._available_encoders = None
    
    async def check_encoder_available(self, encoder: str) -> bool:
        """检查编码器是否可用"""
        if self._available_encoders is None:
            self._available_encoders = await self._probe_encoders()
        return encoder in self._available_encoders
    
    async def _probe_encoders(self) -> List[str]:
        """探测可用的编码器"""
        try:
            result = subprocess.run(
                ["ffmpeg", "-hide_banner", "-encoders"],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            encoders = []
            for line in result.stdout.split('\n'):
                if 'h264_nvenc' in line:
                    encoders.append('h264_nvenc')
                if 'libx264' in line:
                    encoders.append('libx264')
            
            return encoders
        except Exception as e:
            print(f"Failed to probe encoders: {e}")
            return ["libx264"]  # 保底
    
    async def compose_video(
        self,
        scenes: List[SceneAsset],
        output_path: str,
        fps: int = 24,
        bitrate: str = "2M",
        preferred_encoder: Optional[str] = None
    ) -> Tuple[bool, str]:
        """
        合成视频
        
        Returns:
            (success, message): 是否成功和消息
        """
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        # 选择编码器
        encoder = preferred_encoder
        if encoder is None:
            for enc in self.encoder_priority:
                if await self.check_encoder_available(enc):
                    encoder = enc
                    break
        
        if encoder is None:
            return False, "No suitable encoder found"
        
        print(f"Using encoder: {encoder}")
        
        # Phase 3 实现：完整的场景拼接逻辑
        # 当前简化实现：创建一个基础视频
        
        total_duration = sum(scene.duration_seconds for scene in scenes)
        
        cmd = [
            "ffmpeg", "-y",
            "-f", "lavfi",
            "-i", f"color=c=black:s=512x512:d={total_duration}",
            "-c:v", encoder,
            "-b:v", bitrate,
            "-pix_fmt", "yuv420p",
            "-r", str(fps),
            "-t", str(total_duration),
            str(output_path)
        ]
        
        try:
            proc = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            
            stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=300)
            
            if proc.returncode == 0:
                return True, f"Video composed successfully with {encoder}"
            else:
                error_msg = stderr.decode('utf-8', errors='ignore')
                print(f"FFmpeg error: {error_msg}")
                
                # 如果是 NVENC 失败，尝试回退
                if encoder == "h264_nvenc" and "libx264" in self.encoder_priority:
                    print("NVENC failed, retrying with libx264...")
                    cmd[-5] = "libx264"
                    
                    proc = await asyncio.create_subprocess_exec(
                        *cmd,
                        stdout=asyncio.subprocess.PIPE,
                        stderr=asyncio.subprocess.PIPE
                    )
                    stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=300)
                    
                    if proc.returncode == 0:
                        return True, "Video composed successfully with libx264 (fallback)"
                    else:
                        return False, f"FFmpeg failed: {stderr.decode()[:200]}"
                
                return False, f"FFmpeg failed: {error_msg[:200]}"
        
        except asyncio.TimeoutError:
            return False, "FFmpeg timeout (>300s)"
        except FileNotFoundError:
            return False, "FFmpeg not found in PATH"
        except Exception as e:
            return False, f"Unexpected error: {str(e)}"
