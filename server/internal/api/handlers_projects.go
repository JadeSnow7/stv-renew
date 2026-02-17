package api

import (
	"errors"
	"fmt"
	"net/http"
	"sort"
	"strings"
	"time"

	"stv/server/internal/model"
	"stv/server/internal/store"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

type createProjectRequest struct {
	Name       string `json:"name" binding:"required"`
	StoryText  string `json:"story_text" binding:"required"`
	Style      string `json:"style" binding:"required"`
	SceneCount int    `json:"scene_count" binding:"required,min=1,max=20"`
}

func (s *Server) createProject(c *gin.Context) {
	if !requireJSON(c) {
		return
	}
	userID := userIDFromContext(c)
	var req createProjectRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		writeError(c, http.StatusBadRequest, "INVALID_REQUEST", "Invalid project payload", false, nil)
		return
	}
	now := time.Now().UTC()
	project := model.Project{
		ID:         uuid.NewString(),
		UserID:     userID,
		Name:       strings.TrimSpace(req.Name),
		StoryText:  req.StoryText,
		Style:      req.Style,
		SceneCount: req.SceneCount,
		Status:     "draft",
		CreatedAt:  now,
		UpdatedAt:  now,
	}
	scenes := make([]model.StoryboardScene, 0, req.SceneCount)
	for i := 0; i < req.SceneCount; i++ {
		scenes = append(scenes, model.StoryboardScene{
			ID:             uuid.NewString(),
			ProjectID:      project.ID,
			SceneIndex:     i,
			Narration:      "",
			Prompt:         "",
			NegativePrompt: "",
			DurationMS:     3000,
			Transition:     "cut",
			Locked:         false,
			UpdatedAt:      now,
		})
	}
	created, err := s.store.CreateProject(project, scenes)
	if err != nil {
		if errors.Is(err, store.ErrConflict) {
			writeError(c, http.StatusConflict, "PROJECT_CONFLICT", "Project already exists", false, nil)
			return
		}
		writeError(c, http.StatusInternalServerError, "INTERNAL_ERROR", "Failed to create project", false, nil)
		return
	}
	writeData(c, http.StatusCreated, created)
}

func (s *Server) listProjects(c *gin.Context) {
	userID := userIDFromContext(c)
	page := parseIntDefault(c.Query("page"), 1)
	pageSize := parseIntDefault(c.Query("page_size"), 20)
	items, total := s.store.ListProjects(userID, page, pageSize)
	writeData(c, http.StatusOK, gin.H{
		"items":     items,
		"page":      page,
		"page_size": pageSize,
		"total":     total,
	})
}

func (s *Server) getProject(c *gin.Context) {
	projectID := c.Param("project_id")
	userID := userIDFromContext(c)
	project, err := s.store.GetProject(projectID)
	if err != nil {
		writeError(c, http.StatusNotFound, "PROJECT_NOT_FOUND", "Project not found", false, nil)
		return
	}
	if project.UserID != userID {
		writeError(c, http.StatusForbidden, "FORBIDDEN", "No access to project", false, nil)
		return
	}
	writeData(c, http.StatusOK, project)
}

type patchProjectRequest struct {
	Name      *string `json:"name"`
	StoryText *string `json:"story_text"`
	Style     *string `json:"style"`
}

func (s *Server) patchProject(c *gin.Context) {
	if !requireJSON(c) {
		return
	}
	projectID := c.Param("project_id")
	userID := userIDFromContext(c)
	project, err := s.store.GetProject(projectID)
	if err != nil {
		writeError(c, http.StatusNotFound, "PROJECT_NOT_FOUND", "Project not found", false, nil)
		return
	}
	if project.UserID != userID {
		writeError(c, http.StatusForbidden, "FORBIDDEN", "No access to project", false, nil)
		return
	}
	var req patchProjectRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		writeError(c, http.StatusBadRequest, "INVALID_REQUEST", "Invalid patch payload", false, nil)
		return
	}
	if req.Name != nil {
		project.Name = strings.TrimSpace(*req.Name)
	}
	if req.StoryText != nil {
		project.StoryText = *req.StoryText
	}
	if req.Style != nil {
		project.Style = *req.Style
	}
	project.UpdatedAt = time.Now().UTC()
	if err := s.store.UpdateProject(project); err != nil {
		writeError(c, http.StatusInternalServerError, "INTERNAL_ERROR", "Failed to update project", false, nil)
		return
	}
	writeData(c, http.StatusOK, project)
}

func (s *Server) getStoryboard(c *gin.Context) {
	projectID := c.Param("project_id")
	userID := userIDFromContext(c)
	project, err := s.store.GetProject(projectID)
	if err != nil || project.UserID != userID {
		writeError(c, http.StatusNotFound, "PROJECT_NOT_FOUND", "Project not found", false, nil)
		return
	}
	scenes, err := s.store.GetScenes(projectID)
	if err != nil {
		writeError(c, http.StatusNotFound, "STORYBOARD_NOT_FOUND", "Storyboard not found", false, nil)
		return
	}
	writeData(c, http.StatusOK, gin.H{
		"project_id": projectID,
		"scenes":     scenes,
	})
}

