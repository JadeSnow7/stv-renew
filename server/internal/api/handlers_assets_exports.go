package api

import (
	"fmt"
	"net/http"
	"time"

	"stv/server/internal/model"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

func (s *Server) listAssets(c *gin.Context) {
	userID := userIDFromContext(c)
	projectID := c.Query("project_id")
	assetType := model.AssetType(c.Query("type"))
	page := parseIntDefault(c.Query("page"), 1)
	pageSize := parseIntDefault(c.Query("page_size"), 20)
	items, total := s.store.ListAssets(userID, projectID, assetType, page, pageSize)
	writeData(c, http.StatusOK, gin.H{
		"items":     items,
		"page":      page,
		"page_size": pageSize,
		"total":     total,
	})
}

func (s *Server) getAsset(c *gin.Context) {
	assetID := c.Param("asset_id")
	userID := userIDFromContext(c)
	asset, err := s.store.GetAsset(assetID)
	if err != nil {
		writeError(c, http.StatusNotFound, "ASSET_NOT_FOUND", "Asset not found", false, nil)
		return
	}
	if asset.UserID != userID {
		writeError(c, http.StatusForbidden, "FORBIDDEN", "No access to asset", false, nil)
		return
	}
	writeData(c, http.StatusOK, asset)
}

func (s *Server) createExport(c *gin.Context) {
	projectID := c.Param("project_id")
	userID := userIDFromContext(c)
	project, err := s.store.GetProject(projectID)
	if err != nil || project.UserID != userID {
		writeError(c, http.StatusNotFound, "PROJECT_NOT_FOUND", "Project not found", false, nil)
		return
	}
	now := time.Now().UTC()
	exp := model.Export{
		ID:        uuid.NewString(),
		UserID:    userID,
		ProjectID: projectID,
		Status:    model.ExportQueued,
		CreatedAt: now,
		UpdatedAt: now,
	}
	if _, err := s.store.CreateExport(exp); err != nil {
		writeError(c, http.StatusInternalServerError, "EXPORT_CREATE_FAILED", "Failed to create export", true, nil)
		return
	}
	go s.resolveExport(exp.ID)
	writeData(c, http.StatusAccepted, exp)
}

func (s *Server) getExport(c *gin.Context) {
	exportID := c.Param("export_id")
	userID := userIDFromContext(c)
	exp, err := s.store.GetExport(exportID)
	if err != nil {
		writeError(c, http.StatusNotFound, "EXPORT_NOT_FOUND", "Export not found", false, nil)
		return
	}
	if exp.UserID != userID {
		writeError(c, http.StatusForbidden, "FORBIDDEN", "No access to export", false, nil)
		return
	}
	writeData(c, http.StatusOK, exp)
}

func (s *Server) getExportDownloadURL(c *gin.Context) {
	exportID := c.Param("export_id")
	userID := userIDFromContext(c)
	exp, err := s.store.GetExport(exportID)
	if err != nil {
		writeError(c, http.StatusNotFound, "EXPORT_NOT_FOUND", "Export not found", false, nil)
		return
	}
	if exp.UserID != userID {
		writeError(c, http.StatusForbidden, "FORBIDDEN", "No access to export", false, nil)
		return
	}
	if exp.Status != model.ExportSucceeded {
		writeError(c, http.StatusConflict, "EXPORT_NOT_READY", "Export is not ready", true, gin.H{"status": exp.Status})
		return
	}
	writeData(c, http.StatusOK, gin.H{
		"export_id":     exp.ID,
		"download_url":  exp.DownloadURL,
		"expires_in_sec": 600,
	})
}

func (s *Server) resolveExport(exportID string) {
	exp, err := s.store.GetExport(exportID)
	if err != nil {
		return
	}
	exp.Status = model.ExportRunning
	exp.UpdatedAt = time.Now().UTC()
	_ = s.store.UpdateExport(exp)

	deadline := time.Now().Add(30 * time.Second)
	for time.Now().Before(deadline) {
		assets, _ := s.store.ListAssets(exp.UserID, exp.ProjectID, model.AssetFinalVideo, 1, 1)
		if len(assets) > 0 {
			exp.Status = model.ExportSucceeded
			exp.AssetID = assets[0].ID
			exp.DownloadURL = signedURL(assets[0])
			exp.UpdatedAt = time.Now().UTC()
			_ = s.store.UpdateExport(exp)
			return
		}
		time.Sleep(500 * time.Millisecond)
	}
	exp.Status = model.ExportFailed
	exp.UpdatedAt = time.Now().UTC()
	_ = s.store.UpdateExport(exp)
}

func signedURL(asset model.Asset) string {
	return fmt.Sprintf("https://storage.local/%s?expires_in=600&token=%s", asset.StorageKey, uuid.NewString())
}
