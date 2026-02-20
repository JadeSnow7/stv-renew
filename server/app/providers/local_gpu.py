"""
Local GPU Provider - 使用本地 GPU 进行推理
依赖：torch, diffusers, transformers
资源预算：gpu_slots=1, vram_soft_limit=7.5GiB（for 8GB GPU）
"""
import asyncio
import os
from pathlib import Path
from datetime import datetime
import random
from providers.base import BaseProvider
from schemas import (
    StoryboardRequest, StoryboardResponse, Scene,
    ImageGenRequest, ImageGenResponse,
    TtsRequest, TtsResponse,
    ComposeRequest, ComposeResponse
)


class LocalGpuProvider(BaseProvider):
    """本地 GPU Provider - 使用 SD1.5 + CPU TTS"""
    
    def __init__(self):
        self.output_dir = Path(os.getenv("STV_OUTPUT_DIR", "/tmp/stv-output"))
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # GPU 资源管理
        self.gpu_slots = int(os.getenv("STV_GPU_SLOTS", "1"))
        self.vram_soft_limit_gb = float(os.getenv("STV_VRAM_LIMIT_GB", "7.5"))
        
        # 延迟加载模型（避免启动时卡顿）
        self._sd_pipe = None
        self._tts_engine = None
        self._gpu_available = None
        self._gpu_lock = asyncio.Lock()  # 串行化 GPU 访问
        
        print(f"LocalGpuProvider initialized")
        print(f"  Output dir: {self.output_dir}")
        print(f"  GPU slots: {self.gpu_slots}")
        print(f"  VRAM soft limit: {self.vram_soft_limit_gb} GB")
    
    async def _check_gpu_available(self) -> bool:
        """检查 GPU 是否可用"""
        if self._gpu_available is not None:
            return self._gpu_available
        
        try:
            import torch
            self._gpu_available = torch.cuda.is_available()
            if self._gpu_available:
                device_name = torch.cuda.get_device_name(0)
                total_vram = torch.cuda.get_device_properties(0).total_memory / (1024**3)
                print(f"✓ GPU available: {device_name}, VRAM: {total_vram:.1f} GB")
            else:
                print("⚠ GPU not available, will use CPU or mock")
            return self._gpu_available
        except ImportError:
            print("⚠ PyTorch not installed, GPU inference disabled")
            self._gpu_available = False
            return False
    
    async def _load_sd_model(self):
        """加载 Stable Diffusion 模型"""
        if self._sd_pipe is not None:
            return
        
        if not await self._check_gpu_available():
            print("GPU not available, SD model not loaded")
            return
        
        try:
            from diffusers import StableDiffusionPipeline
            import torch
            
            print("Loading Stable Diffusion 1.5 model...")
            
            # 使用 SD 1.5（较小，适合 8GB 显存）
            model_id = os.getenv("STV_SD_MODEL", "runwayml/stable-diffusion-v1-5")
            
            self._sd_pipe = StableDiffusionPipeline.from_pretrained(
                model_id,
                torch_dtype=torch.float16,  # 使用 fp16 节省显存
                safety_checker=None,  # 禁用安全检查器节省显存
            )
            
            self._sd_pipe = self._sd_pipe.to("cuda")
            
            # 启用内存优化
            self._sd_pipe.enable_attention_slicing()
            
            # 如果支持 xformers，启用以提高性能
            try:
                self._sd_pipe.enable_xformers_memory_efficient_attention()
                print("✓ xformers enabled")
            except Exception:
                print("⚠ xformers not available, using default attention")
            
            print("✓ Stable Diffusion model loaded")
            
        except Exception as e:
            print(f"✗ Failed to load SD model: {e}")
            print("  Will fallback to mock image generation")
            self._sd_pipe = None
    
    async def generate_storyboard(self, request: StoryboardRequest) -> StoryboardResponse:
        """生成分镜脚本（使用简单规则，暂不调用 LLM）"""
        # Phase M3 可以集成本地 LLM（如 Llama 2）
        await asyncio.sleep(0.2)
        
        # 简单分割策略：按句子或段落分割
        sentences = request.story_text.replace('!', '.').replace('?', '.').split('.')
        sentences = [s.strip() for s in sentences if s.strip()]
        
        scenes = []
        sentences_per_scene = max(1, len(sentences) // request.scene_count)
        duration_per_scene = request.target_duration / request.scene_count
        
        for i in range(request.scene_count):
            start_idx = i * sentences_per_scene
            end_idx = start_idx + sentences_per_scene if i < request.scene_count - 1 else len(sentences)
            scene_text = ". ".join(sentences[start_idx:end_idx])
            
            # 生成视觉描述（简单提取关键词）
            visual_desc = self._generate_visual_description(scene_text)
            
            scene = Scene(
                scene_number=i,
                narration=scene_text,
                visual_description=visual_desc,
                duration_seconds=duration_per_scene
            )
            scenes.append(scene)
        
        return StoryboardResponse(
            request_id=request.request_id,
            trace_id=request.trace_id,
            scenes=scenes,
            total_duration=request.target_duration
        )
    
    def _generate_visual_description(self, text: str) -> str:
        """简单的视觉描述生成（关键词提取）"""
        # 移除常见停用词，提取内容词
        words = text.lower().split()
        # 简化：只取前几个词作为描述
        key_words = [w for w in words if len(w) > 3][:10]
        return "A scene depicting " + " ".join(key_words[:5])
    
    async def generate_image(self, request: ImageGenRequest) -> ImageGenResponse:
        """使用 Stable Diffusion 生成图像"""
        async with self._gpu_lock:  # 串行化 GPU 访问
            # 尝试加载模型
            if self._sd_pipe is None:
                await self._load_sd_model()
            
            # 如果模型加载失败，使用 mock 生成
            if self._sd_pipe is None:
                return await self._generate_image_mock(request)
            
            # 真实 SD 推理
            try:
                import torch
                
                seed = request.seed if request.seed is not None else random.randint(0, 2**32-1)
                generator = torch.Generator(device="cuda").manual_seed(seed)
                
                print(f"Generating image: {request.prompt[:50]}...")
                
                # 运行推理
                with torch.no_grad():
                    result = self._sd_pipe(
                        prompt=request.prompt,
                        negative_prompt=request.negative_prompt,
                        width=request.width,
                        height=request.height,
                        num_inference_steps=request.num_inference_steps,
                        guidance_scale=request.guidance_scale,
                        generator=generator,
                    )
                
                image = result.images[0]
                
                # 保存图像
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                filename = f"sd_image_{request.request_id}_{timestamp}.png"
                image_path = self.output_dir / filename
                image.save(image_path)
                
                print(f"✓ Image generated: {image_path}")
                
                return ImageGenResponse(
                    request_id=request.request_id,
                    trace_id=request.trace_id,
                    image_path=str(image_path),
                    width=request.width,
                    height=request.height,
                    seed=seed
                )
                
            except Exception as e:
                print(f"✗ SD inference failed: {e}")
                print("  Falling back to mock generation")
                return await self._generate_image_mock(request)
    
    async def _generate_image_mock(self, request: ImageGenRequest) -> ImageGenResponse:
        """后备：生成模拟图像"""
        from PIL import Image, ImageDraw, ImageFont
        
        # 生成一个有渐变的图像
        img = Image.new('RGB', (request.width, request.height), (50, 50, 150))
        draw = ImageDraw.Draw(img)
        
        # 添加文本提示
        try:
            # 尝试绘制提示词前几个字
            text = request.prompt[:30]
            draw.text((10, 10), text, fill=(255, 255, 255))
        except Exception:
            pass
        
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
        """使用 CPU TTS 生成语音（避免显存竞争）"""
        # Phase M3: 集成 piper-tts 或 coqui-tts
        # 当前使用简单的静音音频
        
        await asyncio.sleep(0.3)
        
        import wave
        import struct
        
        sample_rate = 22050
        duration = len(request.text) * 0.05 / request.speed  # 粗略估算
        num_samples = int(sample_rate * duration)
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"tts_audio_{request.request_id}_{timestamp}.wav"
        audio_path = self.output_dir / filename
        
        with wave.open(str(audio_path), 'w') as wav_file:
            wav_file.setnchannels(1)
            wav_file.setsampwidth(2)
            wav_file.setframerate(sample_rate)
            for _ in range(num_samples):
                wav_file.writeframes(struct.pack('h', 0))
        
        print(f"✓ Audio generated (mock): {audio_path}")
        
        return TtsResponse(
            request_id=request.request_id,
            trace_id=request.trace_id,
            audio_path=str(audio_path),
            duration_seconds=duration,
            format="wav"
        )
    
    async def compose_video(self, request: ComposeRequest) -> ComposeResponse:
        """使用 FFmpeg 合成视频（优先 h264_nvenc）"""
        from services.ffmpeg_compose import FFmpegComposer
        
        composer = FFmpegComposer()
        
        # 尝试使用 NVENC
        encoders = ["h264_nvenc", "libx264"]
        for encoder in encoders:
            if await composer.check_encoder_available(encoder):
                print(f"Using encoder: {encoder}")
                break
        
        # 转换 scene 数据
        from services.ffmpeg_compose import SceneAsset as FFmpegSceneAsset
        ffmpeg_scenes = [
            FFmpegSceneAsset(
                image_path=scene.image_path,
                audio_path=scene.audio_path,
                duration_seconds=scene.duration_seconds
            )
            for scene in request.scenes
        ]
        
        success, message = await composer.compose_video(
            scenes=ffmpeg_scenes,
            output_path=request.output_path,
            fps=request.fps,
            bitrate=request.bitrate,
            preferred_encoder=request.codec
        )
        
        if not success:
            raise Exception(f"Video composition failed: {message}")
        
        print(f"✓ Video composed: {request.output_path}")
        print(f"  {message}")
        
        output_path = Path(request.output_path)
        total_duration = sum(scene.duration_seconds for scene in request.scenes)
        size_bytes = output_path.stat().st_size if output_path.exists() else 0
        
        return ComposeResponse(
            request_id=request.request_id,
            trace_id=request.trace_id,
            video_path=str(output_path),
            duration_seconds=total_duration,
            size_bytes=size_bytes
        )
    
    async def cleanup(self):
        """清理 GPU 资源"""
        if self._sd_pipe is not None:
            del self._sd_pipe
            self._sd_pipe = None
        
        if self._tts_engine is not None:
            del self._tts_engine
            self._tts_engine = None
        
        # 清理 CUDA 缓存
        try:
            import torch
            if torch.cuda.is_available():
                torch.cuda.empty_cache()
                print("✓ CUDA cache cleared")
        except ImportError:
            pass
        
        print("LocalGpuProvider cleanup complete")
