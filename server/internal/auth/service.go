package auth

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"strings"
	"time"

	"stv/server/internal/model"
	"stv/server/internal/store"

	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"
)

var (
	ErrUnauthorized = errors.New("unauthorized")
	ErrTokenExpired = errors.New("token expired")
)

type Claims struct {
	UserID string         `json:"uid"`
	Email  string         `json:"email"`
	Role   model.UserRole `json:"role"`
	jwt.RegisteredClaims
}

type Tokens struct {
	AccessToken  string `json:"access_token"`
	RefreshToken string `json:"refresh_token"`
	ExpiresInSec int64  `json:"expires_in_sec"`
}

type Service struct {
	store      *store.MemoryStore
	secret     []byte
	accessTTL  time.Duration
	refreshTTL time.Duration
	now        func() time.Time
}

func NewService(st *store.MemoryStore, secret string, accessTTL, refreshTTL time.Duration) *Service {
	return &Service{
		store:      st,
		secret:     []byte(secret),
		accessTTL:  accessTTL,
		refreshTTL: refreshTTL,
		now:        time.Now,
	}
}

func (s *Service) SeedDemoUser(email, password string) error {
	if _, err := s.store.GetUserByEmail(email); err == nil {
		return nil
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return fmt.Errorf("hash demo user password: %w", err)
	}
	now := s.now().UTC()
	s.store.UpsertUser(model.User{
		ID:           uuid.NewString(),
		Email:        email,
		PasswordHash: string(hash),
		Role:         model.RoleUser,
		Status:       "active",
		CreatedAt:    now,
		UpdatedAt:    now,
	})
	return nil
}

func (s *Service) Login(email, password string) (model.User, Tokens, error) {
	user, err := s.store.GetUserByEmail(email)
	if err != nil {
		return model.User{}, Tokens{}, ErrUnauthorized
	}
	if err := bcrypt.CompareHashAndPassword([]byte(user.PasswordHash), []byte(password)); err != nil {
		return model.User{}, Tokens{}, ErrUnauthorized
	}
	tokens, err := s.issueTokens(user)
	if err != nil {
		return model.User{}, Tokens{}, err
	}
	return user, tokens, nil
}

func (s *Service) ParseAccess(tokenString string) (Claims, error) {
	token, err := jwt.ParseWithClaims(tokenString, &Claims{}, func(token *jwt.Token) (any, error) {
		return s.secret, nil
	})
	if err != nil {
		if errors.Is(err, jwt.ErrTokenExpired) {
			return Claims{}, ErrTokenExpired
		}
		return Claims{}, ErrUnauthorized
	}
	claims, ok := token.Claims.(*Claims)
	if !ok || !token.Valid {
		return Claims{}, ErrUnauthorized
	}
	return *claims, nil
}

func (s *Service) Refresh(refreshToken string) (Tokens, error) {
	tokenID, ok := parseRefreshTokenID(refreshToken)
	if !ok {
		return Tokens{}, ErrUnauthorized
	}
	stored, err := s.store.GetRefreshToken(tokenID)
	if err != nil {
		return Tokens{}, ErrUnauthorized
	}
	if stored.RevokedAt != nil {
		return Tokens{}, ErrUnauthorized
	}
	now := s.now().UTC()
	if stored.ExpiresAt.Before(now) {
		return Tokens{}, ErrTokenExpired
	}
	if !equalHash(stored.TokenHash, hashToken(refreshToken)) {
		return Tokens{}, ErrUnauthorized
	}
	user, err := s.store.GetUserByID(stored.UserID)
	if err != nil {
		return Tokens{}, ErrUnauthorized
	}
	_ = s.store.RevokeRefreshToken(stored.ID, now)
	return s.issueTokens(user)
}

func (s *Service) Logout(refreshToken string) error {
	tokenID, ok := parseRefreshTokenID(refreshToken)
	if !ok {
		return ErrUnauthorized
	}
	if err := s.store.RevokeRefreshToken(tokenID, s.now().UTC()); err != nil {
		if errors.Is(err, store.ErrNotFound) {
			return ErrUnauthorized
		}
		return err
	}
	return nil
}

func (s *Service) issueTokens(user model.User) (Tokens, error) {
	now := s.now().UTC()
	claims := Claims{
		UserID: user.ID,
		Email:  user.Email,
		Role:   user.Role,
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer:    "stv-server",
			Subject:   user.ID,
			ExpiresAt: jwt.NewNumericDate(now.Add(s.accessTTL)),
			IssuedAt:  jwt.NewNumericDate(now),
			NotBefore: jwt.NewNumericDate(now),
			ID:        uuid.NewString(),
		},
	}
	access, err := jwt.NewWithClaims(jwt.SigningMethodHS256, claims).SignedString(s.secret)
	if err != nil {
		return Tokens{}, fmt.Errorf("sign access token: %w", err)
	}
	refreshID := uuid.NewString()
	secretPart := strings.ReplaceAll(uuid.NewString(), "-", "")
	refreshToken := "rt_" + refreshID + "_" + secretPart
	rt := model.RefreshToken{
		ID:        refreshID,
		UserID:    user.ID,
		TokenHash: hashToken(refreshToken),
		ExpiresAt: now.Add(s.refreshTTL),
		CreatedAt: now,
	}
	s.store.SaveRefreshToken(rt)

	return Tokens{
		AccessToken:  access,
		RefreshToken: refreshToken,
		ExpiresInSec: int64(s.accessTTL.Seconds()),
	}, nil
}

func parseRefreshTokenID(refreshToken string) (string, bool) {
	if !strings.HasPrefix(refreshToken, "rt_") {
		return "", false
	}
	parts := strings.Split(refreshToken, "_")
	if len(parts) < 3 {
		return "", false
	}
	return parts[1], true
}

func hashToken(s string) string {
	sum := sha256.Sum256([]byte(s))
	return hex.EncodeToString(sum[:])
}

func equalHash(a, b string) bool {
	return a == b
}
