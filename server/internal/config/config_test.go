package config

import (
	"bytes"
	"os"
	"path/filepath"
	"testing"
)

func TestOriginsAndProtectedBackupTargetAreExplicit(t *testing.T) {
	c := Defaults()
	c.DataDir = t.TempDir()
	if e := c.Validate(); e != nil {
		t.Fatal(e)
	}
	for _, origin := range []string{"http://example.invalid", "https://user:secret@example.invalid", "https://example.invalid/api", "https://example.invalid?x=1"} {
		x := c
		x.PublicOrigin = origin
		if x.Validate() == nil {
			t.Fatalf("unsafe origin accepted %s", origin)
		}
	}
	c.Development = true
	c.PublicOrigin = "http://127.0.0.1:8080"
	if e := c.Validate(); e != nil {
		t.Fatal(e)
	}
	c.Listen = "0.0.0.0:8080"
	if c.Validate() == nil {
		t.Fatal("development public listener accepted")
	}
	c = Defaults()
	c.DataDir = t.TempDir()
	c.BackupTarget = filepath.Join(c.DataDir, "same-failure-domain")
	c.BackupTargetExternal = true
	if c.Validate() == nil {
		t.Fatal("backup target inside live data directory accepted")
	}
	c.BackupTarget = t.TempDir()
	c.BackupTargetExternal = false
	if c.Validate() == nil {
		t.Fatal("unattested backup target accepted")
	}
	c.BackupTargetExternal = true
	if e := c.Validate(); e != nil {
		t.Fatal(e)
	}
}
func TestSecretsAreNeverSilentlyReplaced(t *testing.T) {
	dir := t.TempDir()
	if _, e := SecretFile(dir, "ledger.key", false); e == nil {
		t.Fatal("missing required key recreated")
	}
	first, e := SecretFile(dir, "ledger.key", true)
	if e != nil {
		t.Fatal(e)
	}
	second, e := SecretFile(dir, "ledger.key", true)
	if e != nil || !bytes.Equal(first, second) {
		t.Fatal("key changed")
	}
	if e = os.WriteFile(filepath.Join(dir, "ledger.key"), []byte("truncated"), 0600); e != nil {
		t.Fatal(e)
	}
	if _, e = SecretFile(dir, "ledger.key", true); e == nil {
		t.Fatal("corrupt key overwritten")
	}
	if _, e = SecretFile(dir, "../escape", true); e == nil {
		t.Fatal("key traversal accepted")
	}
}
