package api

import (
	"log/slog"

	"stv/server/internal/auth"
	"stv/server/internal/events"
	"stv/server/internal/job"
	"stv/server/internal/store"

	"github.com/gin-gonic/gin"
)

type Server struct {
	auth  *auth.Service
	store *store.MemoryStore
	jobs  *job.Service
	hub   *events.Hub
	log   *slog.Logger
}

func NewServer(authSvc *auth.Service, st *store.MemoryStore, jobs *job.Service, hub *events.Hub, logger *slog.Logger) *Server {
	return &Server{
		auth:  authSvc,
		store: st,
		jobs:  jobs,
		hub:   hub,
		log:   logger,
	}
}

func (s *Server) Router() *gin.Engine {
	r := gin.New()
	r.Use(gin.Recovery())
	r.Use(TraceMiddleware())
	r.Use(RequestLogMiddleware(s.log))

	v1 := r.Group("/api/v1")
	v1.GET("/healthz", func(c *gin.Context) {
		writeData(c, 200, gin.H{"status": "ok"})
	})

	v1.POST("/auth/login", s.login)
	v1.POST("/auth/refresh", s.refresh)

	authed := v1.Group("")
	authed.Use(AuthMiddleware(s.auth))
	{
		authed.GET("/client/bootstrap", s.clientBootstrap)
		authed.POST("/auth/logout", s.logout)
		authed.GET("/me", s.me)

		authed.POST("/projects", s.createProject)
		authed.GET("/projects", s.listProjects)
		authed.GET("/projects/:project_id", s.getProject)
		authed.PATCH("/projects/:project_id", s.patchProject)
		authed.GET("/projects/:project_id/storyboard", s.getStoryboard)
		authed.PUT("/projects/:project_id/storyboard", s.putStoryboard)
		authed.PATCH("/projects/:project_id/storyboard/scenes/:scene_id", s.patchScene)

		authed.POST("/projects/:project_id/jobs", s.startJob)
		authed.GET("/jobs/:job_id", s.getJob)
		authed.POST("/jobs/:job_id/cancel", s.cancelJob)
		authed.POST("/jobs/:job_id/retry", s.retryJob)
		authed.GET("/jobs/:job_id/events", s.streamJobEvents)

		authed.GET("/assets", s.listAssets)
		authed.GET("/assets/:asset_id", s.getAsset)

		authed.POST("/projects/:project_id/export", s.createExport)
		authed.GET("/exports/:export_id", s.getExport)
		authed.GET("/exports/:export_id/download-url", s.getExportDownloadURL)
	}

	return r
}
