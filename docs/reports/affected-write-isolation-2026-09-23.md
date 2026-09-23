# Affected-symbol write isolation

## Repair boundary

`DatabaseManager::write_data_detailed` now executes each data point in its own PostgreSQL transaction. A statement error rolls back only that point, allowing later symbols to be attempted without reusing an aborted transaction. The result reports the number written and a symbol/error entry for every failed point. The existing `write_data` API remains available and returns the successful count for compatibility.

CLI retrieval commands use the detailed result and log an incomplete/error outcome instead of claiming that the batch was written when any point failed. Values are passed through unchanged; the repair does not clamp, zero-fill, coerce, widen the schema, or mark a failed point as persisted.

## Rollback

Revert the implementation, regression-test, and this report changes in one change set. No schema or production data change is part of this repair, so rollback requires no database migration or data cleanup. Do not rerun production retrieval as part of rollback verification; use the existing test database/CI workflow only.

## Schema migration recommendation

No schema migration is recommended by this change. The repository only declares `NUMERIC(18,8)` prices and `NUMERIC(20,8)` volume, and this task has no production SQLSTATE, offending value, observed precision/scale sample, or read-only database evidence. Consider a separate migration proposal only after collecting bounded read-only evidence for the affected field, required integer digits and fractional scale, compatibility with existing readers, and a reversible rollout plan.
