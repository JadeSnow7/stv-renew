"""
Provider 基类 - 定义所有 AI 服务的统一接口
"""
from abc import ABC, abstractmethod
from schemas import (
    StoryboardRequest, StoryboardResponse,
    ImageGenRequest, ImageGenResponse,
    TtsRequest, TtsResponse,
    ComposeRequest, ComposeResponse
)


class BaseProvider(ABC):
    """AI 服务 Provider 基类"""
    
    @abstractmethod
    async def generate_storyboard(self, request: StoryboardRequest) -> StoryboardResponse:
        """生成分镜脚本"""
        pass
    
    @abstractmethod
    async def generate_image(self, request: ImageGenRequest) -> ImageGenResponse:
        """生成图像"""
        pass
    
    @abstractmethod
    async def generate_speech(self, request: TtsRequest) -> TtsResponse:
        """生成语音"""
        pass
    
    @abstractmethod
    async def compose_video(self, request: ComposeRequest) -> ComposeResponse:
        """合成视频"""
        pass
    
    async def cancel_task(self, request_id: str) -> bool:
        """取消任务（可选实现）"""
        return False
    
    async def cleanup(self):
        """清理资源"""
        pass
