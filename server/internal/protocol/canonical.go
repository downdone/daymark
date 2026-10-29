package protocol

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"encoding/json"
	"io"
	"math"
	"sort"
	"strconv"

	"daymark.local/server/internal/domain"
)

func Canonical(v any) ([]byte, error) {
	var b bytes.Buffer
	if e := encode(&b, v); e != nil {
		return nil, e
	}
	return b.Bytes(), nil
}
func encode(w io.Writer, v any) error {
	put := func(b []byte) error { _, e := w.Write(b); return e }
	count := func(n int) error {
		if uint64(n) > math.MaxUint32 {
			return domain.Fail("payload_too_large")
		}
		var b [4]byte
		binary.BigEndian.PutUint32(b[:], uint32(n))
		return put(b[:])
	}
	switch x := v.(type) {
	case nil:
		return put([]byte{0})
	case bool:
		if x {
			return put([]byte{2})
		}
		return put([]byte{1})
	case string:
		if e := put([]byte{3}); e != nil {
			return e
		}
		if e := count(len(x)); e != nil {
			return e
		}
		return put([]byte(x))
	case json.Number:
		n, e := strconv.ParseInt(string(x), 10, 64)
		if e != nil {
			return domain.Invalid("/integer")
		}
		return encode(w, n)
	case int:
		return encode(w, int64(x))
	case int64:
		var b [9]byte
		b[0] = 4
		binary.BigEndian.PutUint64(b[1:], uint64(x))
		return put(b[:])
	case map[string]any:
		if e := put([]byte{5}); e != nil {
			return e
		}
		if e := count(len(x)); e != nil {
			return e
		}
		keys := make([]string, 0, len(x))
		for k := range x {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		for _, k := range keys {
			if e := encode(w, k); e != nil {
				return e
			}
			if e := encode(w, x[k]); e != nil {
				return e
			}
		}
		return nil
	case []any:
		if e := put([]byte{6}); e != nil {
			return e
		}
		if e := count(len(x)); e != nil {
			return e
		}
		for _, v := range x {
			if e := encode(w, v); e != nil {
				return e
			}
		}
		return nil
	default:
		return domain.Invalid("/canonical_type")
	}
}
func Digest(v any) ([32]byte, error) {
	b, e := Canonical(v)
	if e != nil {
		return [32]byte{}, e
	}
	return sha256.Sum256(b), nil
}
func Value(v any) (any, error) {
	b, e := json.Marshal(v)
	if e != nil {
		return nil, e
	}
	return StrictValue(b)
}
func canonicalSets(m map[string]any) error {
	var walk func(any, string) error
	walk = func(v any, key string) error {
		switch x := v.(type) {
		case map[string]any:
			if x["type"] == "task.create" {
				if _, ok := x["initial"]; !ok {
					x["initial"] = map[string]any{}
				}
			}
			for k, v := range x {
				if e := walk(v, k); e != nil {
					return e
				}
			}
		case []any:
			for _, v := range x {
				if e := walk(v, ""); e != nil {
					return e
				}
			}
			fields := []string{}
			switch key {
			case "tag_ids", "task_ids", "member_ids":
				fields = []string{""}
			case "links":
				fields = []string{"target_id", "role"}
			case "entities":
				fields = []string{"type", "id"}
			case "structures", "structure_revisions":
				fields = []string{"kind", "id"}
			case "aliases":
				fields = []string{"alias_id"}
			case "successor_edges":
				fields = []string{"rule_id", "predecessor_id"}
			}
			if len(fields) > 0 {
				itemKey := func(v any) string {
					if fields[0] == "" {
						return v.(string)
					}
					o := v.(map[string]any)
					s := ""
					for _, f := range fields {
						s += o[f].(string) + "\x00"
					}
					return s
				}
				sort.Slice(x, func(i, j int) bool { return itemKey(x[i]) < itemKey(x[j]) })
				for i := 1; i < len(x); i++ {
					if itemKey(x[i]) == itemKey(x[i-1]) {
						return domain.Invalid("/" + key)
					}
				}
			}
		}
		return nil
	}
	return walk(m, "")
}
func NormalizeSets(m map[string]any) error { return canonicalSets(m) }
