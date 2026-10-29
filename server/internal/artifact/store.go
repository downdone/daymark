package artifact

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"daymark.local/server/internal/domain"
)

type Store struct{ root *os.Root }

func Open(path string) (*Store, error) {
	if e := os.MkdirAll(path, 0700); e != nil {
		return nil, e
	}
	r, e := os.OpenRoot(path)
	if e != nil {
		return nil, e
	}
	return &Store{r}, nil
}
func (s *Store) Close() error { return s.root.Close() }
func key(job domain.ID, index int) (string, error) {
	if !domain.ValidID(string(job), false) || index < 0 || index > 65535 {
		return "", domain.Invalid("/artifact_key")
	}
	return filepath.Join(string(job), "0", fmt.Sprintf("%05d.bin", index)), nil
}

type contextReader struct {
	ctx context.Context
	r   io.Reader
}

func (r contextReader) Read(b []byte) (int, error) {
	if e := r.ctx.Err(); e != nil {
		return 0, e
	}
	return r.r.Read(b)
}
func (s *Store) Put(ctx context.Context, job domain.ID, index int, reader io.Reader, size int64, expected string) (domain.Chunk, error) {
	return s.put(ctx, job, 0, index, reader, size, expected)
}
func (s *Store) put(ctx context.Context, job domain.ID, attempt int64, index int, reader io.Reader, size int64, expected string) (domain.Chunk, error) {
	name, e := key(job, index)
	if e != nil {
		return domain.Chunk{}, e
	}
	if attempt < 0 {
		return domain.Chunk{}, domain.Invalid("/attempt")
	}
	name = filepath.Join(string(job), strconv.FormatInt(attempt, 10), fmt.Sprintf("%05d.bin", index))
	if size < 1 || size > 262144 {
		return domain.Chunk{}, domain.Fail("payload_too_large")
	}
	want, e := hex.DecodeString(expected)
	if e != nil || len(want) != 32 {
		return domain.Chunk{}, domain.Invalid("/sha256")
	}
	if e = s.root.MkdirAll(filepath.Dir(name), 0700); e != nil {
		return domain.Chunk{}, domain.Wrap("storage_io", e)
	}
	tempID, e := domain.NewID()
	if e != nil {
		return domain.Chunk{}, e
	}
	temp := filepath.Join(filepath.Dir(name), ".pending-"+string(tempID))
	f, e := s.root.OpenFile(temp, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if e != nil {
		return domain.Chunk{}, domain.Wrap("storage_io", e)
	}
	defer func() { f.Close(); _ = s.root.Remove(temp) }()
	h := sha256.New()
	n, e := io.Copy(io.MultiWriter(f, h), io.LimitReader(contextReader{ctx, reader}, size+1))
	if e != nil {
		return domain.Chunk{}, domain.Wrap("storage_io", e)
	}
	if n != size {
		return domain.Chunk{}, domain.Fail("chunk_hash_mismatch")
	}
	if hex.EncodeToString(h.Sum(nil)) != expected {
		return domain.Chunk{}, domain.Fail("chunk_hash_mismatch")
	}
	if e = f.Sync(); e != nil {
		return domain.Chunk{}, domain.Wrap("storage_io", e)
	}
	if e = f.Close(); e != nil {
		return domain.Chunk{}, domain.Wrap("storage_io", e)
	}
	// Hard-link publication is atomic and never overwrites a competing chunk.
	if e = s.root.Link(temp, name); e != nil {
		if !errors.Is(e, os.ErrExist) {
			return domain.Chunk{}, domain.Wrap("storage_io", e)
		}
		existing, e := s.root.Open(name)
		if e != nil {
			return domain.Chunk{}, e
		}
		old := sha256.New()
		length, e := io.Copy(old, io.LimitReader(existing, 262145))
		existing.Close()
		if e != nil || length != size || hex.EncodeToString(old.Sum(nil)) != expected {
			return domain.Chunk{}, domain.Fail("chunk_hash_mismatch")
		}
	}
	return domain.Chunk{Index: index, Bytes: domain.Counter(size), Hash: expected, ArtifactKey: filepath.ToSlash(name)}, nil
}
func (s *Store) OpenChunk(job domain.ID, index int) (io.ReadCloser, error) {
	name, e := key(job, index)
	if e != nil {
		return nil, e
	}
	f, e := s.root.Open(name)
	if e != nil {
		return nil, domain.Wrap("storage_io", e)
	}
	return f, nil
}
func (s *Store) Read(ctx context.Context, job domain.ID, chunks []domain.Chunk, limit int64) ([]byte, error) {
	out := []byte{}
	if len(chunks) == 0 {
		return nil, domain.Fail("chunk_hash_mismatch")
	}
	for index, c := range chunks {
		if c.Index != index || int64(c.Bytes) < 1 || int64(c.Bytes) > 262144 || int64(len(out))+int64(c.Bytes) > limit {
			return nil, domain.Fail("payload_too_large")
		}
		f, e := s.OpenStored(job, c)
		if e != nil {
			return nil, e
		}
		b, e := io.ReadAll(io.LimitReader(contextReader{ctx, f}, 262145))
		f.Close()
		if e != nil {
			return nil, e
		}
		h := sha256.Sum256(b)
		if int64(len(b)) != int64(c.Bytes) || hex.EncodeToString(h[:]) != c.Hash {
			return nil, domain.Fail("chunk_hash_mismatch")
		}
		out = append(out, b...)
	}
	return out, nil
}
func (s *Store) Publish(ctx context.Context, job domain.ID, data []byte) ([]domain.Chunk, string, error) {
	return s.PublishAttempt(ctx, job, 0, data)
}
func (s *Store) PublishAttempt(ctx context.Context, job domain.ID, attempt int64, data []byte) ([]domain.Chunk, string, error) {
	if len(data) == 0 {
		return nil, "", domain.Invalid("/payload")
	}
	chunks := []domain.Chunk{}
	for offset, index := 0, 0; offset < len(data); index++ {
		end := min(offset+262144, len(data))
		b := data[offset:end]
		h := sha256.Sum256(b)
		chunk, e := s.put(ctx, job, attempt, index, bytesReader(b), int64(len(b)), hex.EncodeToString(h[:]))
		if e != nil {
			return nil, "", e
		}
		chunks = append(chunks, chunk)
		offset = end
	}
	h := sha256.Sum256(data)
	return chunks, hex.EncodeToString(h[:]), nil
}

type byteReader struct{ b []byte }

func (s *Store) OpenStored(job domain.ID, chunk domain.Chunk) (io.ReadCloser, error) {
	if !domain.ValidID(string(job), false) {
		return nil, domain.Invalid("/artifact_key")
	}
	parts := strings.Split(chunk.ArtifactKey, "/")
	if len(parts) != 3 || parts[0] != string(job) || parts[2] != fmt.Sprintf("%05d.bin", chunk.Index) {
		return nil, domain.Invalid("/artifact_key")
	}
	n, e := strconv.ParseInt(parts[1], 10, 64)
	if e != nil || n < 0 || strconv.FormatInt(n, 10) != parts[1] {
		return nil, domain.Invalid("/artifact_key")
	}
	return s.root.Open(filepath.FromSlash(chunk.ArtifactKey))
}
func (s *Store) CleanAttempts(job domain.ID, current int64) error {
	if !domain.ValidID(string(job), false) || current < 1 {
		return domain.Invalid("/artifact_key")
	}
	entries, e := fs.ReadDir(s.root.FS(), string(job))
	if errors.Is(e, os.ErrNotExist) {
		return nil
	}
	if e != nil {
		return e
	}
	for _, entry := range entries {
		n, e := strconv.ParseInt(entry.Name(), 10, 64)
		if e == nil && n > 0 && n < current && strconv.FormatInt(n, 10) == entry.Name() {
			if e = s.root.RemoveAll(filepath.Join(string(job), entry.Name())); e != nil {
				return e
			}
		}
	}
	return nil
}

func bytesReader(b []byte) *byteReader { return &byteReader{b} }
func (r *byteReader) Read(out []byte) (int, error) {
	if len(r.b) == 0 {
		return 0, io.EOF
	}
	n := copy(out, r.b)
	r.b = r.b[n:]
	return n, nil
}
func (s *Store) RemoveJob(job domain.ID) error {
	if !domain.ValidID(string(job), false) {
		return domain.Invalid("/artifact_key")
	}
	return s.root.RemoveAll(string(job))
}
func (s *Store) Orphans(known map[domain.ID]bool, before time.Time) ([]domain.ID, error) {
	entries, e := fs.ReadDir(s.root.FS(), ".")
	if e != nil {
		return nil, e
	}
	out := []domain.ID{}
	for _, entry := range entries {
		if !entry.IsDir() || !domain.ValidID(entry.Name(), false) || known[domain.ID(entry.Name())] {
			continue
		}
		info, e := entry.Info()
		if e != nil {
			return nil, e
		}
		if info.ModTime().Before(before) {
			out = append(out, domain.ID(entry.Name()))
		}
	}
	return out, nil
}
