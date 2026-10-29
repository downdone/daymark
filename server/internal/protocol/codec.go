package protocol

import (
	"crypto/sha256"
	_ "embed"
	"encoding/hex"
	"encoding/json"
	"sync"

	"daymark.local/server/internal/domain"
	"github.com/santhosh-tekuri/jsonschema/v6"
)

//go:embed sync-v1.openapi.json
var apiJSON []byte

type Codec struct{ schemas map[string]*jsonschema.Schema }

func ErrorStatuses() map[string]int {
	var raw struct {
		Codes map[string][]string `json:"x-error-status-map"`
	}
	_ = json.Unmarshal(apiJSON, &raw)
	out := map[string]int{}
	for status, codes := range raw.Codes {
		var number int
		for _, c := range status {
			number = number*10 + int(c-'0')
		}
		for _, code := range codes {
			out[code] = number
		}
	}
	return out
}

var cachedCodec *Codec
var codecError error
var codecOnce sync.Once

func NewCodec() (*Codec, error) {
	codecOnce.Do(func() { cachedCodec, codecError = compileCodec() })
	return cachedCodec, codecError
}
func compileCodec() (*Codec, error) {
	var root map[string]any
	if e := json.Unmarshal(apiJSON, &root); e != nil {
		return nil, e
	}
	// Equivalent nonzero-year expression for Go's bounded-time RE2 engine.
	// Input source remains byte-identical to the authoritative OpenAPI.
	schemas := root["components"].(map[string]any)["schemas"].(map[string]any)
	for _, name := range []string{"Date", "Instant"} {
		p := schemas[name].(map[string]any)
		pattern := p["pattern"].(string)
		const from = "(?!0000)[0-9]{4}"
		const to = "(000[1-9]|00[1-9][0-9]|0[1-9][0-9]{2}|[1-9][0-9]{3})"
		for i := 0; i+len(from) <= len(pattern); i++ {
			if pattern[i:i+len(from)] == from {
				pattern = pattern[:i] + to + pattern[i+len(from):]
				break
			}
		}
		p["pattern"] = pattern
	}
	c := jsonschema.NewCompiler()
	c.DefaultDraft(jsonschema.Draft2020)
	c.AssertFormat()
	if e := c.AddResource("urn:daymark:sync:v1", root); e != nil {
		return nil, e
	}
	out := &Codec{schemas: map[string]*jsonschema.Schema{}}
	for name := range schemas {
		s, e := c.Compile("urn:daymark:sync:v1#/components/schemas/" + name)
		if e != nil {
			return nil, e
		}
		out.schemas[name] = s
	}
	return out, nil
}
func (c *Codec) Validate(name string, v any) error {
	s, ok := c.schemas[name]
	if !ok {
		return domain.Fail("internal_error")
	}
	if e := s.Validate(v); e != nil {
		return domain.Wrap("validation_failed", e)
	}
	return validateCapsules(v)
}
func (c *Codec) Decode(name string, data []byte, target any) error {
	v, e := StrictValue(data)
	if e != nil {
		return e
	}
	if e = c.Validate(name, v); e != nil {
		return e
	}
	if e = json.Unmarshal(data, target); e != nil {
		return domain.Wrap("malformed_request", e)
	}
	return nil
}

