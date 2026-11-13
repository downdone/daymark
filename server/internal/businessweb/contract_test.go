package businessweb_test

import (
	"encoding/json"
	"os"
	"testing"

	"github.com/santhosh-tekuri/jsonschema/v6"
)

// Validate real HTTP output against the published independent business contract.
// The task protocol schema remains unchanged and is tested by its own codec.
func TestPublishedBusinessContractMatchesHTTP(t *testing.T) {
	data, e := os.ReadFile("../../../docs/api/business-v1.openapi.json")
	if e != nil {
		t.Fatal(e)
	}
	var root map[string]any
	if e = json.Unmarshal(data, &root); e != nil {
		t.Fatal(e)
	}
	compiler := jsonschema.NewCompiler()
	compiler.DefaultDraft(jsonschema.Draft2020)
	compiler.AssertFormat()
	if e = compiler.AddResource("urn:daymark:business:v1", root); e != nil {
		t.Fatal(e)
	}
	schemas := map[string]*jsonschema.Schema{}
	for name := range root["components"].(map[string]any)["schemas"].(map[string]any) {
		schema, e := compiler.Compile("urn:daymark:business:v1#/components/schemas/" + name)
		if e != nil {
			t.Fatal(name, e)
		}
		schemas[name] = schema
	}
	f := fixtureFor(t, true)
	csrf := f.login(t, f.owner, "synthetic-owner")
	if csrf == "" {
		t.Fatal("missing csrf")
	}
	for _, tc := range []struct{ path, schema string }{{"/admin/api/v1/users", "Users"}, {"/admin/api/v1/admins", "Admins"}, {"/admin/api/v1/notices", "Notices"}, {"/admin/api/v1/audit", "Audits"}, {"/admin/api/v1/metrics", "MetricsResponse"}} {
		response, body := f.request(t, "GET", tc.path, nil, "", "", "", "")
		if response.StatusCode != 200 {
			t.Fatal(tc.path, response.StatusCode, string(body))
		}
		var value any
		if e = json.Unmarshal(body, &value); e != nil {
			t.Fatal(e)
		}
		if e = schemas[tc.schema].Validate(value); e != nil {
			t.Fatal(tc.path, e)
		}
	}
}
