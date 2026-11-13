package domain

// Business types deliberately do not join the task synchronization entity union.
type Admin struct {
	ID             ID       `json:"id"`
	Login          string   `json:"login"`
	PasswordHash   string   `json:"-"`
	SecretCipher   []byte   `json:"-"`
	RecoveryHashes []string `json:"-"`
	Roles          []string `json:"roles"`
	Status         string   `json:"status"`
	Version        int64    `json:"version"`
	LastStep       int64    `json:"-"`
	CreatedMS      int64    `json:"created_ms"`
}
type AdminSession struct {
	Hash                          [32]byte
	AdminID                       ID
	Version, CreatedMS, ExpiresMS int64
	AuthEpoch                     ID
}
type UserSummary struct {
	ID        ID     `json:"id"`
	Login     string `json:"login"`
	Status    string `json:"status"`
	Version   int64  `json:"version"`
	CreatedMS int64  `json:"created_ms"`
	Sessions  int    `json:"sessions"`
}
type UserQuery struct {
	After          ID
	Status, Search string
	Limit          int
}
type Notice struct {
	ID         ID     `json:"id"`
	Title      string `json:"title"`
	Body       string `json:"body"`
	State      string `json:"state"`
	Action     string `json:"action"`
	Platform   string `json:"platform"`
	MinVersion string `json:"min_version"`
	MaxVersion string `json:"max_version"`
	Targets    []ID   `json:"targets"`
	Version    int64  `json:"version"`
	PublishMS  int64  `json:"publish_ms"`
	ExpireMS   int64  `json:"expire_ms"`
	UpdatedMS  int64  `json:"updated_ms"`
	CreatedBy  ID     `json:"created_by"`
}
type Audit struct {
	Seq       int64  `json:"seq"`
	Actor     ID     `json:"actor"`
	Action    string `json:"action"`
	Target    ID     `json:"target"`
	Reason    string `json:"reason"`
	RequestID string `json:"request_id"`
	AtMS      int64  `json:"at_ms"`
}
type BusinessOperation struct {
	Actor, ID    ID
	Hash, Result string
	AtMS         int64
}
type CommandReceipt struct {
	ID      ID    `json:"id"`
	Version int64 `json:"version"`
}
type BusinessOverview struct {
	Users       int64 `json:"users"`
	ActiveUsers int64 `json:"active_users"`
	Sessions    int64 `json:"sessions"`
	Notices     int64 `json:"notices"`
}
type Cleanup struct {
	UserID ID
	AtMS   int64
}
