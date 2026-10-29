package protocol

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"

	"daymark.local/server/internal/domain"
)

func TestStrictJSON(t *testing.T) {
	for _, b := range [][]byte{[]byte(`{"a":1,"\u0061":2}`), []byte(`{"s":"\ud800"}`), []byte(`{"s":"\udc00"}`), []byte(`{"s":"\ud800\u0041"}`), []byte(`{} {}`), []byte(`{"n":1.0}`), {'"', 255, '"'}} {
		if _, e := StrictValue(b); e == nil {
			t.Fatalf("accepted invalid JSON %q", b)
		}
	}
	if _, e := StrictValue([]byte(`{"s":"\ud83d\ude00","a":[null,true,false,-1]}`)); e != nil {
		t.Fatal(e)
	}
}
func TestPublishedSchemaFixtures(t *testing.T) {
	c, e := NewCodec()
	if e != nil {
		t.Fatal(e)
	}
	b, e := os.ReadFile(filepath.Join("..", "..", "..", "docs", "api", "fixtures", "contract-v1.json"))
	if e != nil {
		t.Fatal(e)
	}
	var f struct {
		Cases []struct {
			ID, Schema string
			Valid      bool
			Value      json.RawMessage
		} `json:"schema_cases"`
	}
	if e = json.Unmarshal(b, &f); e != nil {
		t.Fatal(e)
	}
	if len(f.Cases) != 77 {
		t.Fatalf("fixture count %d", len(f.Cases))
	}
	for _, v := range f.Cases {
		t.Run(v.ID, func(t *testing.T) {
			x, e := StrictValue(v.Value)
			if e != nil {
				t.Fatal(e)
			}
			e = c.Validate(v.Schema, x)
			if (e == nil) != v.Valid {
				t.Fatalf("valid=%t, error=%v", v.Valid, e)
			}
		})
	}
}
func TestEmbeddedContractMatchesSource(t *testing.T) {
	b, e := os.ReadFile(filepath.Join("..", "..", "..", "docs", "api", "sync-v1.openapi.json"))
	if e != nil {
		t.Fatal(e)
	}
	if !bytes.Equal(b, apiJSON) {
		t.Fatal("embedded contract drift")
	}
}
func TestCanonicalGoldenVectors(t *testing.T) {
	b, e := os.ReadFile(filepath.Join("..", "..", "..", "docs", "api", "fixtures", "canonical-v1.json"))
	if e != nil {
		t.Fatal(e)
	}
	var f struct {
		Vectors []struct {
			Name     string          `json:"id"`
			Semantic json.RawMessage `json:"semantic"`
			Hex      string          `json:"expected_hex"`
			SHA      string          `json:"expected_sha256"`
		} `json:"cases"`
	}
	if e = json.Unmarshal(b, &f); e != nil {
		t.Fatal(e)
	}
	if len(f.Vectors) != 16 {
		t.Fatalf("vectors %d", len(f.Vectors))
	}
	for _, v := range f.Vectors {
		t.Run(v.Name, func(t *testing.T) {
			x, e := StrictValue(v.Semantic)
			if e != nil {
				t.Fatal(e)
			}
			out, e := Canonical(x)
			if e != nil {
				t.Fatal(e)
			}
			if hex.EncodeToString(out) != v.Hex {
				t.Fatal("canonical bytes differ")
			}
			h := sha256.Sum256(out)
			if hex.EncodeToString(h[:]) != v.SHA {
				t.Fatal("digest differs")
			}
		})
	}
}
func TestPatchPresenceAndFrozenDefault(t *testing.T) {
	c, e := NewCodec()
	if e != nil {
		t.Fatal(e)
	}
	var patch domain.Patch
	if e = c.Decode("TaskPatch", []byte(`{"deadline_on":null,"pinned":false,"notes":""}`), &patch); e != nil {
		t.Fatal(e)
	}
	if patch.Title.Present || !patch.DeadlineOn.Present || !patch.DeadlineOn.Null || patch.Pinned.Null || patch.Pinned.Value || !patch.Pinned.Present || !patch.Notes.Present {
		t.Fatal("three-state loss")
	}
	b, e := json.Marshal(patch)
	if e != nil {
		t.Fatal(e)
	}
	if bytes.Contains(b, []byte(`"title"`)) {
		t.Fatal("absent field emitted")
	}
}
