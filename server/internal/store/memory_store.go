package store

import (
	"errors"
	"sort"
	"strings"
	"sync"
	"time"

	"stv/server/internal/model"

	"github.com/google/uuid"
)

var (
	ErrNotFound   = errors.New("not found")
	ErrConflict   = errors.New("conflict")
	ErrForbidden  = errors.New("forbidden")
	ErrBadRequest = errors.New("bad request")
)

type MemoryStore struct {
	mu sync.RWMutex

	users       map[string]model.User
	userByEmail map[string]string

	refreshTokens map[string]model.RefreshToken

	projects       map[string]model.Project
	scenesByProject map[string][]model.StoryboardScene

	jobs             map[string]model.Job
	tasksByJob       map[string][]model.JobTask
	eventsByJob      map[string][]model.JobEvent
	eventSeqByJob    map[string]int64
	idempotencyToJob map[string]string

	assets         map[string]model.Asset
	assetByProject map[string][]string

	exports map[string]model.Export
}

func NewMemoryStore() *MemoryStore {
	return &MemoryStore{
		users:            map[string]model.User{},
		userByEmail:      map[string]string{},
		refreshTokens:    map[string]model.RefreshToken{},
		projects:         map[string]model.Project{},
		scenesByProject:  map[string][]model.StoryboardScene{},
		jobs:             map[string]model.Job{},
		tasksByJob:       map[string][]model.JobTask{},
		eventsByJob:      map[string][]model.JobEvent{},
		eventSeqByJob:    map[string]int64{},
		idempotencyToJob: map[string]string{},
		assets:           map[string]model.Asset{},
		assetByProject:   map[string][]string{},
		exports:          map[string]model.Export{},
	}
}

func (s *MemoryStore) UpsertUser(user model.User) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.users[user.ID] = user
	s.userByEmail[strings.ToLower(user.Email)] = user.ID
}

func (s *MemoryStore) GetUserByEmail(email string) (model.User, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	id, ok := s.userByEmail[strings.ToLower(email)]
	if !ok {
		return model.User{}, ErrNotFound
	}
	user, ok := s.users[id]
	if !ok {
		return model.User{}, ErrNotFound
	}
	return user, nil
}

func (s *MemoryStore) GetUserByID(id string) (model.User, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	user, ok := s.users[id]
	if !ok {
		return model.User{}, ErrNotFound
	}
	return user, nil
}

func (s *MemoryStore) SaveRefreshToken(tok model.RefreshToken) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.refreshTokens[tok.ID] = tok
}

func (s *MemoryStore) GetRefreshToken(id string) (model.RefreshToken, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	tok, ok := s.refreshTokens[id]
	if !ok {
		return model.RefreshToken{}, ErrNotFound
	}
	return tok, nil
}

func (s *MemoryStore) RevokeRefreshToken(id string, revokedAt time.Time) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	tok, ok := s.refreshTokens[id]
	if !ok {
		return ErrNotFound
	}
	tok.RevokedAt = &revokedAt
	s.refreshTokens[id] = tok
	return nil
}

func (s *MemoryStore) CreateProject(project model.Project, scenes []model.StoryboardScene) (model.Project, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.projects[project.ID]; ok {
		return model.Project{}, ErrConflict
	}
	s.projects[project.ID] = project
	s.scenesByProject[project.ID] = append([]model.StoryboardScene(nil), scenes...)
	return project, nil
}

func (s *MemoryStore) ListProjects(userID string, page, pageSize int) ([]model.Project, int) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	if page < 1 {
		page = 1
	}
	if pageSize < 1 {
		pageSize = 20
	}
	var items []model.Project
	for _, p := range s.projects {
		if p.UserID == userID {
			items = append(items, p)
		}
	}
	sort.Slice(items, func(i, j int) bool { return items[i].CreatedAt.After(items[j].CreatedAt) })
	total := len(items)
	start := (page - 1) * pageSize
	if start > total {
		return []model.Project{}, total
	}
	end := start + pageSize
	if end > total {
		end = total
	}
	return append([]model.Project(nil), items[start:end]...), total
}

func (s *MemoryStore) GetProject(projectID string) (model.Project, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	p, ok := s.projects[projectID]
	if !ok {
		return model.Project{}, ErrNotFound
	}
	return p, nil
}

func (s *MemoryStore) UpdateProject(project model.Project) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.projects[project.ID]; !ok {
		return ErrNotFound
	}
	s.projects[project.ID] = project
	return nil
}

func (s *MemoryStore) GetScenes(projectID string) ([]model.StoryboardScene, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	scenes, ok := s.scenesByProject[projectID]
	if !ok {
		return nil, ErrNotFound
	}
	out := append([]model.StoryboardScene(nil), scenes...)
	sort.Slice(out, func(i, j int) bool { return out[i].SceneIndex < out[j].SceneIndex })
	return out, nil
}

func (s *MemoryStore) ReplaceScenes(projectID string, scenes []model.StoryboardScene) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.projects[projectID]; !ok {
		return ErrNotFound
	}
	out := append([]model.StoryboardScene(nil), scenes...)
	sort.Slice(out, func(i, j int) bool { return out[i].SceneIndex < out[j].SceneIndex })
	s.scenesByProject[projectID] = out
	return nil
}

