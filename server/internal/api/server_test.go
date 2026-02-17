package api

import (
	"bytes"
	"encoding/json"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"stv/server/internal/auth"
	"stv/server/internal/events"
	"stv/server/internal/job"
	"stv/server/internal/provider"
	"stv/server/internal/store"
)

func setupTestRouter(t *testing.T) *http.ServeMux {
	t.Helper()
	st := store.NewMemoryStore()
	authSvc := auth.NewService(st, "test-secret", 15*time.Minute, 24*time.Hour)
	if err := authSvc.SeedDemoUser("demo@stv.local", "demo123456"); err != nil {
		t.Fatalf("seed user: %v", err)
	}
	hub := events.NewHub()
	jobSvc := job.NewService(st, hub, provider.NewMockAdapter(), slog.Default(), 20, 2, 6)
	s := NewServer(authSvc, st, jobSvc, hub, slog.Default())
	router := s.Router()

	mux := http.NewServeMux()
	mux.Handle("/", router)
	return mux
}

func TestLoginAndCreateProject(t *testing.T) {
	router := setupTestRouter(t)

	loginBody := map[string]any{
		"email":    "demo@stv.local",
		"password": "demo123456",
	}
	loginRaw, _ := json.Marshal(loginBody)
	loginReq := httptest.NewRequest(http.MethodPost, "/api/v1/auth/login", bytes.NewReader(loginRaw))
	loginReq.Header.Set("Content-Type", "application/json")
	loginRec := httptest.NewRecorder()
	router.ServeHTTP(loginRec, loginReq)

	if loginRec.Code != http.StatusOK {
		t.Fatalf("login status=%d body=%s", loginRec.Code, loginRec.Body.String())
	}

	var loginResp struct {
		Data struct {
			AccessToken string `json:"access_token"`
		} `json:"data"`
	}
	if err := json.Unmarshal(loginRec.Body.Bytes(), &loginResp); err != nil {
		t.Fatalf("decode login response: %v", err)
	}
	if loginResp.Data.AccessToken == "" {
		t.Fatalf("empty access token")
	}

	createBody := map[string]any{
		"name":        "test project",
		"story_text":  "hello story",
		"style":       "cinematic",
		"scene_count": 3,
	}
	createRaw, _ := json.Marshal(createBody)
	createReq := httptest.NewRequest(http.MethodPost, "/api/v1/projects", bytes.NewReader(createRaw))
	createReq.Header.Set("Content-Type", "application/json")
	createReq.Header.Set("Authorization", "Bearer "+loginResp.Data.AccessToken)
	createRec := httptest.NewRecorder()
	router.ServeHTTP(createRec, createReq)

	if createRec.Code != http.StatusCreated {
		t.Fatalf("create project status=%d body=%s", createRec.Code, createRec.Body.String())
	}
}
