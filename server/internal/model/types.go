package model

import "time"

type UserRole string

const (
	RoleUser  UserRole = "user"
	RoleAdmin UserRole = "admin"
)

type User struct {
	ID           string    `json:"id"`
	Email        string    `json:"email"`
	PasswordHash string    `json:"-"`
	Role         UserRole  `json:"role"`
	Status       string    `json:"status"`
	CreatedAt    time.Time `json:"created_at"`
	UpdatedAt    time.Time `json:"updated_at"`
}

type RefreshToken struct {
	ID        string
	UserID    string
	TokenHash string
	ExpiresAt time.Time
	RevokedAt *time.Time
	CreatedAt time.Time
}

type Project struct {
	ID           string    `json:"id"`
	UserID       string    `json:"user_id"`
	Name         string    `json:"name"`
	StoryText    string    `json:"story_text"`
	Style        string    `json:"style"`
	SceneCount   int       `json:"scene_count"`
	Status       string    `json:"status"`
	CurrentJobID string    `json:"current_job_id,omitempty"`
	CreatedAt    time.Time `json:"created_at"`
	UpdatedAt    time.Time `json:"updated_at"`
}

type StoryboardScene struct {
	ID             string    `json:"id"`
	ProjectID      string    `json:"project_id"`
	SceneIndex     int       `json:"scene_index"`
	Narration      string    `json:"narration"`
	Prompt         string    `json:"prompt"`
	NegativePrompt string    `json:"negative_prompt"`
	DurationMS     int       `json:"duration_ms"`
	Transition     string    `json:"transition"`
	Locked         bool      `json:"locked"`
	UpdatedAt      time.Time `json:"updated_at"`
}

type AssetType string

const (
	AssetStoryboardJSON AssetType = "storyboard_json"
	AssetImage          AssetType = "image"
	AssetAudio          AssetType = "audio"
	AssetVideoClip      AssetType = "video_clip"
	AssetFinalVideo     AssetType = "final_video"
	AssetThumbnail      AssetType = "thumbnail"
)

type Asset struct {
	ID         string         `json:"id"`
	UserID     string         `json:"user_id"`
	ProjectID  string         `json:"project_id"`
	Type       AssetType      `json:"type"`
	Status     string         `json:"status"`
	StorageKey string         `json:"storage_key"`
	MimeType   string         `json:"mime_type"`
	SizeBytes  int64          `json:"size_bytes"`
	DurationMS int            `json:"duration_ms"`
	Metadata   map[string]any `json:"metadata"`
	ExpiresAt  time.Time      `json:"expires_at"`
	CreatedAt  time.Time      `json:"created_at"`
}

type JobStatus string

const (
	JobQueued    JobStatus = "queued"
	JobRunning   JobStatus = "running"
	JobFailed    JobStatus = "failed"
	JobCanceled  JobStatus = "canceled"
	JobSucceeded JobStatus = "succeeded"
)

type Job struct {
	ID              string    `json:"id"`
	UserID          string    `json:"user_id"`
	ProjectID       string    `json:"project_id"`
	Status          JobStatus `json:"status"`
	Progress        float64   `json:"progress"`
	CancelRequested bool      `json:"cancel_requested"`
	ErrorCode       string    `json:"error_code,omitempty"`
	ErrorMessage    string    `json:"error_message,omitempty"`
	Retryable       bool      `json:"retryable"`
	TraceID         string    `json:"trace_id"`
	IdempotencyKey  string    `json:"idempotency_key,omitempty"`
	CreatedAt       time.Time `json:"created_at"`
	StartedAt       time.Time `json:"started_at,omitempty"`
	EndedAt         time.Time `json:"ended_at,omitempty"`
}

type TaskStatus string

const (
	TaskQueued    TaskStatus = "queued"
	TaskRunning   TaskStatus = "running"
	TaskFailed    TaskStatus = "failed"
	TaskCanceled  TaskStatus = "canceled"
	TaskSucceeded TaskStatus = "succeeded"
)

type TaskType string

const (
	TaskStoryboardGenerate TaskType = "storyboard_generate"
	TaskImageGenerate      TaskType = "image_generate"
	TaskTTSGenerate        TaskType = "tts_generate"
	TaskComposeVideo       TaskType = "compose_video"
)

type JobTask struct {
	ID          string         `json:"id"`
	JobID       string         `json:"job_id"`
	TaskKey     string         `json:"task_key"`
	TaskType    TaskType       `json:"task_type"`
	Status      TaskStatus     `json:"status"`
	Attempt     int            `json:"attempt"`
	MaxAttempt  int            `json:"max_attempt"`
	DependsOn   []string       `json:"depends_on"`
	Input       map[string]any `json:"input"`
	Output      map[string]any `json:"output"`
	ErrorCode   string         `json:"error_code,omitempty"`
	ErrorMsg    string         `json:"error_message,omitempty"`
	Retryable   bool           `json:"retryable"`
	NextRunAt   time.Time      `json:"next_run_at,omitempty"`
	StartedAt   time.Time      `json:"started_at,omitempty"`
	EndedAt     time.Time      `json:"ended_at,omitempty"`
	WorkerID    string         `json:"worker_id,omitempty"`
	ProjectID   string         `json:"project_id"`
	TraceID     string         `json:"trace_id"`
	SceneIndex  int            `json:"scene_index,omitempty"`
	DisplayName string         `json:"display_name"`
}

type JobEventType string

const (
	EventJobCreated   JobEventType = "job_created"
	EventJobProgress  JobEventType = "job_progress"
	EventTaskStarted  JobEventType = "task_started"
	EventTaskSuccess  JobEventType = "task_succeeded"
	EventTaskFailed   JobEventType = "task_failed"
	EventJobCanceled  JobEventType = "job_canceled"
	EventJobSucceeded JobEventType = "job_succeeded"
	EventJobFailed    JobEventType = "job_failed"
	EventAssetReady   JobEventType = "asset_ready"
)

type JobEvent struct {
	EventID   string         `json:"event_id"`
	Seq       int64          `json:"seq"`
	TraceID   string         `json:"trace_id"`
	JobID     string         `json:"job_id"`
	ProjectID string         `json:"project_id"`
	Type      JobEventType   `json:"type"`
	TS        time.Time      `json:"ts"`
	Payload   map[string]any `json:"payload"`
}

type ExportStatus string

const (
	ExportQueued    ExportStatus = "queued"
	ExportRunning   ExportStatus = "running"
	ExportFailed    ExportStatus = "failed"
	ExportSucceeded ExportStatus = "succeeded"
)

type Export struct {
	ID          string       `json:"id"`
	UserID      string       `json:"user_id"`
	ProjectID   string       `json:"project_id"`
	Status      ExportStatus `json:"status"`
	AssetID     string       `json:"asset_id,omitempty"`
	DownloadURL string       `json:"download_url,omitempty"`
	CreatedAt   time.Time    `json:"created_at"`
	UpdatedAt   time.Time    `json:"updated_at"`
}
