package maintenance

import (
	"bytes"
	"os"
	"path/filepath"
	"testing"

	"daymark.local/server/internal/domain"
)

func TestLedgerDurabilityReplayPartialTailAndTampering(t *testing.T) {
	dir := t.TempDir()
	key := bytes.Repeat([]byte{42}, 32)
	ledger, e := OpenLedger(dir, key, nil, true)
	if e != nil {
		t.Fatal(e)
	}
	id := ledger.ID()
	event := domain.LedgerEvent{OperationID: domain.StableID("operation"), UserID: domain.StableID("user"), RequestedMS: 1}
	receipt, e := ledger.Append(t.Context(), event)
	if e != nil || !ValidReceipt(receipt) {
		t.Fatal(e)
	}
	same, e := ledger.Append(t.Context(), event)
	if e != nil || same != receipt {
		t.Fatal(e)
	}
	ledger.Close()
	f, e := os.OpenFile(filepath.Join(dir, "ledger.jsonl"), os.O_APPEND|os.O_WRONLY, 0600)
	if e != nil {
		t.Fatal(e)
	}
	_, _ = f.Write([]byte(`{"partial`))
	f.Close()
	ledger, e = OpenLedger(dir, key, &id, false)
	if e != nil {
		t.Fatal(e)
	}
	events, e := ledger.Read(t.Context())
	if e != nil || len(events) != 1 {
		t.Fatal(e)
	}
	ledger.Close()
	raw, e := os.ReadFile(filepath.Join(dir, "ledger.jsonl"))
	if e != nil {
		t.Fatal(e)
	}
	raw = bytes.Replace(raw, []byte(`"requested_ms":1`), []byte(`"requested_ms":2`), 1)
	if e = os.WriteFile(filepath.Join(dir, "ledger.jsonl"), raw, 0600); e != nil {
		t.Fatal(e)
	}
	if _, e = OpenLedger(dir, key, &id, false); e == nil {
		t.Fatal("tampered ledger accepted")
	}
}
func TestMissingLedgerCannotInitializeDuringRestore(t *testing.T) {
	id := domain.StableID("protected-ledger")
	if _, e := OpenLedger(filepath.Join(t.TempDir(), "missing"), bytes.Repeat([]byte{1}, 32), &id, false); e == nil {
		t.Fatal("missing ledger accepted")
	}
}
func TestAESGCMVaultIntegrityAndVerifiedCopy(t *testing.T) {
	key := bytes.Repeat([]byte{7}, 32)
	dir := t.TempDir()
	v, e := OpenVault(dir, key)
	if e != nil {
		t.Fatal(e)
	}
	defer v.Close()
	source := filepath.Join(t.TempDir(), "synthetic.sqlite")
	data := bytes.Repeat([]byte("synthetic database bytes"), 1024)
	if e = os.WriteFile(source, data, 0600); e != nil {
		t.Fatal(e)
	}
	id := domain.StableID("backup")
	archive, e := v.Seal(t.Context(), id, source, 123)
	if e != nil {
		t.Fatal(e)
	}
	target := filepath.Join(t.TempDir(), "restored.sqlite")
	same, e := v.Extract(t.Context(), id, target)
	if e != nil || same.Hash != archive.Hash {
		t.Fatal(e)
	}
	got, e := os.ReadFile(target)
	if e != nil || !bytes.Equal(got, data) {
		t.Fatal(e)
	}
	if _, e = v.CopyVerified(t.Context(), id, t.TempDir()); e != nil {
		t.Fatal(e)
	}
	path, _ := v.Path(id)
	encoded, e := os.ReadFile(path)
	if e != nil {
		t.Fatal(e)
	}
	encoded[len(encoded)-1] ^= 1
	if e = os.WriteFile(path, encoded, 0600); e != nil {
		t.Fatal(e)
	}
	if _, e = v.Extract(t.Context(), id, filepath.Join(t.TempDir(), "must-not-exist.sqlite")); e == nil {
		t.Fatal("tampered GCM payload accepted")
	}
}
