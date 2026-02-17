package auth

import (
	"testing"
	"time"

	"stv/server/internal/store"
)

func TestLoginRefreshLogout(t *testing.T) {
	st := store.NewMemoryStore()
	svc := NewService(st, "test-secret", 2*time.Minute, 24*time.Hour)
	if err := svc.SeedDemoUser("demo@stv.local", "demo123456"); err != nil {
		t.Fatalf("seed user: %v", err)
	}

	_, tokens, err := svc.Login("demo@stv.local", "demo123456")
	if err != nil {
		t.Fatalf("login failed: %v", err)
	}
	if tokens.AccessToken == "" || tokens.RefreshToken == "" {
		t.Fatalf("tokens must not be empty")
	}

	newTokens, err := svc.Refresh(tokens.RefreshToken)
	if err != nil {
		t.Fatalf("refresh failed: %v", err)
	}
	if newTokens.AccessToken == "" || newTokens.RefreshToken == "" {
		t.Fatalf("new tokens must not be empty")
	}

	if err := svc.Logout(newTokens.RefreshToken); err != nil {
		t.Fatalf("logout failed: %v", err)
	}
	if _, err := svc.Refresh(newTokens.RefreshToken); err == nil {
		t.Fatalf("refresh should fail after logout")
	}
}
