"""
Pydantic schemas for API requests and responses
统一字段：trace_id、request_id、retryable、error_code、user_message、internal_message
"""
from pydantic import BaseModel, Field
from typing import Optional, List, Dict, Any
from enum import Enum


# ========== Common Models ==========

class ErrorResponse(BaseModel):
    """统一错误响应"""
    error_code: str = Field(..., description="错误代码")
    user_message: str = Field(..., description="面向用户的错误信息")
    internal_message: str = Field(..., description="内部调试信息")
    retryable: bool = Field(..., description="是否可重试")
    trace_id: str = Field(..., description="跟踪 ID")
    request_id: Optional[str] = Field(None, description="请求 ID")
    details: Dict[str, Any] = Field(default_factory=dict, description="额外详情")


class HealthResponse(BaseModel):
    """健康检查响应"""
    status: str = Field(..., description="服务状态: healthy/degraded/unhealthy")
    provider: str = Field(..., description="当前 provider 类型")
    version: str = Field(..., description="API 版本")


# ========== Storyboard ==========

class Scene(BaseModel):
    """单个场景"""
    scene_number: int = Field(..., description="场景编号（从 0 开始）")
    narration: str = Field(..., description="旁白文本")
    visual_description: str = Field(..., description="视觉描述（用于图像生成）")
    duration_seconds: float = Field(..., description="场景时长（秒）")


class StoryboardRequest(BaseModel):
    """分镜脚本生成请求"""
    trace_id: str = Field(..., description="跟踪 ID")
    request_id: str = Field(..., description="请求 ID（用于取消/去重）")
    story_text: str = Field(..., description="输入故事文本", min_length=10, max_length=10000)
    target_duration: Optional[float] = Field(30.0, description="目标视频时长（秒）")
    scene_count: Optional[int] = Field(4, description="目标场景数量", ge=1, le=20)


class StoryboardResponse(BaseModel):
    """分镜脚本生成响应"""
    request_id: str
    trace_id: str
    scenes: List[Scene] = Field(..., description="场景列表")
    total_duration: float = Field(..., description="总时长（秒）")


# ========== Image Generation ==========

class ImageGenRequest(BaseModel):
    """图像生成请求"""
    trace_id: str
    request_id: str
    prompt: str = Field(..., description="图像生成提示词", min_length=1, max_length=2000)
    negative_prompt: Optional[str] = Field("", description="负面提示词")
    width: int = Field(512, description="图像宽度", ge=256, le=2048)
    height: int = Field(512, description="图像高度", ge=256, le=2048)
    num_inference_steps: int = Field(20, description="推理步数", ge=10, le=100)
    guidance_scale: float = Field(7.5, description="引导系数", ge=1.0, le=20.0)
    seed: Optional[int] = Field(None, description="随机种子（None 为随机）")


class ImageGenResponse(BaseModel):
    """图像生成响应"""
    request_id: str
    trace_id: str
    image_path: str = Field(..., description="生成的图像路径（绝对路径）")
    width: int
    height: int
    seed: int = Field(..., description="实际使用的随机种子")


# ========== Text-to-Speech ==========

class TtsRequest(BaseModel):
    """语音合成请求"""
    trace_id: str
    request_id: str
    text: str = Field(..., description="要合成的文本", min_length=1, max_length=5000)
    voice: Optional[str] = Field("default", description="语音类型")
    speed: float = Field(1.0, description="语速（0.5-2.0）", ge=0.5, le=2.0)
    output_format: str = Field("wav", description="输出格式: wav/mp3")


class TtsResponse(BaseModel):
    """语音合成响应"""
    request_id: str
    trace_id: str
    audio_path: str = Field(..., description="生成的音频路径（绝对路径）")
    duration_seconds: float = Field(..., description="音频时长（秒）")
    format: str = Field(..., description="音频格式")


# ========== Video Composition ==========

class SceneAsset(BaseModel):
    """场景资源"""
    scene_number: int
    image_path: str = Field(..., description="图像路径")
    audio_path: str = Field(..., description="音频路径")
    duration_seconds: float = Field(..., description="场景时长")


class ComposeRequest(BaseModel):
    """视频合成请求"""
    trace_id: str
    request_id: str
    scenes: List[SceneAsset] = Field(..., description="场景资源列表")
    output_path: str = Field(..., description="输出视频路径（绝对路径）")
    fps: int = Field(24, description="帧率", ge=1, le=60)
    codec: Optional[str] = Field("h264_nvenc", description="视频编码器")
    bitrate: Optional[str] = Field("2M", description="视频比特率")


class ComposeResponse(BaseModel):
    """视频合成响应"""
    request_id: str
    trace_id: str
    video_path: str = Field(..., description="生成的视频路径")
    duration_seconds: float = Field(..., description="视频总时长")
    size_bytes: int = Field(..., description="文件大小（字节）")


# ========== Cancellation ==========

class CancelResponse(BaseModel):
    """取消任务响应"""
    request_id: str
    canceled: bool = Field(..., description="是否成功取消")
    message: str = Field(..., description="取消状态描述")
