package protocol

import (
	"bytes"
	"encoding/json"
	"io"
	"strconv"
	"unicode/utf8"

	"daymark.local/server/internal/domain"
)

// StrictValue rejects corruption BEFORE encoding/json can replace invalid text.
// Object keys are compared after unescaping, so "a" and "\u0061" conflict.
func StrictValue(data []byte) (any, error) {
	if !utf8.Valid(data) || !validEscapes(data) {
		return nil, domain.Fail("malformed_request")
	}
	d := json.NewDecoder(bytes.NewReader(data))
	d.UseNumber()
	nodes := 0
	var read func(int) (any, error)
	read = func(depth int) (any, error) {
		nodes++
		if depth > 64 || nodes > 2000000 {
			return nil, domain.Fail("payload_too_large")
		}
		t, e := d.Token()
		if e != nil {
			return nil, domain.Wrap("malformed_request", e)
		}
		switch t := t.(type) {
		case json.Delim:
			switch t {
			case '{':
				m := map[string]any{}
				for d.More() {
					k, e := d.Token()
					if e != nil {
						return nil, domain.Fail("malformed_request")
					}
					key, ok := k.(string)
					if !ok {
						return nil, domain.Fail("malformed_request")
					}
					if _, ok = m[key]; ok {
						return nil, domain.Fail("malformed_request")
					}
					v, e := read(depth + 1)
					if e != nil {
						return nil, e
					}
					m[key] = v
				}
				end, e := d.Token()
				if e != nil || end != json.Delim('}') {
					return nil, domain.Fail("malformed_request")
				}
				return m, nil
			case '[':
				a := []any{}
				for d.More() {
					v, e := read(depth + 1)
					if e != nil {
						return nil, e
					}
					a = append(a, v)
				}
				end, e := d.Token()
				if e != nil || end != json.Delim(']') {
					return nil, domain.Fail("malformed_request")
				}
				return a, nil
			}
			return nil, domain.Fail("malformed_request")
		case json.Number:
			if _, e := strconv.ParseInt(string(t), 10, 64); e != nil {
				return nil, domain.Fail("malformed_request")
			}
			return t, nil
		default:
			return t, nil
		}
	}
	v, e := read(0)
	if e != nil {
		return nil, e
	}
	if _, e = d.Token(); e != io.EOF {
		return nil, domain.Fail("malformed_request")
	}
	return v, nil
}
func validEscapes(b []byte) bool {
	in := false
	for i := 0; i < len(b); i++ {
		if b[i] == '"' {
			in = !in
			continue
		}
		if !in || b[i] != '\\' {
			continue
		}
		i++
		if i >= len(b) {
			return false
		}
		if b[i] != 'u' {
			continue
		}
		if i+4 >= len(b) {
			return false
		}
		n, e := strconv.ParseUint(string(b[i+1:i+5]), 16, 16)
		if e != nil {
			return false
		}
		i += 4
		if n >= 0xdc00 && n <= 0xdfff {
			return false
		}
		if n < 0xd800 || n > 0xdbff {
			continue
		}
		if i+6 >= len(b) || b[i+1] != '\\' || b[i+2] != 'u' {
			return false
		}
		low, e := strconv.ParseUint(string(b[i+3:i+7]), 16, 16)
		if e != nil || low < 0xdc00 || low > 0xdfff {
			return false
		}
		i += 6
	}
	return !in
}
