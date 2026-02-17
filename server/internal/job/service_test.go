package job

import (
	"context"
	"log/slog"
	"testing"
	"time"

	"stv/server/internal/events"
	"stv/server/internal/model"
	"stv/server/internal/provider"
	"stv/server/internal/store"

	"github.com/google/uuid"
)

type successProvider struct{}

func (p successProvider) Execute(ctx context.Context, in provider.ExecuteInput) (provider.ExecuteOutput, *provider.Error) {
	select {
	case <-ctx.Done():
		return provider.ExecuteOutput{}, &provider.Error{
			Category:        "canceled",
			Code:            "CANCELED",
			Retryable:       false,
			UserMessage:     "canceled",
			InternalMessage: "ctx canceled",
		}
	default:
	}
	return provider.ExecuteOutput{Output: map[string]any{"ok": true}}, nil
}

type slowProvider struct{}

func (p slowProvider) Execute(ctx context.Context, in provider.ExecuteInput) (provider.ExecuteOutput, *provider.Error) {
	ticker := time.NewTicker(50 * time.Millisecond)
	defer ticker.Stop()
	timeout := time.NewTimer(2 * time.Second)
	defer timeout.Stop()
	for {
		select {
		case <-ctx.Done():
			return provider.ExecuteOutput{}, &provider.Error{
				Category:        "canceled",
				Code:            "CANCELED",
				Retryable:       false,
				UserMessage:     "canceled",
				InternalMessage: "ctx canceled",
			}
		case <-timeout.C:
			return provider.ExecuteOutput{Output: map[string]any{"ok": true}}, nil
		case <-ticker.C:
		}
	}
}

func setupProject(t *testing.T, st *store.MemoryStore, userID string, sceneCount int) model.Project {
	t.Helper()
	now := time.Now().UTC()
	p := model.Project{
		ID:         uuid.NewString(),
		UserID:     userID,
		Name:       "test",
		StoryText:  "story",
		Style:      "cinematic",
		SceneCount: sceneCount,
		Status:     "draft",
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	scenes := make([]model.StoryboardScene, 0, sceneCount)
	for i := 0; i < sceneCount; i++ {
		scenes = append(scenes, model.StoryboardScene{
			ID:         uuid.NewString(),
			ProjectID:  p.ID,
			SceneIndex: i,
			DurationMS: 1000,
			UpdatedAt:  now,
		})
	}
	if _, err := st.CreateProject(p, scenes); err != nil {
		t.Fatalf("create project: %v", err)
	}
	return p
}

func TestJobSuccess(t *testing.T) {
	st := store.NewMemoryStore()
	hub := events.NewHub()
	svc := NewService(st, hub, successProvider{}, slog.Default(), 20, 2, 6)

	userID := uuid.NewString()
	now := time.Now().UTC()
	st.UpsertUser(model.User{
		ID:           userID,
		Email:        "u@example.com",
		PasswordHash: "x",
		Role:         model.RoleUser,
		Status:       "active",
		CreatedAt:    now,
		UpdatedAt:    now,
	})
	project := setupProject(t, st, userID, 2)

	job, err := svc.StartJob(context.Background(), userID, project.ID, "trace-1", "")
	if err != nil {
		t.Fatalf("start job: %v", err)
	}

	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		got, err := svc.GetJob(job.ID)
		if err != nil {
			t.Fatalf("get job: %v", err)
		}
		if got.Status == model.JobSucceeded {
			return
		}
		time.Sleep(50 * time.Millisecond)
	}
	t.Fatalf("job did not reach succeeded before timeout")
}

func TestJobCancel(t *testing.T) {
	st := store.NewMemoryStore()
	hub := events.NewHub()
	svc := NewService(st, hub, slowProvider{}, slog.Default(), 20, 2, 6)

	userID := uuid.NewString()
	now := time.Now().UTC()
	st.UpsertUser(model.User{
		ID:           userID,
		Email:        "u2@example.com",
		PasswordHash: "x",
		Role:         model.RoleUser,
		Status:       "active",
		CreatedAt:    now,
		UpdatedAt:    now,
	})
	project := setupProject(t, st, userID, 1)
	job, err := svc.StartJob(context.Background(), userID, project.ID, "trace-2", "")
	if err != nil {
		t.Fatalf("start job: %v", err)
	}
	time.Sleep(120 * time.Millisecond)
	if _, err := svc.CancelJob(userID, job.ID); err != nil {
		t.Fatalf("cancel job: %v", err)
	}
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		got, err := svc.GetJob(job.ID)
		if err != nil {
			t.Fatalf("get job: %v", err)
		}
		if got.Status == model.JobCanceled {
			return
		}
		time.Sleep(80 * time.Millisecond)
	}
	t.Fatalf("job did not reach canceled before timeout")
}
