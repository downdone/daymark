package domain

import (
	"errors"
	"fmt"
)

// Fault carries only stable, safe public details. Cause is never serialized.
type Fault struct {
	Code       string             `json:"code"`
	Field      string             `json:"field_path,omitempty"`
	Conflicts  []Conflict         `json:"conflicts,omitempty"`
	Structures []StructureVersion `json:"structure_conflicts,omitempty"`
	Cause      error              `json:"-"`
}

func (f *Fault) Error() string   { return f.Code }
func (f *Fault) Unwrap() error   { return f.Cause }
func Fail(code string) error     { return &Fault{Code: code} }
func Invalid(field string) error { return &Fault{Code: "validation_failed", Field: field} }
func Wrap(code string, err error) error {
	if err == nil {
		return nil
	}
	return &Fault{Code: code, Cause: err}
}
func Code(err error) string {
	var f *Fault
	if errors.As(err, &f) {
		return f.Code
	}
	return "internal_error"
}

type Conflict struct {
	Type     Kind    `json:"type"`
	ID       ID      `json:"id"`
	Expected Counter `json:"expected_revision"`
	Actual   Counter `json:"actual_revision"`
}

func ConflictError(k Key, expected, actual Counter) error {
	return &Fault{Code: "revision_conflict", Conflicts: []Conflict{{k.Type, k.ID, expected, actual}}}
}
func Missing(k Key) error {
	return &Fault{Code: "read_set_incomplete", Field: fmt.Sprintf("/preconditions/entities/%s/%s", k.Type, k.ID)}
}
