package config

import (
	"os"
	"strconv"
	"time"
)

type Config struct {
	Addr             string
	AccessTTL        time.Duration
	RefreshTTL       time.Duration
	JWTSecret        string
	MaxConcurrentJob int
	MaxUserJobs      int
	MaxSceneWorkers  int
}

func Load() Config {
	return Config{
		Addr:             env("STV_SERVER_ADDR", ":8080"),
		AccessTTL:        envDuration("STV_ACCESS_TTL", 15*time.Minute),
		RefreshTTL:       envDuration("STV_REFRESH_TTL", 14*24*time.Hour),
		JWTSecret:        env("STV_JWT_SECRET", "dev-change-me"),
		MaxConcurrentJob: envInt("STV_MAX_CONCURRENT_TASKS", 20),
		MaxUserJobs:      envInt("STV_MAX_USER_JOBS", 2),
		MaxSceneWorkers:  envInt("STV_MAX_SCENE_WORKERS", 6),
	}
}

func env(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

func envInt(key string, fallback int) int {
	if v := os.Getenv(key); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			return n
		}
	}
	return fallback
}

func envDuration(key string, fallback time.Duration) time.Duration {
	if v := os.Getenv(key); v != "" {
		if d, err := time.ParseDuration(v); err == nil {
			return d
		}
	}
	return fallback
}
