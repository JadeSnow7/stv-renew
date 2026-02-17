package api

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"time"

	"stv/server/internal/job"
	"stv/server/internal/model"
	"stv/server/internal/store"

	"github.com/gin-gonic/gin"
)

func (s *Server) startJob(c *gin.Context) {
	projectID := c.Param("project_id")
	userID := userIDFromContext(c)
	project, err := s.store.GetProject(projectID)
	if err != nil || project.UserID != userID {
		writeError(c, http.StatusNotFound, "PROJECT_NOT_FOUND", "Project not found", false, nil)
		return
	}
	idempotencyKey := c.GetHeader("Idempotency-Key")
	j, err := s.jobs.StartJob(c.Request.Context(), userID, projectID, traceIDFromContext(c), idempotencyKey)
	if err != nil {
		switch {
		case errors.Is(err, job.ErrTooManyRunningJobs):
			writeError(c, http.StatusTooManyRequests, "USER_JOB_LIMIT", "Too many running jobs", true, nil)
		default:
			writeError(c, http.StatusInternalServerError, "START_JOB_FAILED", "Failed to start job", true, nil)
		}
		return
	}
	writeData(c, http.StatusCreated, j)
}

func (s *Server) getJob(c *gin.Context) {
	jobID := c.Param("job_id")
	userID := userIDFromContext(c)
	j, err := s.jobs.GetJob(jobID)
	if err != nil {
		writeError(c, http.StatusNotFound, "JOB_NOT_FOUND", "Job not found", false, nil)
		return
	}
	if j.UserID != userID {
		writeError(c, http.StatusForbidden, "FORBIDDEN", "No access to job", false, nil)
		return
	}
	tasks, _ := s.jobs.GetJobTasks(jobID)
	writeData(c, http.StatusOK, gin.H{
		"job":   j,
		"tasks": tasks,
	})
}

func (s *Server) cancelJob(c *gin.Context) {
	jobID := c.Param("job_id")
	userID := userIDFromContext(c)
	j, err := s.jobs.CancelJob(userID, jobID)
	if err != nil {
		if errors.Is(err, store.ErrNotFound) {
			writeError(c, http.StatusNotFound, "JOB_NOT_FOUND", "Job not found", false, nil)
			return
		}
		if errors.Is(err, store.ErrForbidden) {
			writeError(c, http.StatusForbidden, "FORBIDDEN", "No access to job", false, nil)
			return
		}
		writeError(c, http.StatusInternalServerError, "CANCEL_FAILED", "Failed to cancel job", false, nil)
		return
	}
	writeData(c, http.StatusOK, j)
}

func (s *Server) retryJob(c *gin.Context) {
	jobID := c.Param("job_id")
	userID := userIDFromContext(c)
	j, err := s.jobs.RetryJob(c.Request.Context(), userID, jobID, traceIDFromContext(c))
	if err != nil {
		switch {
		case errors.Is(err, store.ErrNotFound):
			writeError(c, http.StatusNotFound, "JOB_NOT_FOUND", "Job not found", false, nil)
		case errors.Is(err, store.ErrForbidden):
			writeError(c, http.StatusForbidden, "FORBIDDEN", "No access to job", false, nil)
		case errors.Is(err, job.ErrInvalidJobState):
			writeError(c, http.StatusConflict, "INVALID_JOB_STATE", "Only failed/canceled jobs can retry", false, nil)
		case errors.Is(err, job.ErrTooManyRunningJobs):
			writeError(c, http.StatusTooManyRequests, "USER_JOB_LIMIT", "Too many running jobs", true, nil)
		default:
			writeError(c, http.StatusInternalServerError, "RETRY_FAILED", "Failed to retry job", true, nil)
		}
		return
	}
	writeData(c, http.StatusOK, j)
}

func (s *Server) streamJobEvents(c *gin.Context) {
	jobID := c.Param("job_id")
	userID := userIDFromContext(c)
	j, err := s.jobs.GetJob(jobID)
	if err != nil {
		writeError(c, http.StatusNotFound, "JOB_NOT_FOUND", "Job not found", false, nil)
		return
	}
	if j.UserID != userID {
		writeError(c, http.StatusForbidden, "FORBIDDEN", "No access to job", false, nil)
		return
	}

	fromSeq := parseLastEventSeq(c.GetHeader("Last-Event-ID"))
	if q := c.Query("from_seq"); q != "" {
		if v, err := strconv.ParseInt(q, 10, 64); err == nil && v > 0 {
			fromSeq = v
		}
	}

	backlog, _ := s.jobs.ListEventsFrom(jobID, fromSeq)
	_, sub, unsubscribe := s.hub.Subscribe(jobID, 128)
	defer unsubscribe()

	c.Writer.Header().Set("Content-Type", "text/event-stream")
	c.Writer.Header().Set("Cache-Control", "no-cache")
	c.Writer.Header().Set("Connection", "keep-alive")
	c.Writer.Header().Set("X-Accel-Buffering", "no")

	flusher, ok := c.Writer.(http.Flusher)
	if !ok {
		writeError(c, http.StatusInternalServerError, "SSE_UNSUPPORTED", "Streaming unsupported", false, nil)
		return
	}

	for _, evt := range backlog {
		writeSSE(c, evt)
	}
	flusher.Flush()

	heartbeat := time.NewTicker(15 * time.Second)
	defer heartbeat.Stop()

	for {
		select {
		case <-c.Request.Context().Done():
			return
		case evt, ok := <-sub:
			if !ok {
				return
			}
			writeSSE(c, evt)
			flusher.Flush()
		case <-heartbeat.C:
			fmt.Fprintf(c.Writer, ": ping %d\n\n", time.Now().Unix())
			flusher.Flush()
		}
	}
}

func writeSSE(c *gin.Context, evt model.JobEvent) {
	payload, _ := json.Marshal(evt)
	fmt.Fprintf(c.Writer, "id: %d\n", evt.Seq)
	fmt.Fprintf(c.Writer, "event: %s\n", evt.Type)
	fmt.Fprintf(c.Writer, "data: %s\n\n", string(payload))
}

func parseLastEventSeq(v string) int64 {
	if v == "" {
		return 0
	}
	n, err := strconv.ParseInt(v, 10, 64)
	if err != nil || n < 0 {
		return 0
	}
	return n
}
