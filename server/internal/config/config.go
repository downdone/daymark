package config

import (
	"crypto/rand"
	"encoding/json"
	"errors"
	"io"
	"net"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"time"

	"daymark.local/server/internal/domain"
)

type Config struct {
	RegistrationEnabled  bool   `json:"registration_enabled"`
	DataDir              string `json:"data_dir"`
	Listen               string `json:"listen"`
	PublicOrigin         string `json:"public_origin"`
	TLSCert              string `json:"tls_cert"`
	TLSKey               string `json:"tls_key"`
	Development          bool   `json:"development"`
	WriterQueue          int    `json:"writer_queue"`
	ReadPool             int    `json:"read_pool"`
	RequestMS            int    `json:"request_timeout_ms"`
	BusyMS               int    `json:"busy_timeout_ms"`
	KDFParallelism       int    `json:"kdf_parallelism"`
	MaxArtifactBytes     int64  `json:"max_artifact_bytes"`
	BackupTarget         string `json:"backup_target"`
	BackupTargetExternal bool   `json:"backup_target_external"`
	BackupKeep           int    `json:"backup_keep"`
}

func Defaults() Config {
	return Config{Listen: "127.0.0.1:8080", PublicOrigin: "https://localhost", WriterQueue: 32, ReadPool: 4, RequestMS: 10000, BusyMS: 2000, KDFParallelism: 1, MaxArtifactBytes: 1073741824, BackupKeep: 48}
}
func Load(path string) (Config, error) {
	c := Defaults()
	if path != "" {
		f, e := os.Open(path)
		if e != nil {
			return c, e
		}
		defer f.Close()
		decoder := json.NewDecoder(io.LimitReader(f, 65537))
		decoder.DisallowUnknownFields()
		if e = decoder.Decode(&c); e != nil {
			return c, domain.Wrap("validation_failed", e)
		}
		var extra any
		if e = decoder.Decode(&extra); e != io.EOF {
			return c, domain.Invalid("/config")
		}
	}
	if v := os.Getenv("DAYMARK_SERVER_DATA_DIR"); v != "" {
		c.DataDir = v
	}
	if v := os.Getenv("DAYMARK_SERVER_LISTEN"); v != "" {
		c.Listen = v
	}
	if v := os.Getenv("DAYMARK_SERVER_ORIGIN"); v != "" {
		c.PublicOrigin = v
	}
	return c, nil
}
func (c Config) Validate() error {
	if c.BackupKeep < 2 || c.BackupKeep > 10000 {
		return domain.Invalid("/backup_keep")
	}
	if c.DataDir == "" || c.WriterQueue < 1 || c.WriterQueue > 1024 || c.ReadPool < 1 || c.ReadPool > 32 || c.RequestMS < 100 || c.RequestMS > 120000 || c.BusyMS < 1 || c.BusyMS >= c.RequestMS || c.KDFParallelism < 1 || c.KDFParallelism > 4 || c.MaxArtifactBytes < 268435456 {
		return domain.Invalid("/config")
	}
	host, _, e := net.SplitHostPort(c.Listen)
	if e != nil {
		return domain.Invalid("/listen")
	}
	ip := net.ParseIP(host)
	loopback := ip != nil && ip.IsLoopback()
	origin, e := url.Parse(c.PublicOrigin)
	if e != nil || origin.Host == "" || origin.User != nil || origin.RawQuery != "" || origin.Fragment != "" || origin.Path != "" && origin.Path != "/" {
		return domain.Invalid("/public_origin")
	}
	if c.Development {
		if !loopback {
			return domain.Invalid("/development")
		}
		if origin.Scheme != "http" && origin.Scheme != "https" {
			return domain.Invalid("/public_origin")
		}
		if origin.Scheme == "http" {
			p := net.ParseIP(origin.Hostname())
			if p == nil || !p.IsLoopback() {
				return domain.Invalid("/public_origin")
			}
		}
	} else if origin.Scheme != "https" {
		return domain.Invalid("/public_origin")
	}
	if (c.TLSCert == "") != (c.TLSKey == "") {
		return domain.Invalid("/tls")
	}
	if c.TLSCert == "" && !loopback {
		return domain.Invalid("/tls")
	}
	if c.BackupTarget != "" {
		if !c.BackupTargetExternal {
			return domain.Invalid("/backup_target_external")
		}
		data, e := filepath.Abs(c.DataDir)
		if e != nil {
			return e
		}
		target, e := filepath.Abs(c.BackupTarget)
		if e != nil {
			return e
		}
		rel, e := filepath.Rel(data, target)
		if e != nil {
			return e
		}
		if rel == "." || rel != ".." && !strings.HasPrefix(rel, ".."+string(filepath.Separator)) {
			return domain.Invalid("/backup_target")
		}
	}
	return nil
}
func (c Config) RequestTimeout() time.Duration { return time.Duration(c.RequestMS) * time.Millisecond }

// KeyFile never overwrites an existing key. The data-dir process lock must
// already be held, and paths are derived by the composition root only.
func KeyFile(dir, name string) ([]byte, error) {
	return SecretFile(dir, name, true)
}
func SecretFile(dir, name string, create bool) ([]byte, error) {
	if filepath.Base(name) != name || strings.ContainsAny(name, "/\\") {
		return nil, domain.Invalid("/key_name")
	}
	path := filepath.Join(dir, name)
	b, e := os.ReadFile(path)
	if e == nil {
		if len(b) != 32 {
			return nil, domain.Fail("storage_corrupt")
		}
		return b, nil
	}
	if !errors.Is(e, os.ErrNotExist) {
		return nil, e
	}
	if !create {
		return nil, domain.Wrap("storage_corrupt", e)
	}
	b = make([]byte, 32)
	if _, e = rand.Read(b); e != nil {
		return nil, e
	}
	f, e := os.OpenFile(path, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if e != nil {
		return nil, e
	}
	defer f.Close()
	if _, e = f.Write(b); e != nil {
		return nil, e
	}
	if e = f.Sync(); e != nil {
		return nil, e
	}
	return b, nil
}
