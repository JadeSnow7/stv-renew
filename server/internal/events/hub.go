package events

import (
	"sync"

	"stv/server/internal/model"

	"github.com/google/uuid"
)

type Hub struct {
	mu   sync.RWMutex
	subs map[string]map[string]chan model.JobEvent
}

func NewHub() *Hub {
	return &Hub{
		subs: map[string]map[string]chan model.JobEvent{},
	}
}

func (h *Hub) Subscribe(jobID string, buf int) (string, <-chan model.JobEvent, func()) {
	h.mu.Lock()
	defer h.mu.Unlock()
	subID := uuid.NewString()
	if _, ok := h.subs[jobID]; !ok {
		h.subs[jobID] = map[string]chan model.JobEvent{}
	}
	ch := make(chan model.JobEvent, buf)
	h.subs[jobID][subID] = ch

	unsubscribe := func() {
		h.mu.Lock()
		defer h.mu.Unlock()
		jobSubs, ok := h.subs[jobID]
		if !ok {
			return
		}
		c, ok := jobSubs[subID]
		if !ok {
			return
		}
		delete(jobSubs, subID)
		close(c)
		if len(jobSubs) == 0 {
			delete(h.subs, jobID)
		}
	}
	return subID, ch, unsubscribe
}

func (h *Hub) Publish(jobID string, evt model.JobEvent) {
	h.mu.RLock()
	defer h.mu.RUnlock()
	jobSubs, ok := h.subs[jobID]
	if !ok {
		return
	}
	for _, ch := range jobSubs {
		select {
		case ch <- evt:
		default:
			// Drop stale subscribers to keep producer non-blocking.
		}
	}
}
