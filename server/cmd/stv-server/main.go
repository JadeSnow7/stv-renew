package main

import (
	"log/slog"
	"os"

	"stv/server/internal/api"
	"stv/server/internal/auth"
	"stv/server/internal/config"
	"stv/server/internal/events"
	"stv/server/internal/job"
	"stv/server/internal/provider"
	"stv/server/internal/store"
	"stv/server/internal/telemetry"
)

func main() {
	cfg := config.Load()
	logger := telemetry.NewLogger()

	st := store.NewMemoryStore()
	authSvc := auth.NewService(st, cfg.JWTSecret, cfg.AccessTTL, cfg.RefreshTTL)
	if err := authSvc.SeedDemoUser("demo@stv.local", "demo123456"); err != nil {
		logger.Error("seed demo user failed", "error", err)
		os.Exit(1)
	}

	hub := events.NewHub()
	prov := provider.NewMockAdapter()
	jobSvc := job.NewService(st, hub, prov, logger, cfg.MaxConcurrentJob, cfg.MaxUserJobs, cfg.MaxSceneWorkers)

	srv := api.NewServer(authSvc, st, jobSvc, hub, logger)
	router := srv.Router()

	logger.Info("server_start",
		"addr", cfg.Addr,
		"demo_user", "demo@stv.local",
		"max_concurrent_tasks", cfg.MaxConcurrentJob,
		"max_user_jobs", cfg.MaxUserJobs,
	)
	if err := router.Run(cfg.Addr); err != nil {
		slog.Error("server exited with error", "error", err)
		os.Exit(1)
	}
}
