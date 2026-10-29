package storage

import (
	"fmt"
	"testing"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
)

// A real file/WAL workload, with no timing threshold tied to runner hardware.
func TestTenThousandTaskImportLoadAndAtomicEdit(t *testing.T) {
	db, scope := openTest(t)
	entities := make([]domain.Entity, 10000)
	for i := range entities {
		entities[i] = sampleTask(domain.StableID(fmt.Sprintf("scale-%d", i)))
	}
	start := time.Now()
	plan, err := domain.PlanImport(domain.EmptyGraph(), domain.ImportPayload{DTOVersion: 1, Entities: entities, Aliases: []domain.Alias{}, Edges: []domain.Edge{}}, scope.DatasetID)
	if err != nil {
		t.Fatal(err)
	}
	planned := time.Now()
	if _, err = db.Update(t.Context(), func(tx ports.Tx) error { return tx.Apply(t.Context(), scope, plan) }); err != nil {
		t.Fatal(err)
	}
	written := time.Now()
	var graph domain.Graph
	if err = db.View(t.Context(), func(tx ports.Tx) error { var e error; graph, e = tx.LoadGraph(t.Context(), scope); return e }); err != nil {
		t.Fatal(err)
	}
	loaded := time.Now()
	if len(graph.Entities) != 10000 {
		t.Fatalf("lost entities: %d", len(graph.Entities))
	}
	patch := &domain.PatchTask{Event: domain.Event{Type: "task.patch", EventAt: "2026-10-29T00:00:00.000Z"}, TaskID: entities[5000].ID, Patch: domain.Patch{Title: domain.Field[string]{Present: true, Value: "updated synthetic task"}}}
	change, err := domain.PlanCommands(graph, []domain.Command{patch}, scope.DatasetID)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = db.Update(t.Context(), func(tx ports.Tx) error { return tx.Apply(t.Context(), scope, change) }); err != nil {
		t.Fatal(err)
	}
	if err = db.View(t.Context(), func(tx ports.Tx) error {
		g, e := tx.LoadGraph(t.Context(), scope)
		if e != nil {
			return e
		}
		task, _ := g.Task(entities[5000].ID)
		if task.Title != "updated synthetic task" {
			t.Fatal("edit did not persist")
		}
		return nil
	}); err != nil {
		t.Fatal(err)
	}
	t.Logf("10000 tasks: plan=%s SQLite write=%s read=%s edit+verify=%s", planned.Sub(start), written.Sub(planned), loaded.Sub(written), time.Since(loaded))
}