func (s *MemoryStore) UpdateScene(projectID, sceneID string, updateFn func(*model.StoryboardScene) error) (model.StoryboardScene, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	scenes, ok := s.scenesByProject[projectID]
	if !ok {
		return model.StoryboardScene{}, ErrNotFound
	}
	for i := range scenes {
		if scenes[i].ID == sceneID {
			if err := updateFn(&scenes[i]); err != nil {
				return model.StoryboardScene{}, err
			}
			s.scenesByProject[projectID] = scenes
			return scenes[i], nil
		}
	}
	return model.StoryboardScene{}, ErrNotFound
}

func (s *MemoryStore) CreateJob(job model.Job, tasks []model.JobTask, idempotencyKey string) (model.Job, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if idempotencyKey != "" {
		k := job.UserID + ":" + idempotencyKey
		if existing, ok := s.idempotencyToJob[k]; ok {
			return s.jobs[existing], nil
		}
		s.idempotencyToJob[k] = job.ID
	}
	s.jobs[job.ID] = job
	s.tasksByJob[job.ID] = append([]model.JobTask(nil), tasks...)
	s.eventsByJob[job.ID] = []model.JobEvent{}
	s.eventSeqByJob[job.ID] = 0
	return job, nil
}

func (s *MemoryStore) GetJobByIdempotency(userID, idempotencyKey string) (model.Job, bool) {
	if idempotencyKey == "" {
		return model.Job{}, false
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	k := userID + ":" + idempotencyKey
	jobID, ok := s.idempotencyToJob[k]
	if !ok {
		return model.Job{}, false
	}
	job, ok := s.jobs[jobID]
	return job, ok
}

func (s *MemoryStore) GetJob(jobID string) (model.Job, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	j, ok := s.jobs[jobID]
	if !ok {
		return model.Job{}, ErrNotFound
	}
	return j, nil
}

func (s *MemoryStore) UpdateJob(job model.Job) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.jobs[job.ID]; !ok {
		return ErrNotFound
	}
	s.jobs[job.ID] = job
	return nil
}

func (s *MemoryStore) GetJobTasks(jobID string) ([]model.JobTask, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	tasks, ok := s.tasksByJob[jobID]
	if !ok {
		return nil, ErrNotFound
	}
	return append([]model.JobTask(nil), tasks...), nil
}

func (s *MemoryStore) ReplaceJobTasks(jobID string, tasks []model.JobTask) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.tasksByJob[jobID]; !ok {
		return ErrNotFound
	}
	s.tasksByJob[jobID] = append([]model.JobTask(nil), tasks...)
	return nil
}

func (s *MemoryStore) AppendJobEvent(jobID string, event model.JobEvent) (model.JobEvent, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.jobs[jobID]; !ok {
		return model.JobEvent{}, ErrNotFound
	}
	seq := s.eventSeqByJob[jobID] + 1
	s.eventSeqByJob[jobID] = seq
	event.Seq = seq
	event.EventID = uuid.NewString()
	s.eventsByJob[jobID] = append(s.eventsByJob[jobID], event)
	return event, nil
}

func (s *MemoryStore) ListJobEventsFromSeq(jobID string, fromSeq int64) ([]model.JobEvent, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	events, ok := s.eventsByJob[jobID]
	if !ok {
		return nil, ErrNotFound
	}
	if fromSeq <= 0 {
		return append([]model.JobEvent(nil), events...), nil
	}
	out := make([]model.JobEvent, 0, len(events))
	for _, e := range events {
		if e.Seq > fromSeq {
			out = append(out, e)
		}
	}
	return out, nil
}

func (s *MemoryStore) CreateAsset(asset model.Asset) (model.Asset, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.assets[asset.ID]; ok {
		return model.Asset{}, ErrConflict
	}
	s.assets[asset.ID] = asset
	s.assetByProject[asset.ProjectID] = append(s.assetByProject[asset.ProjectID], asset.ID)
	return asset, nil
}

func (s *MemoryStore) GetAsset(assetID string) (model.Asset, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	a, ok := s.assets[assetID]
	if !ok {
		return model.Asset{}, ErrNotFound
	}
	return a, nil
}

func (s *MemoryStore) ListAssets(userID, projectID string, assetType model.AssetType, page, pageSize int) ([]model.Asset, int) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	if page < 1 {
		page = 1
	}
	if pageSize < 1 {
		pageSize = 20
	}
	var out []model.Asset
	for _, a := range s.assets {
		if a.UserID != userID {
			continue
		}
		if projectID != "" && a.ProjectID != projectID {
			continue
		}
		if assetType != "" && a.Type != assetType {
			continue
		}
		out = append(out, a)
	}
	sort.Slice(out, func(i, j int) bool { return out[i].CreatedAt.After(out[j].CreatedAt) })
	total := len(out)
	start := (page - 1) * pageSize
	if start > total {
		return []model.Asset{}, total
	}
	end := start + pageSize
	if end > total {
		end = total
	}
	return append([]model.Asset(nil), out[start:end]...), total
}

func (s *MemoryStore) CreateExport(exp model.Export) (model.Export, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.exports[exp.ID] = exp
	return exp, nil
}

func (s *MemoryStore) GetExport(exportID string) (model.Export, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	exp, ok := s.exports[exportID]
	if !ok {
		return model.Export{}, ErrNotFound
	}
	return exp, nil
}

func (s *MemoryStore) UpdateExport(exp model.Export) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.exports[exp.ID]; !ok {
		return ErrNotFound
	}
	s.exports[exp.ID] = exp
	return nil
}