type storyboardScenePayload struct {
	ID             string `json:"id"`
	SceneIndex     int    `json:"scene_index"`
	Narration      string `json:"narration"`
	Prompt         string `json:"prompt"`
	NegativePrompt string `json:"negative_prompt"`
	DurationMS     int    `json:"duration_ms"`
	Transition     string `json:"transition"`
	Locked         bool   `json:"locked"`
}

type putStoryboardRequest struct {
	Scenes []storyboardScenePayload `json:"scenes" binding:"required"`
}

func (s *Server) putStoryboard(c *gin.Context) {
	if !requireJSON(c) {
		return
	}
	projectID := c.Param("project_id")
	userID := userIDFromContext(c)
	project, err := s.store.GetProject(projectID)
	if err != nil || project.UserID != userID {
		writeError(c, http.StatusNotFound, "PROJECT_NOT_FOUND", "Project not found", false, nil)
		return
	}
	var req putStoryboardRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		writeError(c, http.StatusBadRequest, "INVALID_REQUEST", "Invalid storyboard payload", false, nil)
		return
	}
	now := time.Now().UTC()
	scenes := make([]model.StoryboardScene, 0, len(req.Scenes))
	for i, in := range req.Scenes {
		sceneID := in.ID
		if sceneID == "" {
			sceneID = uuid.NewString()
		}
		duration := in.DurationMS
		if duration <= 0 {
			duration = 3000
		}
		scenes = append(scenes, model.StoryboardScene{
			ID:             sceneID,
			ProjectID:      projectID,
			SceneIndex:     i,
			Narration:      in.Narration,
			Prompt:         in.Prompt,
			NegativePrompt: in.NegativePrompt,
			DurationMS:     duration,
			Transition:     fallback(in.Transition, "cut"),
			Locked:         in.Locked,
			UpdatedAt:      now,
		})
	}
	if err := s.store.ReplaceScenes(projectID, scenes); err != nil {
		writeError(c, http.StatusInternalServerError, "INTERNAL_ERROR", "Failed to replace storyboard", false, nil)
		return
	}
	project.SceneCount = len(scenes)
	project.UpdatedAt = now
	_ = s.store.UpdateProject(project)
	writeData(c, http.StatusOK, gin.H{
		"project_id": projectID,
		"scenes":     scenes,
	})
}

type patchSceneRequest struct {
	Narration      *string `json:"narration"`
	Prompt         *string `json:"prompt"`
	NegativePrompt *string `json:"negative_prompt"`
	DurationMS     *int    `json:"duration_ms"`
	Transition     *string `json:"transition"`
	Locked         *bool   `json:"locked"`
	SceneIndex     *int    `json:"scene_index"`
}

func (s *Server) patchScene(c *gin.Context) {
	if !requireJSON(c) {
		return
	}
	projectID := c.Param("project_id")
	sceneID := c.Param("scene_id")
	userID := userIDFromContext(c)

	project, err := s.store.GetProject(projectID)
	if err != nil || project.UserID != userID {
		writeError(c, http.StatusNotFound, "PROJECT_NOT_FOUND", "Project not found", false, nil)
		return
	}

	var req patchSceneRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		writeError(c, http.StatusBadRequest, "INVALID_REQUEST", "Invalid patch payload", false, nil)
		return
	}

	scene, err := s.store.UpdateScene(projectID, sceneID, func(sc *model.StoryboardScene) error {
		if req.Narration != nil {
			sc.Narration = *req.Narration
		}
		if req.Prompt != nil {
			sc.Prompt = *req.Prompt
		}
		if req.NegativePrompt != nil {
			sc.NegativePrompt = *req.NegativePrompt
		}
		if req.DurationMS != nil && *req.DurationMS > 0 {
			sc.DurationMS = *req.DurationMS
		}
		if req.Transition != nil {
			sc.Transition = *req.Transition
		}
		if req.Locked != nil {
			sc.Locked = *req.Locked
		}
		if req.SceneIndex != nil && *req.SceneIndex >= 0 {
			sc.SceneIndex = *req.SceneIndex
		}
		sc.UpdatedAt = time.Now().UTC()
		return nil
	})
	if err != nil {
		writeError(c, http.StatusNotFound, "SCENE_NOT_FOUND", "Scene not found", false, nil)
		return
	}

	// Normalize ordering when scene_index changes.
	scenes, err := s.store.GetScenes(projectID)
	if err == nil {
		sort.SliceStable(scenes, func(i, j int) bool { return scenes[i].SceneIndex < scenes[j].SceneIndex })
		for i := range scenes {
			scenes[i].SceneIndex = i
		}
		_ = s.store.ReplaceScenes(projectID, scenes)
	}
	writeData(c, http.StatusOK, scene)
}

func parseIntDefault(raw string, fallback int) int {
	if raw == "" {
		return fallback
	}
	var n int
	_, err := fmt.Sscanf(raw, "%d", &n)
	if err != nil || n <= 0 {
		return fallback
	}
	return n
}

func fallback(v, def string) string {
	if strings.TrimSpace(v) == "" {
		return def
	}
	return v
}
