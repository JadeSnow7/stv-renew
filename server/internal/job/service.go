package job

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"math/rand"
	"sync"
	"time"

	"stv/server/internal/events"
	"stv/server/internal/model"
	"stv/server/internal/provider"
	"stv/server/internal/store"

	"github.com/google/uuid"
)

var (
	ErrTooManyRunningJobs = errors.New("too many running jobs for user")
	ErrInvalidJobState    = errors.New("invalid job state")
)

type Service struct {
	store *store.MemoryStore
	hub   *events.Hub
	prov  provider.Adapter
	log   *slog.Logger

	maxConcurrent int
	maxUserJobs   int
	maxSceneJobs  int

	globalSem chan struct{}

	mu            sync.Mutex
	runningByUser map[string]int
	activeRunner  map[string]bool
}

func NewService(st *store.MemoryStore, hub *events.Hub, prov provider.Adapter, logger *slog.Logger, maxConcurrent, maxUserJobs, maxSceneJobs int) *Service {
	if maxConcurrent < 1 {
		maxConcurrent = 20
	}
	if maxUserJobs < 1 {
		maxUserJobs = 2
	}
	if maxSceneJobs < 1 {
		maxSceneJobs = 6
	}
	return &Service{
		store:         st,
		hub:           hub,
		prov:          prov,
		log:           logger,
		maxConcurrent: maxConcurrent,
		maxUserJobs:   maxUserJobs,
		maxSceneJobs:  maxSceneJobs,
		globalSem:     make(chan struct{}, maxConcurrent),
		runningByUser: map[string]int{},
		activeRunner:  map[string]bool{},
	}
}

func (s *Service) StartJob(ctx context.Context, userID, projectID, traceID, idempotencyKey string) (model.Job, error) {
	if existing, ok := s.store.GetJobByIdempotency(userID, idempotencyKey); ok {
		return existing, nil
	}

	project, err := s.store.GetProject(projectID)
	if err != nil {
		return model.Job{}, err
	}
	if project.UserID != userID {
		return model.Job{}, store.ErrForbidden
	}

	s.mu.Lock()
	if s.runningByUser[userID] >= s.maxUserJobs {
		s.mu.Unlock()
		return model.Job{}, ErrTooManyRunningJobs
	}
	s.mu.Unlock()

	now := time.Now().UTC()
	job := model.Job{
		ID:             uuid.NewString(),
		UserID:         userID,
		ProjectID:      projectID,
		Status:         model.JobQueued,
		Progress:       0,
		CancelRequested: false,
		TraceID:        traceID,
		IdempotencyKey: idempotencyKey,
		CreatedAt:      now,
	}
	tasks := buildTasks(project, job)
	created, err := s.store.CreateJob(job, tasks, idempotencyKey)
	if err != nil {
		return model.Job{}, err
	}

	project.CurrentJobID = created.ID
	project.Status = "running"
	project.UpdatedAt = now
	_ = s.store.UpdateProject(project)

	s.publishEvent(created, model.EventJobCreated, map[string]any{
		"status": created.Status,
		"tasks":  len(tasks),
	})

	s.startRunnerIfNeeded(created)
	return created, nil
}

func (s *Service) GetJob(jobID string) (model.Job, error) {
	return s.store.GetJob(jobID)
}

func (s *Service) GetJobTasks(jobID string) ([]model.JobTask, error) {
	return s.store.GetJobTasks(jobID)
}

func (s *Service) ListEventsFrom(jobID string, fromSeq int64) ([]model.JobEvent, error) {
	return s.store.ListJobEventsFromSeq(jobID, fromSeq)
}

func (s *Service) CancelJob(userID, jobID string) (model.Job, error) {
	job, err := s.store.GetJob(jobID)
	if err != nil {
		return model.Job{}, err
	}
	if job.UserID != userID {
		return model.Job{}, store.ErrForbidden
	}
	if isJobTerminal(job.Status) {
		return job, nil
	}
	job.CancelRequested = true
	if err := s.store.UpdateJob(job); err != nil {
		return model.Job{}, err
	}
	s.publishEvent(job, model.EventJobProgress, map[string]any{
		"status":           job.Status,
		"cancel_requested": true,
		"progress":         job.Progress,
	})
	return job, nil
}

