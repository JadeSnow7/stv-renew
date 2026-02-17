package api

import (
	"log/slog"
	"net/http"
	"strings"
	"time"

	"stv/server/internal/auth"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

const (
	ctxTraceID = "trace_id"
	ctxUserID  = "user_id"
	ctxEmail   = "email"
	ctxRole    = "role"
)

func TraceMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		traceID := strings.TrimSpace(c.GetHeader("X-Trace-Id"))
		if traceID == "" {
			if v7, err := uuid.NewV7(); err == nil {
				traceID = v7.String()
			} else {
				traceID = uuid.NewString()
			}
		}
		c.Set(ctxTraceID, traceID)
		c.Writer.Header().Set("X-Trace-Id", traceID)
		c.Next()
	}
}

func RequestLogMiddleware(logger *slog.Logger) gin.HandlerFunc {
	return func(c *gin.Context) {
		start := time.Now()
		c.Next()
		latency := time.Since(start)
		logger.Info("http_request",
			"trace_id", traceIDFromContext(c),
			"method", c.Request.Method,
			"path", c.Request.URL.Path,
			"status", c.Writer.Status(),
			"latency_ms", latency.Milliseconds(),
		)
	}
}

func AuthMiddleware(authSvc *auth.Service) gin.HandlerFunc {
	return func(c *gin.Context) {
		header := c.GetHeader("Authorization")
		if header == "" {
			writeUnauthorized(c)
			c.Abort()
			return
		}
		const prefix = "Bearer "
		if !strings.HasPrefix(header, prefix) {
			writeUnauthorized(c)
			c.Abort()
			return
		}
		token := strings.TrimSpace(strings.TrimPrefix(header, prefix))
		claims, err := authSvc.ParseAccess(token)
		if err != nil {
			writeUnauthorized(c)
			c.Abort()
			return
		}
		c.Set(ctxUserID, claims.UserID)
		c.Set(ctxEmail, claims.Email)
		c.Set(ctxRole, string(claims.Role))
		c.Next()
	}
}

func traceIDFromContext(c *gin.Context) string {
	if v, ok := c.Get(ctxTraceID); ok {
		if s, ok := v.(string); ok {
			return s
		}
	}
	return ""
}

func userIDFromContext(c *gin.Context) string {
	if v, ok := c.Get(ctxUserID); ok {
		if s, ok := v.(string); ok {
			return s
		}
	}
	return ""
}

func requireJSON(c *gin.Context) bool {
	if c.ContentType() == "" {
		return true
	}
	if strings.Contains(c.ContentType(), "application/json") {
		return true
	}
	writeError(c, http.StatusUnsupportedMediaType, "UNSUPPORTED_MEDIA_TYPE", "Content-Type must be application/json", false, nil)
	return false
}
