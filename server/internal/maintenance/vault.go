package maintenance

import (
	"bytes"
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io"
	"os"
	"path/filepath"

	"daymark.local/server/internal/domain"
)

const MaxBackupBytes int64 = 268435456

type Vault struct {
	root *os.Root
	key  []byte
}
type BackupHeader struct {
	Version    int       `json:"version"`
	JobID      domain.ID `json:"job_id"`
	KeyID      string    `json:"key_id"`
	SnapshotMS int64     `json:"snapshot_ms"`
}
type Archive struct {
	Header BackupHeader
	Hash   [32]byte
	Bytes  int64
	Key    string
}

func OpenVault(path string, key []byte) (*Vault, error) {
	if len(key) != 32 {
		return nil, domain.Invalid("/backup_key")
	}
	if e := os.MkdirAll(path, 0700); e != nil {
		return nil, e
	}
	r, e := os.OpenRoot(path)
	if e != nil {
		return nil, e
	}
	return &Vault{r, append([]byte{}, key...)}, nil
}
func (v *Vault) Close() error { return v.root.Close() }
func backupName(id domain.ID) (string, error) {
	if !domain.ValidID(string(id), false) {
		return "", domain.Invalid("/backup_job_id")
	}
	return string(id) + ".dmb", nil
}
func (v *Vault) aead() (cipher.AEAD, error) {
	block, e := aes.NewCipher(v.key)
	if e != nil {
		return nil, e
	}
	return cipher.NewGCM(block)
}

