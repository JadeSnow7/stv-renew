"""
任务注册表 - 跟踪所有正在运行的任务
支持取消、状态查询
"""
import threading
from typing import Dict, Optional
from dataclasses import dataclass
from datetime import datetime
from enum import Enum


class TaskStatus(Enum):
    """任务状态"""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELED = "canceled"


@dataclass
class TaskInfo:
    """任务信息"""
    task_id: str  # 内部生成的任务 ID
    request_id: str  # 客户端提供的请求 ID
    trace_id: str  # 跟踪 ID
    task_type: str  # storyboard/imagegen/tts/compose
    status: TaskStatus
    created_at: datetime
    updated_at: datetime
    error_message: Optional[str] = None


class TaskRegistry:
    """线程安全的任务注册表"""
    
    def __init__(self):
        self._tasks: Dict[str, TaskInfo] = {}  # request_id -> TaskInfo
        self._lock = threading.Lock()
        self._task_counter = 0
    
    def register(self, trace_id: str, request_id: str, task_type: str) -> str:
        """注册新任务"""
        with self._lock:
            self._task_counter += 1
            task_id = f"task_{self._task_counter:06d}"
            
            now = datetime.now()
            task_info = TaskInfo(
                task_id=task_id,
                request_id=request_id,
                trace_id=trace_id,
                task_type=task_type,
                status=TaskStatus.RUNNING,
                created_at=now,
                updated_at=now
            )
            
            self._tasks[request_id] = task_info
            return task_id
    
    def complete(self, request_id: str):
        """标记任务完成"""
        with self._lock:
            if request_id in self._tasks:
                self._tasks[request_id].status = TaskStatus.COMPLETED
                self._tasks[request_id].updated_at = datetime.now()
    
    def fail(self, request_id: str, error_message: str):
        """标记任务失败"""
        with self._lock:
            if request_id in self._tasks:
                self._tasks[request_id].status = TaskStatus.FAILED
                self._tasks[request_id].error_message = error_message
                self._tasks[request_id].updated_at = datetime.now()
    
    def cancel(self, request_id: str) -> bool:
        """取消任务"""
        with self._lock:
            if request_id not in self._tasks:
                return False
            
            task = self._tasks[request_id]
            if task.status in (TaskStatus.COMPLETED, TaskStatus.FAILED, TaskStatus.CANCELED):
                return False  # 已经结束的任务无法取消
            
            task.status = TaskStatus.CANCELED
            task.updated_at = datetime.now()
            return True
    
    def get(self, request_id: str) -> Optional[TaskInfo]:
        """获取任务信息"""
        with self._lock:
            return self._tasks.get(request_id)
    
    def is_canceled(self, request_id: str) -> bool:
        """检查任务是否被取消"""
        with self._lock:
            task = self._tasks.get(request_id)
            return task is not None and task.status == TaskStatus.CANCELED
    
    def cleanup_old_tasks(self, max_age_seconds: int = 3600):
        """清理旧任务（避免内存泄漏）"""
        with self._lock:
            now = datetime.now()
            to_remove = []
            
            for request_id, task in self._tasks.items():
                age = (now - task.updated_at).total_seconds()
                if age > max_age_seconds and task.status in (
                    TaskStatus.COMPLETED, TaskStatus.FAILED, TaskStatus.CANCELED
                ):
                    to_remove.append(request_id)
            
            for request_id in to_remove:
                del self._tasks[request_id]
            
            if to_remove:
                print(f"Cleaned up {len(to_remove)} old tasks")