func (s *Service) RetryJob(ctx context.Context, userID, jobID, traceID string) (model.Job, error) {
	job, err := s.store.GetJob(jobID)
	if err != nil {
		return model.Job{}, err
	}
	if job.UserID != userID {
		return model.Job{}, store.ErrForbidden
	}
	if job.Status != model.JobFailed && job.Status != model.JobCanceled {
		return model.Job{}, ErrInvalidJobState
	}

	s.mu.Lock()
	if s.runningByUser[userID] >= s.maxUserJobs {
		s.mu.Unlock()
		return model.Job{}, ErrTooManyRunningJobs
	}
	s.mu.Unlock()

	tasks, err := s.store.GetJobTasks(jobID)
	if err != nil {
		return model.Job{}, err
	}
	for i := range tasks {
		if tasks[i].Status == model.TaskFailed || tasks[i].Status == model.TaskCanceled {
			tasks[i].Status = model.TaskQueued
			tasks[i].ErrorCode = ""
			tasks[i].ErrorMsg = ""
			tasks[i].Retryable = false
			tasks[i].StartedAt = time.Time{}
			tasks[i].EndedAt = time.Time{}
			tasks[i].NextRunAt = time.Time{}
		}
	}
	if err := s.store.ReplaceJobTasks(jobID, tasks); err != nil {
		return model.Job{}, err
	}

	job.Status = model.JobQueued
	job.Progress = calcProgress(tasks)
	job.CancelRequested = false
	job.ErrorCode = ""
	job.ErrorMessage = ""
	job.Retryable = false
	job.TraceID = traceID
	job.StartedAt = time.Time{}
	job.EndedAt = time.Time{}
	if err := s.store.UpdateJob(job); err != nil {
		return model.Job{}, err
	}
	s.publishEvent(job, model.EventJobCreated, map[string]any{
		"status": "retry_queued",
	})
	s.startRunnerIfNeeded(job)
	return job, nil
}

func (s *Service) startRunnerIfNeeded(job model.Job) {
	s.mu.Lock()
	if s.activeRunner[job.ID] {
		s.mu.Unlock()
		return
	}
	s.activeRunner[job.ID] = true
	s.runningByUser[job.UserID]++
	s.mu.Unlock()

	go s.runJob(job.ID)
}

func (s *Service) finishRunner(job model.Job) {
	s.mu.Lock()
	delete(s.activeRunner, job.ID)
	if s.runningByUser[job.UserID] > 0 {
		s.runningByUser[job.UserID]--
	}
	s.mu.Unlock()
}

func (s *Service) runJob(jobID string) {
	sceneSem := make(chan struct{}, s.maxSceneJobs)

	defer func() {
		job, err := s.store.GetJob(jobID)
		if err == nil {
			s.finishRunner(job)
		}
	}()

	for {
		job, err := s.store.GetJob(jobID)
		if err != nil {
			return
		}
		if isJobTerminal(job.Status) {
			return
		}
		if job.StartedAt.IsZero() {
			job.StartedAt = time.Now().UTC()
			job.Status = model.JobRunning
			if err := s.store.UpdateJob(job); err == nil {
				s.publishEvent(job, model.EventJobProgress, map[string]any{
					"status":   job.Status,
					"progress": job.Progress,
				})
			}
		}

		if job.CancelRequested {
			_ = s.cancelQueuedTasks(job)
		}

		tasks, err := s.store.GetJobTasks(jobID)
		if err != nil {
			return
		}
		if allTasksTerminal(tasks) {
			s.finalizeJob(job, tasks)
			return
		}

		ready := readyTasks(tasks)
		if len(ready) == 0 {
			time.Sleep(120 * time.Millisecond)
			continue
		}

		var wg sync.WaitGroup
		wg.Add(len(ready))
		for _, task := range ready {
			t := task
			go func() {
				defer wg.Done()
				s.executeTask(job, t, sceneSem)
			}()
		}
		wg.Wait()
	}
}

func (s *Service) cancelQueuedTasks(job model.Job) error {
	tasks, err := s.store.GetJobTasks(job.ID)
	if err != nil {
		return err
	}
	changed := false
	for i := range tasks {
		if tasks[i].Status == model.TaskQueued {
			tasks[i].Status = model.TaskCanceled
			tasks[i].EndedAt = time.Now().UTC()
			tasks[i].ErrorCode = "CANCELED"
			tasks[i].ErrorMsg = "Canceled before execution"
			changed = true
		}
	}
	if !changed {
		return nil
	}
	if err := s.store.ReplaceJobTasks(job.ID, tasks); err != nil {
		return err
	}
	s.updateProgress(job.ID)
	return nil
}

