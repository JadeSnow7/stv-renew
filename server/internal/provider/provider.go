package provider

import (
	"context"
	"fmt"
	"math/rand"
	"time"

	"stv/server/internal/model"

	"github.com/google/uuid"
)

type Error struct {
	Category string
	Code     string
	Retryable bool
	UserMessage string
	InternalMessage string
}

type ExecuteInput struct {
	UserID    string
	ProjectID string
	JobID     string
	TaskID    string
	TaskType  model.TaskType
	SceneIndex int
	TraceID   string
	Payload   map[string]any
}

type ExecuteOutput struct {
	Output map[string]any
	Asset  *model.Asset
}

type Adapter interface {
	Execute(ctx context.Context, in ExecuteInput) (ExecuteOutput, *Error)
}

type MockAdapter struct {
	rng *rand.Rand
}

func NewMockAdapter() *MockAdapter {
	return &MockAdapter{
		rng: rand.New(rand.NewSource(time.Now().UnixNano())),
	}
}

func (m *MockAdapter) Execute(ctx context.Context, in ExecuteInput) (ExecuteOutput, *Error) {
	workDuration := 300 * time.Millisecond
	switch in.TaskType {
	case model.TaskStoryboardGenerate:
		workDuration = 700 * time.Millisecond
	case model.TaskImageGenerate:
		workDuration = 1200 * time.Millisecond
	case model.TaskTTSGenerate:
		workDuration = 800 * time.Millisecond
	case model.TaskComposeVideo:
		workDuration = 1600 * time.Millisecond
	}

	if err := waitCancelable(ctx, workDuration); err != nil {
		return ExecuteOutput{}, &Error{
			Category:        "canceled",
			Code:            "CANCELED",
			Retryable:       false,
			UserMessage:     "Task canceled",
			InternalMessage: err.Error(),
		}
	}

	if in.Payload != nil {
		if raw, ok := in.Payload["simulate_error"].(bool); ok && raw {
			return ExecuteOutput{}, &Error{
				Category:        "network",
				Code:            "UPSTREAM_TIMEOUT",
				Retryable:       true,
				UserMessage:     "Upstream timeout",
				InternalMessage: "mock simulate_error=true",
			}
		}
	}

	if m.rng.Float64() < 0.03 {
		return ExecuteOutput{}, &Error{
			Category:        "network",
			Code:            "UPSTREAM_5XX",
			Retryable:       true,
			UserMessage:     "Service temporary unavailable",
			InternalMessage: "mock random failure",
		}
	}

	now := time.Now().UTC()
	output := ExecuteOutput{
		Output: map[string]any{
			"provider": "mock",
			"task_key": in.TaskType,
		},
	}

	switch in.TaskType {
	case model.TaskStoryboardGenerate:
		output.Asset = &model.Asset{
			ID:         uuid.NewString(),
			UserID:     in.UserID,
			ProjectID:  in.ProjectID,
			Type:       model.AssetStoryboardJSON,
			Status:     "ready",
			StorageKey: fmt.Sprintf("users/%s/projects/%s/storyboard_json/%s.json", in.UserID, in.ProjectID, uuid.NewString()),
			MimeType:   "application/json",
			SizeBytes:  1024,
			Metadata:   map[string]any{"scene_count_hint": 4},
			ExpiresAt:  now.Add(30 * 24 * time.Hour),
			CreatedAt:  now,
		}
	case model.TaskImageGenerate:
		output.Asset = &model.Asset{
			ID:         uuid.NewString(),
			UserID:     in.UserID,
			ProjectID:  in.ProjectID,
			Type:       model.AssetImage,
			Status:     "ready",
			StorageKey: fmt.Sprintf("users/%s/projects/%s/image/%s.png", in.UserID, in.ProjectID, uuid.NewString()),
			MimeType:   "image/png",
			SizeBytes:  220000,
			Metadata:   map[string]any{"scene_index": in.SceneIndex},
			ExpiresAt:  now.Add(30 * 24 * time.Hour),
			CreatedAt:  now,
		}
	case model.TaskTTSGenerate:
		output.Asset = &model.Asset{
			ID:         uuid.NewString(),
			UserID:     in.UserID,
			ProjectID:  in.ProjectID,
			Type:       model.AssetAudio,
			Status:     "ready",
			StorageKey: fmt.Sprintf("users/%s/projects/%s/audio/%s.mp3", in.UserID, in.ProjectID, uuid.NewString()),
			MimeType:   "audio/mpeg",
			SizeBytes:  120000,
			DurationMS: 4000,
			Metadata:   map[string]any{"scene_index": in.SceneIndex},
			ExpiresAt:  now.Add(30 * 24 * time.Hour),
			CreatedAt:  now,
		}
	case model.TaskComposeVideo:
		output.Asset = &model.Asset{
			ID:         uuid.NewString(),
			UserID:     in.UserID,
			ProjectID:  in.ProjectID,
			Type:       model.AssetFinalVideo,
			Status:     "ready",
			StorageKey: fmt.Sprintf("users/%s/projects/%s/final_video/%s.mp4", in.UserID, in.ProjectID, uuid.NewString()),
			MimeType:   "video/mp4",
			SizeBytes:  2_000_000,
			DurationMS: 12000,
			Metadata:   map[string]any{"codec": "h264"},
			ExpiresAt:  now.Add(30 * 24 * time.Hour),
			CreatedAt:  now,
		}
	}
	return output, nil
}

func waitCancelable(ctx context.Context, d time.Duration) error {
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()
	timeout := time.NewTimer(d)
	defer timeout.Stop()
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-timeout.C:
			return nil
		case <-ticker.C:
		}
	}
}
