package ports

import (
	"context"
	"daymark.local/server/internal/domain"
)

// Separate from Tx: existing task protocol adapters and test stores do not gain
// admin operations. Both interfaces share the same real SQL transaction writer.
type BusinessTx interface {
	State(context.Context) (domain.State, error)
	User(context.Context, domain.ID) (*domain.User, error)
	PutUser(context.Context, domain.User) error
	RevokeSessions(context.Context, domain.ID, int64) error
	Admin(context.Context, domain.ID) (*domain.Admin, error)
	AdminByLogin(context.Context, string) (*domain.Admin, error)
	Admins(context.Context) ([]domain.Admin, error)
	PutAdmin(context.Context, domain.Admin) error
	AdminSession(context.Context, [32]byte) (*domain.AdminSession, error)
	PutAdminSession(context.Context, domain.AdminSession) error
	RemoveAdminSession(context.Context, [32]byte) error
	Users(context.Context, domain.UserQuery, int64) ([]domain.UserSummary, error)
	Overview(context.Context, int64) (domain.BusinessOverview, error)
	Notice(context.Context, domain.ID) (*domain.Notice, error)
	Notices(context.Context, domain.ID, int) ([]domain.Notice, error)
	PutNotice(context.Context, domain.Notice) error
	AppendAudit(context.Context, domain.Audit) error
	Audits(context.Context, int64, int) ([]domain.Audit, error)
	BusinessOperation(context.Context, domain.ID, domain.ID) (*domain.BusinessOperation, error)
	PutBusinessOperation(context.Context, domain.BusinessOperation) error
	CleanupQueue(context.Context, int) ([]domain.Cleanup, error)
	AckCleanup(context.Context, domain.Cleanup) error
	MaintainBusiness(context.Context, int64) error
}
type BusinessStore interface {
	ViewBusiness(context.Context, func(BusinessTx) error) error
	UpdateBusiness(context.Context, func(BusinessTx) error) (CommitState, error)
}