func (s *Service) executeTask(job model.Job, task model.JobTask, sceneSem chan struct{}) {
	s.globalSem <- struct{}{}
	defer func() { <-s.globalSem }()

	if task.TaskType == model.TaskImageGenerate || task.TaskType == model.TaskTTSGenerate {
		sceneSem <- struct{}{}
		defer func() { <-sceneSem }()
	}

	_ = s.updateTask(job.ID, task.ID, func(t *model.JobTask) {
		t.Status = model.TaskRunning
		t.Attempt++
		t.StartedAt = time.Now().UTC()
		t.WorkerID = "worker-" + uuid.NewString()[:8]
	})

	tasks, err := s.store.GetJobTasks(job.ID)
	if err != nil {
		return
	}
	current := findTaskByID(tasks, task.ID)
	if current == nil {
		return
	}

	s.publishEvent(job, model.EventTaskStarted, map[string]any{
		"task_id":      current.ID,
		"task_key":     current.TaskKey,
		"task_type":    current.TaskType,
		"attempt":      current.Attempt,
		"display_name": current.DisplayName,
	})

	// Retry policy: 1s, 2s, 4s with 20% jitter
	maxAttempt := current.MaxAttempt
	var lastErr *provider.Error

	for attempt := current.Attempt; attempt <= maxAttempt; attempt++ {
		if s.isJobCanceled(job.ID) {
			s.markTaskCanceled(job.ID, task.ID, "CANCELED", "Canceled during execution")
			s.updateProgress(job.ID)
			return
		}

		ctx, cancel := context.WithCancel(context.Background())
		stopWatch := make(chan struct{})
		go func() {
			defer close(stopWatch)
			t := time.NewTicker(100 * time.Millisecond)
			defer t.Stop()
			for {
				select {
				case <-ctx.Done():
					return
				case <-t.C:
					if s.isJobCanceled(job.ID) {
						cancel()
						return
					}
				}
			}
		}()

		out, pErr := s.prov.Execute(ctx, provider.ExecuteInput{
			UserID:     job.UserID,
			ProjectID:  job.ProjectID,
			JobID:      job.ID,
			TaskID:     task.ID,
			TaskType:   task.TaskType,
			SceneIndex: task.SceneIndex,
			TraceID:    job.TraceID,
			Payload:    task.Input,
		})
		cancel()
		<-stopWatch

		if pErr == nil {
			_ = s.updateTask(job.ID, task.ID, func(t *model.JobTask) {
				t.Status = model.TaskSucceeded
				t.Output = out.Output
				t.EndedAt = time.Now().UTC()
				t.Retryable = false
				t.ErrorCode = ""
				t.ErrorMsg = ""
			})
			if out.Asset != nil {
				if _, err := s.store.CreateAsset(*out.Asset); err == nil {
					s.publishEvent(job, model.EventAssetReady, map[string]any{
						"asset_id":   out.Asset.ID,
						"asset_type": out.Asset.Type,
					})
				}
			}
			s.publishEvent(job, model.EventTaskSuccess, map[string]any{
				"task_id":   task.ID,
				"task_key":  task.TaskKey,
				"task_type": task.TaskType,
			})
			s.updateProgress(job.ID)
			return
		}

		lastErr = pErr
		if !pErr.Retryable || attempt >= maxAttempt {
			break
		}
		backoff := retryBackoff(attempt)
		if !s.sleepWithCancel(job.ID, backoff) {
			s.markTaskCanceled(job.ID, task.ID, "CANCELED", "Canceled during backoff")
			s.updateProgress(job.ID)
			return
		}
		_ = s.updateTask(job.ID, task.ID, func(t *model.JobTask) {
			t.Attempt++
		})
	}

	if lastErr == nil {
		lastErr = &provider.Error{
			Category:        "unknown",
			Code:            "UNKNOWN",
			Retryable:       false,
			UserMessage:     "Unknown failure",
			InternalMessage: "provider returned nil error",
		}
	}
	_ = s.updateTask(job.ID, task.ID, func(t *model.JobTask) {
		t.Status = model.TaskFailed
		t.ErrorCode = lastErr.Code
		t.ErrorMsg = lastErr.InternalMessage
		t.Retryable = lastErr.Retryable
		t.EndedAt = time.Now().UTC()
	})
	s.publishEvent(job, model.EventTaskFailed, map[string]any{
		"task_id":      task.ID,
		"task_key":     task.TaskKey,
		"task_type":    task.TaskType,
		"error_code":   lastErr.Code,
		"retryable":    lastErr.Retryable,
		"user_message": lastErr.UserMessage,
	})
	s.updateProgress(job.ID)
}

