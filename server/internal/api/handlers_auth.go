package api

import (
	"errors"
	"net/http"

	"stv/server/internal/auth"
	"stv/server/internal/store"

	"github.com/gin-gonic/gin"
)

type loginRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8"`
}

func (s *Server) login(c *gin.Context) {
	if !requireJSON(c) {
		return
	}
	var req loginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		writeError(c, http.StatusBadRequest, "INVALID_REQUEST", "Invalid login payload", false, nil)
		return
	}
	user, tokens, err := s.auth.Login(req.Email, req.Password)
	if err != nil {
		writeError(c, http.StatusUnauthorized, "INVALID_CREDENTIALS", "Invalid email or password", false, nil)
		return
	}
	writeData(c, http.StatusOK, gin.H{
		"access_token":  tokens.AccessToken,
		"refresh_token": tokens.RefreshToken,
		"expires_in_sec": tokens.ExpiresInSec,
		"user": gin.H{
			"id":    user.ID,
			"email": user.Email,
			"role":  user.Role,
		},
	})
}

type refreshRequest struct {
	RefreshToken string `json:"refresh_token" binding:"required"`
}

func (s *Server) refresh(c *gin.Context) {
	if !requireJSON(c) {
		return
	}
	var req refreshRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		writeError(c, http.StatusBadRequest, "INVALID_REQUEST", "refresh_token is required", false, nil)
		return
	}
	tokens, err := s.auth.Refresh(req.RefreshToken)
	if err != nil {
		if errors.Is(err, auth.ErrTokenExpired) {
			writeError(c, http.StatusUnauthorized, "TOKEN_EXPIRED", "Refresh token expired", false, nil)
			return
		}
		writeUnauthorized(c)
		return
	}
	writeData(c, http.StatusOK, gin.H{
		"access_token":  tokens.AccessToken,
		"refresh_token": tokens.RefreshToken,
		"expires_in_sec": tokens.ExpiresInSec,
	})
}

func (s *Server) logout(c *gin.Context) {
	if !requireJSON(c) {
		return
	}
	var req refreshRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		writeError(c, http.StatusBadRequest, "INVALID_REQUEST", "refresh_token is required", false, nil)
		return
	}
	if err := s.auth.Logout(req.RefreshToken); err != nil {
		writeUnauthorized(c)
		return
	}
	writeData(c, http.StatusOK, gin.H{"ok": true})
}

func (s *Server) me(c *gin.Context) {
	userID := userIDFromContext(c)
	if userID == "" {
		writeUnauthorized(c)
		return
	}
	user, err := s.store.GetUserByID(userID)
	if err != nil {
		if errors.Is(err, store.ErrNotFound) {
			writeUnauthorized(c)
			return
		}
		writeError(c, http.StatusInternalServerError, "INTERNAL_ERROR", "Failed to load user", false, nil)
		return
	}
	writeData(c, http.StatusOK, gin.H{
		"id":     user.ID,
		"email":  user.Email,
		"role":   user.Role,
		"status": user.Status,
	})
}
