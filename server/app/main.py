"""
StoryToVideo Renew - 服务端入口
提供统一的本地 API，支持 mock / local_gpu / cloud 三种 provider
"""
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import JSONResponse
from contextlib import asynccontextmanager
import uvicorn
import os
import random
import sys
from pathlib import Path

# 添加 app 目录到 Python 路径
sys.path.insert(0, str(Path(__file__).parent))

from schemas import (
    HealthResponse, ErrorResponse,
    StoryboardRequest, StoryboardResponse,
    ImageGenRequest, ImageGenResponse,
    TtsRequest, TtsResponse,
    ComposeRequest, ComposeResponse,
    CancelResponse
)
from services.task_registry import TaskRegistry
from providers.base import BaseProvider
from providers.mock import MockProvider

# 全局状态
task_registry = TaskRegistry()
provider: BaseProvider = None
fault_inject_rate = 0.0


@asynccontextmanager
async def lifespan(app: FastAPI):
    """应用生命周期管理"""
    global provider, fault_inject_rate
    
    # 启动时：初始化 provider
    provider_mode = os.getenv("STV_PROVIDER", "mock")
    
    if provider_mode == "mock":
        from providers.mock import MockProvider
        provider = MockProvider()
    elif provider_mode == "local_gpu":
        from providers.local_gpu import LocalGpuProvider
        provider = LocalGpuProvider()
    elif provider_mode == "cloud":
        raise NotImplementedError("Cloud provider not implemented yet (M3)")
    else:
        raise ValueError(f"Unknown provider mode: {provider_mode}")

    raw_fault_rate = os.getenv("STV_FAULT_INJECT_RATE", "0.0")
    try:
        parsed = float(raw_fault_rate)
    except ValueError:
        parsed = 0.0
    fault_inject_rate = max(0.0, min(1.0, parsed))
    
    print(f"✓ Provider initialized: {provider_mode}")
    if fault_inject_rate > 0.0:
        print(f"✓ Fault injection enabled: rate={fault_inject_rate:.2f}")
    yield
    
    # 关闭时：清理资源
    if provider:
        await provider.cleanup()
    print("✓ Server shutdown complete")


app = FastAPI(
    title="StoryToVideo Renew API",
    version="0.1.0",
    description="Local AI video generation service",
    lifespan=lifespan
)


@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    """全局异常处理"""
    return JSONResponse(
        status_code=500,
        content=ErrorResponse(
            error_code="INTERNAL_ERROR",
            user_message="Internal server error",
            internal_message=str(exc),
            retryable=True,
            trace_id=request.headers.get("X-Trace-ID", "unknown")
        ).model_dump()
    )


@app.middleware("http")
async def fault_injection_middleware(request: Request, call_next):
    """测试态故障注入：对 /v1/* 随机返回瞬时 5xx。"""
    if (
        fault_inject_rate > 0.0
        and request.url.path.startswith("/v1/")
        and not request.url.path.startswith("/v1/cancel/")
    ):
        if random.random() < fault_inject_rate:
            return JSONResponse(
                status_code=503,
                content=ErrorResponse(
                    error_code="INJECTED_5XX",
                    user_message="Transient server overload, please retry.",
                    internal_message="Injected fault for resilience testing",
                    retryable=True,
                    trace_id=request.headers.get("X-Trace-ID", "unknown"),
                ).model_dump(),
            )

    return await call_next(request)


@app.get("/healthz", response_model=HealthResponse)
async def health_check():
    """健康检查"""
    return HealthResponse(
        status="healthy",
        provider=provider.__class__.__name__ if provider else "unknown",
        version="0.1.0"
    )


@app.post("/v1/storyboard", response_model=StoryboardResponse)
async def create_storyboard(request: StoryboardRequest):
    """生成分镜脚本"""
    if not provider:
        raise HTTPException(status_code=503, detail="Provider not initialized")
    
    # 注册任务
    task_id = task_registry.register(request.trace_id, request.request_id, "storyboard")
    
    try:
        result = await provider.generate_storyboard(request)
        task_registry.complete(request.request_id)
        return result
    except Exception as e:
        task_registry.fail(request.request_id, str(e))
        raise


@app.post("/v1/imagegen", response_model=ImageGenResponse)
async def generate_image(request: ImageGenRequest):
    """生成图像"""
    if not provider:
        raise HTTPException(status_code=503, detail="Provider not initialized")
    
    task_id = task_registry.register(request.trace_id, request.request_id, "imagegen")
    
    try:
        result = await provider.generate_image(request)
        task_registry.complete(request.request_id)
        return result
    except Exception as e:
        task_registry.fail(request.request_id, str(e))
        raise


@app.post("/v1/tts", response_model=TtsResponse)
async def generate_speech(request: TtsRequest):
    """生成语音"""
    if not provider:
        raise HTTPException(status_code=503, detail="Provider not initialized")
    
    task_id = task_registry.register(request.trace_id, request.request_id, "tts")
    
    try:
        result = await provider.generate_speech(request)
        task_registry.complete(request.request_id)
        return result
    except Exception as e:
        task_registry.fail(request.request_id, str(e))
        raise


@app.post("/v1/compose", response_model=ComposeResponse)
async def compose_video(request: ComposeRequest):
    """合成视频"""
    if not provider:
        raise HTTPException(status_code=503, detail="Provider not initialized")
    
    task_id = task_registry.register(request.trace_id, request.request_id, "compose")
    
    try:
        result = await provider.compose_video(request)
        task_registry.complete(request.request_id)
        return result
    except Exception as e:
        task_registry.fail(request.request_id, str(e))
        raise


@app.post("/v1/cancel/{request_id}", response_model=CancelResponse)
async def cancel_task(request_id: str):
    """取消任务"""
    canceled = task_registry.cancel(request_id)
    
    # 通知 provider 取消任务（如果支持）
    if provider and canceled:
        await provider.cancel_task(request_id)
    
    return CancelResponse(
        request_id=request_id,
        canceled=canceled,
        message="Task canceled" if canceled else "Task not found or already finished"
    )


def main():
    """启动服务器"""
    host = os.getenv("STV_HOST", "127.0.0.1")
    port = int(os.getenv("STV_PORT", "8765"))
    
    print(f"Starting StoryToVideo server at http://{host}:{port}")
    print(f"Provider mode: {os.getenv('STV_PROVIDER', 'mock')}")
    print(f"Docs available at: http://{host}:{port}/docs")
    
    uvicorn.run(
        app,
        host=host,
        port=port,
        log_level="info"
    )


if __name__ == "__main__":
    main()