func (s *Service) markTaskCanceled(jobID, taskID, code, msg string) {
	_ = s.updateTask(jobID, taskID, func(t *model.JobTask) {
		t.Status = model.TaskCanceled
		t.ErrorCode = code
		t.ErrorMsg = msg
		t.Retryable = false
		t.EndedAt = time.Now().UTC()
	})
}

func (s *Service) updateTask(jobID, taskID string, fn func(*model.JobTask)) error {
	tasks, err := s.store.GetJobTasks(jobID)
	if err != nil {
		return err
	}
	for i := range tasks {
		if tasks[i].ID == taskID {
			fn(&tasks[i])
			return s.store.ReplaceJobTasks(jobID, tasks)
		}
	}
	return store.ErrNotFound
}

func (s *Service) updateProgress(jobID string) {
	job, err := s.store.GetJob(jobID)
	if err != nil {
		return
	}
	tasks, err := s.store.GetJobTasks(jobID)
	if err != nil {
		return
	}
	job.Progress = calcProgress(tasks)
	_ = s.store.UpdateJob(job)
	s.publishEvent(job, model.EventJobProgress, map[string]any{
		"status":   job.Status,
		"progress": job.Progress,
	})
}

func (s *Service) finalizeJob(job model.Job, tasks []model.JobTask) {
	now := time.Now().UTC()
	hasFailed := false
	hasCanceled := false
	for _, t := range tasks {
		if t.Status == model.TaskFailed {
			hasFailed = true
		}
		if t.Status == model.TaskCanceled {
			hasCanceled = true
		}
	}
	switch {
	case hasFailed:
		job.Status = model.JobFailed
		job.ErrorCode = "TASK_FAILED"
		job.ErrorMessage = "One or more tasks failed"
		job.Retryable = true
	case job.CancelRequested || hasCanceled:
		job.Status = model.JobCanceled
		job.ErrorCode = "CANCELED"
		job.ErrorMessage = "Canceled by user"
		job.Retryable = false
	default:
		job.Status = model.JobSucceeded
		job.ErrorCode = ""
		job.ErrorMessage = ""
		job.Retryable = false
	}
	job.Progress = calcProgress(tasks)
	job.EndedAt = now
	if err := s.store.UpdateJob(job); err != nil {
		return
	}

	project, err := s.store.GetProject(job.ProjectID)
	if err == nil {
		project.Status = string(job.Status)
		project.UpdatedAt = now
		_ = s.store.UpdateProject(project)
	}

	switch job.Status {
	case model.JobSucceeded:
		s.publishEvent(job, model.EventJobSucceeded, map[string]any{
			"progress": job.Progress,
			"status":   job.Status,
		})
	case model.JobCanceled:
		s.publishEvent(job, model.EventJobCanceled, map[string]any{
			"progress": job.Progress,
			"status":   job.Status,
		})
	default:
		s.publishEvent(job, model.EventJobFailed, map[string]any{
			"progress":      job.Progress,
			"status":        job.Status,
			"error_code":    job.ErrorCode,
			"error_message": job.ErrorMessage,
		})
	}
}

func (s *Service) isJobCanceled(jobID string) bool {
	job, err := s.store.GetJob(jobID)
	if err != nil {
		return true
	}
	return job.CancelRequested
}

func (s *Service) publishEvent(job model.Job, eventType model.JobEventType, payload map[string]any) {
	evt, err := s.store.AppendJobEvent(job.ID, model.JobEvent{
		TraceID:   job.TraceID,
		JobID:     job.ID,
		ProjectID: job.ProjectID,
		Type:      eventType,
		TS:        time.Now().UTC(),
		Payload:   payload,
	})
	if err != nil {
		s.log.Error("append event failed", "job_id", job.ID, "error", err)
		return
	}
	s.hub.Publish(job.ID, evt)
}

