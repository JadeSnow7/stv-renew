package api

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

type APIError struct {
	Code      string         `json:"code"`
	Message   string         `json:"message"`
	Retryable bool           `json:"retryable"`
	Details   map[string]any `json:"details,omitempty"`
}

func writeData(c *gin.Context, status int, data any) {
	c.JSON(status, gin.H{
		"data":     data,
		"trace_id": traceIDFromContext(c),
	})
}

func writeError(c *gin.Context, status int, code, message string, retryable bool, details map[string]any) {
	c.JSON(status, gin.H{
		"error": APIError{
			Code:      code,
			Message:   message,
			Retryable: retryable,
			Details:   details,
		},
		"trace_id": traceIDFromContext(c),
	})
}

func writeUnauthorized(c *gin.Context) {
	writeError(c, http.StatusUnauthorized, "UNAUTHORIZED", "Unauthorized", false, nil)
}
