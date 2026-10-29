package artifact

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"io"
	"os"
	"path/filepath"
	"testing"

	"daymark.local/server/internal/domain"
)

func TestImmutableChunksRejectCorruptionAndAttemptsStayIsolated(t *testing.T) {
	root := t.TempDir()
	s, e := Open(root)
	if e != nil {
		t.Fatal(e)
	}
	defer s.Close()
	id, _ := domain.NewID()
	data := []byte("synthetic immutable bytes")
	sum := sha256.Sum256(data)
	hash := hex.EncodeToString(sum[:])
	if _, e = s.Put(t.Context(), id, 0, bytes.NewReader(data), int64(len(data)), string(bytes.Repeat([]byte("0"), 64))); e == nil {
		t.Fatal("bad hash published")
	}
	chunk, e := s.Put(t.Context(), id, 0, bytes.NewReader(data), int64(len(data)), hash)
	if e != nil {
		t.Fatal(e)
	}
	if _, e = s.Put(t.Context(), id, 0, bytes.NewReader(data), int64(len(data)), hash); e != nil {
		t.Fatal("identical retry", e)
	}
	other := []byte("different")
	otherHash := sha256.Sum256(other)
	if _, e = s.Put(t.Context(), id, 0, bytes.NewReader(other), int64(len(other)), hex.EncodeToString(otherHash[:])); e == nil {
		t.Fatal("immutable chunk overwritten")
	}
	reader, e := s.OpenStored(id, chunk)
	if e != nil {
		t.Fatal(e)
	}
	read, e := io.ReadAll(reader)
	reader.Close()
	if e != nil || !bytes.Equal(read, data) {
		t.Fatal("chunk changed")
	}
	first, _, e := s.PublishAttempt(t.Context(), id, 1, []byte("attempt one"))
	if e != nil {
		t.Fatal(e)
	}
	second, _, e := s.PublishAttempt(t.Context(), id, 2, []byte("attempt two"))
	if e != nil {
		t.Fatal(e)
	}
	if first[0].ArtifactKey == second[0].ArtifactKey {
		t.Fatal("worker attempts share files")
	}
	if e = s.CleanAttempts(id, 2); e != nil {
		t.Fatal(e)
	}
	if _, e = s.OpenStored(id, first[0]); e == nil {
		t.Fatal("old attempt retained")
	}
	r, e := s.OpenStored(id, second[0])
	if e != nil {
		t.Fatal(e)
	}
	r.Close()
	if e = s.RemoveJob(domain.ID("../outside")); e == nil {
		t.Fatal("path traversal accepted")
	}
	outside := filepath.Join(t.TempDir(), "kept")
	if e = os.WriteFile(outside, []byte("kept"), 0600); e != nil {
		t.Fatal(e)
	}
	if e = s.RemoveJob(id); e != nil {
		t.Fatal(e)
	}
	if _, e = os.Stat(outside); e != nil {
		t.Fatal("escaped artifact root")
	}
}