func buildTasks(project model.Project, job model.Job) []model.JobTask {
	now := time.Now().UTC()
	tasks := make([]model.JobTask, 0, 2+project.SceneCount*2)
	storyboardKey := "storyboard_generate"
	storyboardID := uuid.NewString()
	tasks = append(tasks, model.JobTask{
		ID:          storyboardID,
		JobID:       job.ID,
		TaskKey:     storyboardKey,
		TaskType:    model.TaskStoryboardGenerate,
		Status:      model.TaskQueued,
		Attempt:     0,
		MaxAttempt:  3,
		DependsOn:   []string{},
		Input:       map[string]any{"style": project.Style},
		Output:      map[string]any{},
		ProjectID:   job.ProjectID,
		TraceID:     job.TraceID,
		DisplayName: "Storyboard",
		NextRunAt:   now,
	})

	imageKeys := make([]string, 0, project.SceneCount)
	ttsKeys := make([]string, 0, project.SceneCount)
	for i := 0; i < project.SceneCount; i++ {
		imageKey := fmt.Sprintf("image_generate_%d", i)
		imageKeys = append(imageKeys, imageKey)
		tasks = append(tasks, model.JobTask{
			ID:          uuid.NewString(),
			JobID:       job.ID,
			TaskKey:     imageKey,
			TaskType:    model.TaskImageGenerate,
			Status:      model.TaskQueued,
			Attempt:     0,
			MaxAttempt:  3,
			DependsOn:   []string{storyboardKey},
			Input:       map[string]any{"scene_index": i},
			Output:      map[string]any{},
			ProjectID:   job.ProjectID,
			TraceID:     job.TraceID,
			SceneIndex:  i,
			DisplayName: fmt.Sprintf("Image-%d", i),
			NextRunAt:   now,
		})

		ttsKey := fmt.Sprintf("tts_generate_%d", i)
		ttsKeys = append(ttsKeys, ttsKey)
		tasks = append(tasks, model.JobTask{
			ID:          uuid.NewString(),
			JobID:       job.ID,
			TaskKey:     ttsKey,
			TaskType:    model.TaskTTSGenerate,
			Status:      model.TaskQueued,
			Attempt:     0,
			MaxAttempt:  3,
			DependsOn:   []string{storyboardKey},
			Input:       map[string]any{"scene_index": i},
			Output:      map[string]any{},
			ProjectID:   job.ProjectID,
			TraceID:     job.TraceID,
			SceneIndex:  i,
			DisplayName: fmt.Sprintf("TTS-%d", i),
			NextRunAt:   now,
		})
	}

	deps := append([]string{}, imageKeys...)
	deps = append(deps, ttsKeys...)
	tasks = append(tasks, model.JobTask{
		ID:          uuid.NewString(),
		JobID:       job.ID,
		TaskKey:     "compose_video",
		TaskType:    model.TaskComposeVideo,
		Status:      model.TaskQueued,
		Attempt:     0,
		MaxAttempt:  3,
		DependsOn:   deps,
		Input:       map[string]any{},
		Output:      map[string]any{},
		ProjectID:   job.ProjectID,
		TraceID:     job.TraceID,
		DisplayName: "Compose",
		NextRunAt:   now,
	})
	return tasks
}

func readyTasks(tasks []model.JobTask) []model.JobTask {
	succeeded := map[string]bool{}
	for _, t := range tasks {
		if t.Status == model.TaskSucceeded {
			succeeded[t.TaskKey] = true
		}
	}
	ready := make([]model.JobTask, 0)
	now := time.Now().UTC()
	for _, t := range tasks {
		if t.Status != model.TaskQueued {
			continue
		}
		if !t.NextRunAt.IsZero() && now.Before(t.NextRunAt) {
			continue
		}
		ok := true
		for _, dep := range t.DependsOn {
			if !succeeded[dep] {
				ok = false
				break
			}
		}
		if ok {
			ready = append(ready, t)
		}
	}
	return ready
}

func findTaskByID(tasks []model.JobTask, id string) *model.JobTask {
	for i := range tasks {
		if tasks[i].ID == id {
			return &tasks[i]
		}
	}
	return nil
}

func allTasksTerminal(tasks []model.JobTask) bool {
	for _, t := range tasks {
		if !isTaskTerminal(t.Status) {
			return false
		}
	}
	return true
}

func isTaskTerminal(status model.TaskStatus) bool {
	return status == model.TaskSucceeded || status == model.TaskFailed || status == model.TaskCanceled
}

func isJobTerminal(status model.JobStatus) bool {
	return status == model.JobSucceeded || status == model.JobFailed || status == model.JobCanceled
}

func calcProgress(tasks []model.JobTask) float64 {
	if len(tasks) == 0 {
		return 0
	}
	done := 0
	for _, t := range tasks {
		if isTaskTerminal(t.Status) {
			done++
		}
	}
	return float64(done) / float64(len(tasks))
}

func retryBackoff(attempt int) time.Duration {
	base := time.Second << max(attempt-1, 0) // 1s, 2s, 4s...
	jitter := time.Duration(rand.Int63n(int64(base / 5)))
	return base + jitter
}

func (s *Service) sleepWithCancel(jobID string, d time.Duration) bool {
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		if s.isJobCanceled(jobID) {
			return false
		}
		time.Sleep(100 * time.Millisecond)
	}
	return true
}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}
