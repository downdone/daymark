package application

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"io"
	"testing"
	"time"

	"daymark.local/server/internal/artifact"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
)

func transferFixture(t *testing.T) (*fixture, *Transfer) {
	f := newFixture(t)
	files, e := artifact.Open(t.TempDir())
	if e != nil {
		t.Fatal(e)
	}
	t.Cleanup(func() { files.Close() })
	x := NewTransfer(f.db, f.auth, f.sync, f.codec, files, f.clock, 1073741824)
	f.sync.Exporter = x
	f.sync.Resolver = x
	return f, x
}
func upload(t *testing.T, f *fixture, x *Transfer, kind string, data []byte, mutation domain.ID, base domain.Counter) (domain.Job, SealedResponse) {
	t.Helper()
	in := CreateJobInput{ProtocolVersion: 1, DataEpoch: f.scope.DataEpoch, OperationID: domain.StableID("operation:" + string(mutation)), MutationID: mutation, InstallationID: f.scope.Principal.InstallationID, ClientSeq: 1, ExpectedBytes: domain.Counter(len(data)), BaseHead: base, DTOVersion: 1, SourceExportVersion: 9}
	j, e := x.Create(t.Context(), f.scope.Principal, &f.scope, kind, in)
	if e != nil {
		t.Fatal(e)
	}
	h := sha256.Sum256(data)
	hash := hex.EncodeToString(h[:])
	chunks := []domain.Chunk{}
	for off, index := 0, 0; off < len(data); index++ {
		end := min(off+262144, len(data))
		b := data[off:end]
		sum := sha256.Sum256(b)
		chunk, e := x.PutChunk(t.Context(), f.scope, j.ID, index, bytes.NewReader(b), int64(len(b)), hex.EncodeToString(sum[:]))
		if e != nil {
			t.Fatal(e)
		}
		chunks = append(chunks, chunk)
		off = end
	}
	sealed, e := x.Seal(t.Context(), f.scope, j.ID, SealInput{ProtocolVersion: 1, DataEpoch: f.scope.DataEpoch, Chunks: chunks, TotalBytes: domain.Counter(len(data)), PayloadHash: hash})
	if e != nil {
		t.Fatal(e)
	}
	return j, sealed
}
func TestSnapshotCutoffPinManifestAndDownload(t *testing.T) {
	f, x := transferFixture(t)
	id := domain.StableID("snapshot-task")
	if _, e := f.sync.Push(t.Context(), f.scope, f.mutation(t, createCommand(id, "snapshot"))); e != nil {
		t.Fatal(e)
	}
	in := CreateJobInput{ProtocolVersion: 1, DataEpoch: f.scope.DataEpoch, OperationID: domain.StableID("snapshot-operation")}
	j, e := x.Create(t.Context(), f.scope.Principal, &f.scope, "SNAPSHOT", in)
	if e != nil {
		t.Fatal(e)
	}
	same, e := x.Create(t.Context(), f.scope.Principal, &f.scope, "SNAPSHOT", in)
	if e != nil || same.ID != j.ID {
		t.Fatal("job dedup", e)
	}
	if ran, e := x.RunOne(t.Context()); e != nil || !ran {
		t.Fatal(ran, e)
	}
	m, e := x.GetManifest(t.Context(), f.scope.Principal, &f.scope, j.ID)
	if e != nil || m.Cutoff == nil || *m.Cutoff != 1 || m.EntityCount != 1 {
		t.Fatal(m, e)
	}
	var data []byte
	for i := range m.Chunks {
		chunk, r, e := x.OpenChunk(t.Context(), f.scope.Principal, &f.scope, j.ID, i)
		if e != nil {
			t.Fatal(e)
		}
		b, e := io.ReadAll(r)
		r.Close()
		if e != nil || len(b) != int(chunk.Bytes) {
			t.Fatal(e)
		}
		data = append(data, b...)
	}
	var snap domain.Snapshot
	if e = f.codec.Decode("SnapshotPayload", data, &snap); e != nil {
		t.Fatal(e)
	}
	if snap.Cutoff != 1 || len(snap.Entities) != 1 {
		t.Fatal("inconsistent snapshot")
	}
	if _, e = f.sync.Push(t.Context(), f.scope, f.mutation(t, patchCommand(id, "after cutoff"))); e != nil {
		t.Fatal(e)
	}
	page, e := f.sync.Pull(t.Context(), f.scope, *m.ResumeCursor, 128)
	if e != nil || len(page.Batches) != 1 {
		t.Fatal(page, e)
	}
	_, e = f.db.Update(t.Context(), func(tx ports.Tx) error {
		floor, e := tx.Compact(t.Context(), f.scope, 2, f.clock.Now().UnixMilli())
		if floor != 1 {
			t.Fatal("pin not enforced", floor)
		}
		return e
	})
	if e != nil {
		t.Fatal(e)
	}
	if e = x.ReleaseSnapshot(t.Context(), f.scope, j.ID, 2); e != nil {
		t.Fatal(e)
	}
}
func TestStagedMutationReplayAfterExpiry(t *testing.T) {
	f, x := transferFixture(t)
	id := domain.StableID("staged-task")
	m := f.mutation(t, createCommand(id, "staged"))
	payload, _ := json.Marshal(map[string]any{"preconditions": m.Preconditions, "commands": m.Commands})
	j, sealed := upload(t, f, x, "STAGED", payload, m.MutationID, 0)
	if sealed.Ref == nil {
		t.Fatal("no ref")
	}
	outer, _ := json.Marshal(map[string]any{"protocol_version": 1, "data_epoch": m.DataEpoch, "mutation_id": m.MutationID, "installation_id": m.InstallationID, "client_seq": m.ClientSeq, "payload_ref": sealed.Ref})
	request, e := f.codec.DecodeMutation(outer, f.scope.DatasetID)
	if e != nil {
		t.Fatal(e)
	}
	receipt, e := f.sync.Push(t.Context(), f.scope, request)
	if e != nil || receipt.Result != "committed" {
		t.Fatal(receipt, e)
	}
	f.clock.value = f.clock.value.Add(2 * time.Hour)
	fresh, e := f.auth.Refresh(t.Context(), f.pair.Refresh)
	if e != nil {
		t.Fatal(e)
	}
	principal, e := f.auth.Authenticate(t.Context(), fresh.Access)
	if e != nil {
		t.Fatal(e)
	}
	f.scope.Principal = principal
	replayed, e := f.sync.Push(t.Context(), f.scope, request)
	if e != nil || !domain.Equal(receipt, replayed) {
		t.Fatal("receipt lost with payload expiry", e)
	}
	_ = j
}
func TestAtomicImportAndRebase(t *testing.T) {
	f, x := transferFixture(t)
	id := domain.StableID("import-task")
	task := &domain.Task{ID: id, Title: "import synthetic", PlanningState: "INBOX", Status: "OPEN", CreatedAt: "2026-10-28T08:00:00.000Z", UpdatedAt: "2026-10-28T08:00:00.000Z", TagIDs: []domain.ID{}, Links: []domain.Link{}}
	input := domain.ImportPayload{DTOVersion: 1, BaseHead: 0, Entities: []domain.Entity{{Type: domain.TaskKind, ID: id, Revision: 99, Operation: "replace", Value: task}}, Aliases: []domain.Alias{}, Edges: []domain.Edge{}}
	raw, _ := json.Marshal(input)
	mid := domain.StableID("import-mutation")
	j, sealed := upload(t, f, x, "IMPORT", raw, mid, 0)
	if sealed.Ref != nil {
		t.Fatal("import must not become ordinary mutation ref")
	}
	receipt, e := x.PublishImport(t.Context(), f.scope, j.ID, domain.StableID("operation:"+string(mid)))
	if e != nil || receipt.Result != "committed" || receipt.Entities[0].Revision != 1 {
		t.Fatal(receipt, e)
	}
	same, e := x.PublishImport(t.Context(), f.scope, j.ID, domain.StableID("operation:"+string(mid)))
	if e != nil || !domain.Equal(receipt, same) {
		t.Fatal(e)
	}
	input.Entities[0].ID = domain.StableID("import-other")
	input.Entities[0].Value.(*domain.Task).ID = input.Entities[0].ID
	input.BaseHead = 1
	raw, _ = json.Marshal(input)
	mid = domain.StableID("rebase-import")
	j, _ = upload(t, f, x, "IMPORT", raw, mid, 1)
	if _, e = f.sync.Push(t.Context(), f.scope, f.mutation(t, patchCommand(id, "concurrent edit"))); e != nil {
		t.Fatal(e)
	}
	_, e = x.PublishImport(t.Context(), f.scope, j.ID, domain.StableID("operation:"+string(mid)))
	if domain.Code(e) != "import_rebase_required" {
		t.Fatal(e)
	}
	e = f.db.View(t.Context(), func(tx ports.Tx) error {
		g, e := tx.LoadGraph(t.Context(), f.scope)
		if len(g.Entities) != 1 {
			t.Fatal("partial import")
		}
		return e
	})
	if e != nil {
		t.Fatal(e)
	}
}
func TestSnapshotWorkerFencingAndExpiredCleanup(t *testing.T) {
	f, x := transferFixture(t)
	j, e := x.Create(t.Context(), f.scope.Principal, &f.scope, "SNAPSHOT", CreateJobInput{ProtocolVersion: 1, DataEpoch: f.scope.DataEpoch, OperationID: domain.StableID("fenced-snapshot")})
	if e != nil {
		t.Fatal(e)
	}
	old, e := x.claim(t.Context(), &j.ID)
	if e != nil || old == nil {
		t.Fatal(e)
	}
	f.clock.value = f.clock.value.Add(61 * time.Second)
	current, e := x.claim(t.Context(), &j.ID)
	if e != nil || current == nil || current.Attempt == old.Attempt {
		t.Fatal(e)
	}
	if e = x.build(t.Context(), *old); domain.Code(e) != "lost_lease" {
		t.Fatal(e)
	}
	if e = x.build(t.Context(), *current); e != nil {
		t.Fatal(e)
	}
	m, e := x.GetManifest(t.Context(), f.scope.Principal, &f.scope, j.ID)
	if e != nil {
		t.Fatal(e)
	}
	if len(m.Chunks) != 1 {
		t.Fatal("empty snapshot still needs valid JSON chunk")
	}
	f.clock.value = f.clock.value.Add(25 * time.Hour)
	if e = x.Reconcile(t.Context()); e != nil {
		t.Fatal(e)
	}
}
