package maintenance

import (
	"bytes"
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io"
	"os"
	"strings"
	"sync"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
)

type Ledger struct {
	root *os.Root
	key  []byte
	id   domain.ID
	mu   sync.Mutex
}
type ledgerBody struct {
	LedgerID domain.ID          `json:"ledger_id"`
	Seq      int64              `json:"seq"`
	Previous string             `json:"previous"`
	Event    domain.LedgerEvent `json:"event"`
}
type ledgerLine struct {
	ledgerBody
	MAC string `json:"mac"`
}
type headBody struct {
	LedgerID domain.ID `json:"ledger_id"`
	Seq      int64     `json:"seq"`
	Hash     string    `json:"hash"`
}
type ledgerHead struct {
	headBody
	MAC string `json:"mac"`
}

const zeroHash = "0000000000000000000000000000000000000000000000000000000000000000"

func mac(key []byte, value any) string {
	b, _ := json.Marshal(value)
	h := hmac.New(sha256.New, key)
	_, _ = h.Write(b)
	return hex.EncodeToString(h.Sum(nil))
}
func OpenLedger(path string, key []byte, expected *domain.ID, create bool) (out *Ledger, err error) {
	if len(key) != 32 {
		return nil, domain.Invalid("/ledger_key")
	}
	if create {
		if err = os.MkdirAll(path, 0700); err != nil {
			return nil, err
		}
	}
	root, e := os.OpenRoot(path)
	if e != nil {
		return nil, domain.Wrap("storage_corrupt", e)
	}
	l := &Ledger{root: root, key: append([]byte{}, key...)}
	defer func() {
		if err != nil {
			root.Close()
		}
	}()
	data, e := root.ReadFile("head.json")
	if errors.Is(e, os.ErrNotExist) && create {
		existing, e2 := root.ReadFile("ledger.jsonl")
		if e2 != nil && !errors.Is(e2, os.ErrNotExist) {
			return nil, e2
		}
		if len(existing) != 0 {
			return nil, domain.Fail("storage_corrupt")
		}
		id, e2 := domain.NewID()
		if e2 != nil {
			return nil, e2
		}
		if expected != nil {
			id = *expected
		}
		l.id = id
		f, e2 := root.OpenFile("ledger.jsonl", os.O_CREATE|os.O_WRONLY, 0600)
		if e2 != nil {
			return nil, e2
		}
		e2 = f.Sync()
		f.Close()
		if e2 != nil {
			return nil, e2
		}
		if e2 = l.writeHead(ledgerHead{headBody: headBody{LedgerID: id, Hash: zeroHash}}); e2 != nil {
			return nil, e2
		}
	} else {
		if e != nil {
			return nil, domain.Wrap("storage_corrupt", e)
		}
		var head ledgerHead
		if e = json.Unmarshal(data, &head); e != nil {
			return nil, domain.Wrap("storage_corrupt", e)
		}
		l.id = head.LedgerID
		if !domain.ValidID(string(l.id), false) || expected != nil && l.id != *expected {
			return nil, domain.Fail("storage_corrupt")
		}
	}
	if _, _, e = l.readLocked(); e != nil {
		return nil, e
	}
	return l, nil
}
func (l *Ledger) Close() error  { return l.root.Close() }
func (l *Ledger) ID() domain.ID { return l.id }
func (l *Ledger) writeHead(head ledgerHead) error {
	head.MAC = mac(l.key, head.headBody)
	data, e := json.Marshal(head)
	if e != nil {
		return e
	}
	id, e := domain.NewID()
	if e != nil {
		return e
	}
	name := ".head-" + string(id)
	f, e := l.root.OpenFile(name, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if e != nil {
		return e
	}
	defer func() { f.Close(); _ = l.root.Remove(name) }()
	if _, e = f.Write(data); e != nil {
		return e
	}
	if e = f.Sync(); e != nil {
		return e
	}
	if e = f.Close(); e != nil {
		return e
	}
	return l.root.Rename(name, "head.json")
}
func (l *Ledger) readLocked() ([]domain.LedgerEvent, ledgerHead, error) {
	var head ledgerHead
	raw, e := l.root.ReadFile("head.json")
	if e != nil {
		return nil, head, domain.Wrap("storage_corrupt", e)
	}
	if _, e = protocol.StrictValue(raw); e != nil {
		return nil, head, domain.Fail("storage_corrupt")
	}
	if e = json.Unmarshal(raw, &head); e != nil {
		return nil, head, e
	}
	want := mac(l.key, head.headBody)
	if head.LedgerID != l.id || head.Seq < 0 || !hmac.Equal([]byte(head.MAC), []byte(want)) {
		return nil, head, domain.Fail("storage_corrupt")
	}
	f, e := l.root.Open("ledger.jsonl")
	if e != nil {
		return nil, head, domain.Wrap("storage_corrupt", e)
	}
	data, e := io.ReadAll(io.LimitReader(f, 67108865))
	f.Close()
	if e != nil || len(data) > 67108864 {
		return nil, head, domain.Fail("storage_corrupt")
	}
	complete := len(data)
	if len(data) > 0 && data[len(data)-1] != '\n' {
		complete = bytes.LastIndexByte(data, '\n') + 1
	}
	lines := bytes.Split(data[:complete], []byte{'\n'})
	events := []domain.LedgerEvent{}
	hashes := []string{}
	previous := zeroHash
	seen := map[domain.ID]bool{}
	for _, line := range lines {
		if len(line) == 0 {
			continue
		}
		var record ledgerLine
		if len(line) > 4096 {
			return nil, head, domain.Fail("storage_corrupt")
		}
		if _, e = protocol.StrictValue(line); e != nil {
			return nil, head, domain.Fail("storage_corrupt")
		}
		decoder := json.NewDecoder(bytes.NewReader(line))
		decoder.DisallowUnknownFields()
		if e = decoder.Decode(&record); e != nil {
			return nil, head, domain.Fail("storage_corrupt")
		}
		if record.LedgerID != l.id || record.Seq != int64(len(events)+1) || record.Previous != previous || record.Event.Receipt != "" || !domain.ValidID(string(record.Event.UserID), false) || !domain.ValidID(string(record.Event.OperationID), false) || seen[record.Event.OperationID] || !hmac.Equal([]byte(record.MAC), []byte(mac(l.key, record.ledgerBody))) {
			return nil, head, domain.Fail("storage_corrupt")
		}
		canonical, _ := json.Marshal(record)
		hash := sha256.Sum256(canonical)
		previous = hex.EncodeToString(hash[:])
		record.Event.Receipt = previous
		events = append(events, record.Event)
		hashes = append(hashes, previous)
		seen[record.Event.OperationID] = true
	}
	if head.Seq > int64(len(events)) {
		return nil, head, domain.Fail("storage_corrupt")
	}
	headHash := zeroHash
	if head.Seq > 0 {
		headHash = hashes[head.Seq-1]
	}
	if head.Hash != headHash {
		return nil, head, domain.Fail("storage_corrupt")
	}
	if complete < len(data) {
		f, e := l.root.OpenFile("ledger.jsonl", os.O_WRONLY, 0600)
		if e != nil {
			return nil, head, e
		}
		e = f.Truncate(int64(complete))
		if e == nil {
			e = f.Sync()
		}
		f.Close()
		if e != nil {
			return nil, head, e
		}
	}
	if head.Seq != int64(len(events)) {
		head.Seq = int64(len(events))
		head.Hash = previous
		if e = l.writeHead(head); e != nil {
			return nil, head, e
		}
	}
	return events, head, nil
}
func (l *Ledger) Read(ctx context.Context) ([]domain.LedgerEvent, error) {
	if e := ctx.Err(); e != nil {
		return nil, e
	}
	l.mu.Lock()
	defer l.mu.Unlock()
	events, _, e := l.readLocked()
	return events, e
}
func (l *Ledger) Append(ctx context.Context, event domain.LedgerEvent) (string, error) {
	if e := ctx.Err(); e != nil {
		return "", e
	}
	if !domain.ValidID(string(event.UserID), false) || !domain.ValidID(string(event.OperationID), false) {
		return "", domain.Invalid("/deletion")
	}
	event.Receipt = ""
	l.mu.Lock()
	defer l.mu.Unlock()
	events, head, e := l.readLocked()
	if e != nil {
		return "", e
	}
	for _, old := range events {
		if old.OperationID == event.OperationID {
			if old.UserID != event.UserID || old.RequestedMS != event.RequestedMS {
				return "", domain.Fail("mutation_payload_mismatch")
			}
			return old.Receipt, nil
		}
	}
	record := ledgerLine{ledgerBody: ledgerBody{LedgerID: l.id, Seq: head.Seq + 1, Previous: head.Hash, Event: event}}
	record.MAC = mac(l.key, record.ledgerBody)
	data, e := json.Marshal(record)
	if e != nil {
		return "", e
	}
	f, e := l.root.OpenFile("ledger.jsonl", os.O_WRONLY|os.O_APPEND, 0600)
	if e != nil {
		return "", e
	}
	_, e = f.Write(append(data, '\n'))
	if e == nil {
		e = f.Sync()
	}
	f.Close()
	if e != nil {
		return "", e
	}
	hash := sha256.Sum256(data)
	head.Seq = record.Seq
	head.Hash = hex.EncodeToString(hash[:])
	if e = l.writeHead(head); e != nil {
		return "", e
	}
	return head.Hash, nil
}

type ReplicatedLedger struct{ Primary, Replica ports.Ledger }

func (l *ReplicatedLedger) ID() domain.ID { return l.Primary.ID() }
func (l *ReplicatedLedger) Read(ctx context.Context) ([]domain.LedgerEvent, error) {
	a, e := l.Primary.Read(ctx)
	if e != nil {
		return nil, e
	}
	b, e := l.Replica.Read(ctx)
	if e != nil {
		return nil, e
	}
	if l.Primary.ID() != l.Replica.ID() || len(b) > len(a) {
		return nil, domain.Fail("storage_corrupt")
	}
	for _, event := range a {
		receipt, e := l.Replica.Append(ctx, event)
		if e != nil || receipt != event.Receipt {
			if e != nil {
				return nil, e
			}
			return nil, domain.Fail("storage_corrupt")
		}
	}
	return a, nil
}
func (l *ReplicatedLedger) Append(ctx context.Context, event domain.LedgerEvent) (string, error) {
	first, e := l.Primary.Append(ctx, event)
	if e != nil {
		return "", e
	}
	if _, e = l.Read(ctx); e != nil {
		return "", e
	}
	return first, nil
}
func KeyID(key []byte) string { sum := sha256.Sum256(key); return hex.EncodeToString(sum[:8]) }
func ValidReceipt(s string) bool {
	b, e := hex.DecodeString(s)
	return e == nil && len(b) == 32 && strings.ToLower(s) == s
}
