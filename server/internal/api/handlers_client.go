package api

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

func (s *Server) clientBootstrap(c *gin.Context) {
	writeData(c, http.StatusOK, gin.H{
		"minimum_client_versions": gin.H{
			"windows": "0.1.0",
			"macos":   "0.1.0",
			"linux":   "0.1.0",
		},
		"feature_flags": gin.H{
			"sse_job_events":   true,
			"storyboard_editor": true,
			"asset_library":    true,
			"export_download":  true,
		},
		"sse": gin.H{
			"heartbeat_sec": 15,
			"retry_ms":      2000,
		},
	})
}