func validateCapsules(value any) error {
	switch v := value.(type) {
	case map[string]any:
		if legacy, ok := v["legacy_metadata"].(map[string]any); ok {
			text, ok := legacy["canonical_json"].(string)
			if !ok {
				return domain.Invalid("/legacy_metadata/canonical_json")
			}
			decoded, e := StrictValue([]byte(text))
			if e != nil {
				return domain.Invalid("/legacy_metadata/canonical_json")
			}
			if _, ok = decoded.(map[string]any); !ok {
				return domain.Invalid("/legacy_metadata/canonical_json")
			}
		}
		for _, item := range v {
			if e := validateCapsules(item); e != nil {
				return e
			}
		}
	case []any:
		for _, item := range v {
			if e := validateCapsules(item); e != nil {
				return e
			}
		}
	}
	return nil
}
func (c *Codec) DecodeMutation(data []byte, dataset domain.ID) (domain.Mutation, error) {
	var m domain.Mutation
	v, e := StrictValue(data)
	if e != nil {
		return m, e
	}
	if e = c.Validate("PushRequest", v); e != nil {
		return m, e
	}
	o := v.(map[string]any)
	var env struct {
		ProtocolVersion int                  `json:"protocol_version"`
		DataEpoch       domain.ID            `json:"data_epoch"`
		MutationID      domain.ID            `json:"mutation_id"`
		InstallationID  domain.ID            `json:"installation_id"`
		ClientSeq       domain.Counter       `json:"client_seq"`
		Preconditions   domain.Preconditions `json:"preconditions"`
		Commands        []json.RawMessage    `json:"commands"`
		Ref             *domain.PayloadRef   `json:"payload_ref"`
	}
	if e = json.Unmarshal(data, &env); e != nil {
		return m, domain.Wrap("malformed_request", e)
	}
	m = domain.Mutation{ProtocolVersion: env.ProtocolVersion, DatasetID: dataset, DataEpoch: env.DataEpoch, MutationID: env.MutationID, InstallationID: env.InstallationID, ClientSeq: env.ClientSeq, Preconditions: env.Preconditions, Ref: env.Ref}
	o["dataset_id"] = string(dataset)
	outer := map[string]any{}
	for _, key := range []string{"protocol_version", "dataset_id", "data_epoch", "mutation_id", "installation_id", "client_seq"} {
		outer[key] = o[key]
	}
	m.OuterHash, e = Digest(outer)
	if e != nil {
		return m, e
	}
	if env.Ref != nil {
		b, e := hex.DecodeString(env.Ref.CanonicalHash)
		if e != nil || len(b) != 32 {
			return m, domain.Invalid("/payload_ref/canonical_sha256")
		}
		copy(m.Hash[:], b)
		return m, nil
	}
	for _, raw := range env.Commands {
		cmd, e := DecodeCommand(raw)
		if e != nil {
			return m, e
		}
		m.Commands = append(m.Commands, cmd)
	}
	if e = canonicalSets(o); e != nil {
		return m, e
	}
	m.Canonical, e = Canonical(o)
	if e != nil {
		return m, e
	}
	m.Hash = sha256.Sum256(m.Canonical)
	return m, nil
}
func DecodeCommand(raw []byte) (domain.Command, error) {
	var event domain.Event
	if e := json.Unmarshal(raw, &event); e != nil {
		return nil, e
	}
	var cmd domain.Command
	switch event.Type {
	case "task.create":
		cmd = &domain.CreateTask{}
	case "task.patch":
		cmd = &domain.PatchTask{}
	case "task.change_state":
		cmd = &domain.ChangeState{}
	case "task.move_tree":
		cmd = &domain.MoveTree{}
	case "category.save":
		cmd = &domain.SaveCategory{}
	case "category.relocate":
		cmd = &domain.RelocateCategory{}
	case "category.remove":
		cmd = &domain.RemoveCategory{}
	case "tag.save":
		cmd = &domain.SaveTag{}
	case "tag.remove":
		cmd = &domain.RemoveTag{}
	case "recurrence.replace":
		cmd = &domain.ReplaceRecurrence{}
	case "recurrence.stop":
		cmd = &domain.StopRecurrence{}
	case "recurrence.materialize":
		cmd = &domain.MaterializeRecurrence{}
	case "ordering.replace":
		cmd = &domain.ReplaceOrdering{}
	default:
		return nil, domain.Fail("protocol_upgrade_required")
	}
	if e := json.Unmarshal(raw, cmd); e != nil {
		return nil, domain.Wrap("malformed_request", e)
	}
	return cmd, nil
}
