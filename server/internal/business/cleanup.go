package business

import (
	"context"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
)

type AccountEraser interface {
	DropAccount(context.Context, domain.ID) error
}

// CleanupOnePage acknowledges only after the independent store confirms erasure.
// A crash between those writes safely repeats the same deletion.
func CleanupOnePage(ctx context.Context, store ports.BusinessStore, metrics AccountEraser) (int, error) {
	var queue []domain.Cleanup
	e := store.ViewBusiness(ctx, func(tx ports.BusinessTx) error { var e error; queue, e = tx.CleanupQueue(ctx, 32); return e })
	if e != nil {
		return 0, e
	}
	for _, entry := range queue {
		if metrics != nil {
			if e = metrics.DropAccount(ctx, entry.UserID); e != nil {
				return 0, e
			}
		}
		if _, e = store.UpdateBusiness(ctx, func(tx ports.BusinessTx) error { return tx.AckCleanup(ctx, entry) }); e != nil {
			return 0, e
		}
	}
	return len(queue), nil
}