// AES-256-GCM is used as a standard one-shot authenticated envelope. The
// explicit 256MiB bound avoids an unaudited custom streaming crypto protocol.
func (v *Vault) Seal(ctx context.Context, id domain.ID, plainPath string, snapshotMS int64) (Archive, error) {
	name, e := backupName(id)
	if e != nil {
		return Archive{}, e
	}
	if _, e = v.root.Stat(name); e == nil {
		plain, archive, e := v.decrypt(ctx, id)
		clear(plain)
		return archive, e
	} else if !errors.Is(e, os.ErrNotExist) {
		return Archive{}, e
	}
	f, e := os.Open(plainPath)
	if e != nil {
		return Archive{}, e
	}
	plain, e := io.ReadAll(io.LimitReader(f, MaxBackupBytes+1))
	f.Close()
	if e != nil {
		return Archive{}, e
	}
	if int64(len(plain)) > MaxBackupBytes {
		return Archive{}, domain.Fail("quota_exceeded")
	}
	if e = ctx.Err(); e != nil {
		return Archive{}, e
	}
	header := BackupHeader{1, id, KeyID(v.key), snapshotMS}
	meta, _ := json.Marshal(header)
	aead, e := v.aead()
	if e != nil {
		return Archive{}, e
	}
	nonce := make([]byte, aead.NonceSize())
	if _, e = rand.Read(nonce); e != nil {
		return Archive{}, e
	}
	body := aead.Seal(nil, nonce, plain, meta)
	clear(plain)
	payload := append(append(append(append([]byte("DAYMARK1\n"), meta...), '\n'), nonce...), body...)
	hash := sha256.Sum256(payload)
	if e = publish(v.root, name, payload); e != nil {
		return Archive{}, e
	}
	return Archive{Header: header, Hash: hash, Bytes: int64(len(payload)), Key: name}, nil
}
func publish(root *os.Root, name string, data []byte) error {
	id, e := domain.NewID()
	if e != nil {
		return e
	}
	temp := ".pending-" + string(id)
	f, e := root.OpenFile(temp, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if e != nil {
		return e
	}
	defer func() { f.Close(); _ = root.Remove(temp) }()
	if _, e = f.Write(data); e != nil {
		return e
	}
	if e = f.Sync(); e != nil {
		return e
	}
	if e = f.Close(); e != nil {
		return e
	}
	if e = root.Link(temp, name); e != nil {
		return e
	}
	return nil
}
func (v *Vault) decrypt(ctx context.Context, id domain.ID) ([]byte, Archive, error) {
	name, e := backupName(id)
	if e != nil {
		return nil, Archive{}, e
	}
	f, e := v.root.Open(name)
	if e != nil {
		return nil, Archive{}, e
	}
	payload, e := io.ReadAll(io.LimitReader(f, MaxBackupBytes+4097))
	f.Close()
	if e != nil || int64(len(payload)) > MaxBackupBytes+4096 {
		return nil, Archive{}, domain.Fail("storage_corrupt")
	}
	if !bytes.HasPrefix(payload, []byte("DAYMARK1\n")) {
		return nil, Archive{}, domain.Fail("storage_corrupt")
	}
	end := bytes.IndexByte(payload[9:], '\n')
	if end < 1 || end > 2048 {
		return nil, Archive{}, domain.Fail("storage_corrupt")
	}
	end += 9
	meta := payload[9:end]
	var header BackupHeader
	if e = json.Unmarshal(meta, &header); e != nil || header.Version != 1 || header.JobID != id || header.KeyID != KeyID(v.key) {
		return nil, Archive{}, domain.Fail("storage_corrupt")
	}
	aead, e := v.aead()
	if e != nil {
		return nil, Archive{}, e
	}
	start := end + 1
	if len(payload) < start+aead.NonceSize()+aead.Overhead() {
		return nil, Archive{}, domain.Fail("storage_corrupt")
	}
	nonce := payload[start : start+aead.NonceSize()]
	plain, e := aead.Open(nil, nonce, payload[start+aead.NonceSize():], meta)
	if e != nil {
		return nil, Archive{}, domain.Wrap("storage_corrupt", e)
	}
	if e = ctx.Err(); e != nil {
		clear(plain)
		return nil, Archive{}, e
	}
	return plain, Archive{Header: header, Hash: sha256.Sum256(payload), Bytes: int64(len(payload)), Key: name}, nil
}
func (v *Vault) Extract(ctx context.Context, id domain.ID, target string) (Archive, error) {
	plain, a, e := v.decrypt(ctx, id)
	if e != nil {
		return a, e
	}
	defer clear(plain)
	f, e := os.OpenFile(target, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if e != nil {
		return a, e
	}
	defer f.Close()
	if _, e = f.Write(plain); e != nil {
		return a, e
	}
	return a, f.Sync()
}
func (v *Vault) CopyVerified(ctx context.Context, id domain.ID, target string) (string, error) {
	name, e := backupName(id)
	if e != nil {
		return "", e
	}
	if e = os.MkdirAll(target, 0700); e != nil {
		return "", e
	}
	root, e := os.OpenRoot(target)
	if e != nil {
		return "", e
	}
	defer root.Close()
	source, e := v.root.Open(name)
	if e != nil {
		return "", e
	}
	payload, e := io.ReadAll(io.LimitReader(source, MaxBackupBytes+4097))
	source.Close()
	if e != nil {
		return "", e
	}
	if int64(len(payload)) > MaxBackupBytes+4096 {
		return "", domain.Fail("quota_exceeded")
	}
	if e = ctx.Err(); e != nil {
		return "", e
	}
	sum := sha256.Sum256(payload)
	existing, e := root.ReadFile(name)
	if e == nil {
		if sha256.Sum256(existing) != sum {
			return "", domain.Fail("storage_corrupt")
		}
	} else if errors.Is(e, os.ErrNotExist) {
		if e = publish(root, name, payload); e != nil {
			return "", e
		}
	} else {
		return "", e
	}
	verified, e := root.ReadFile(name)
	if e != nil {
		return "", e
	}
	if sha256.Sum256(verified) != sum {
		return "", domain.Fail("storage_corrupt")
	}
	return hex.EncodeToString(sum[:]), nil
}
func (v *Vault) Path(id domain.ID) (string, error) {
	name, e := backupName(id)
	if e != nil {
		return "", e
	}
	return filepath.Join(v.root.Name(), name), nil
}
func (v *Vault) Verify(ctx context.Context, id domain.ID) (Archive, error) {
	plain, archive, e := v.decrypt(ctx, id)
	clear(plain)
	return archive, e
}
func (v *Vault) Remove(id domain.ID) error {
	name, e := backupName(id)
	if e != nil {
		return e
	}
	e = v.root.Remove(name)
	if errors.Is(e, os.ErrNotExist) {
		return nil
	}
	return e
}
func RemoveTargetArchive(target string, id domain.ID) error {
	name, e := backupName(id)
	if e != nil {
		return e
	}
	root, e := os.OpenRoot(target)
	if e != nil {
		return e
	}
	defer root.Close()
	e = root.Remove(name)
	if errors.Is(e, os.ErrNotExist) {
		return nil
	}
	return e
}
