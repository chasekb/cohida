# Cohida historical retrieval DNS recurrence

## Runtime evidence

The relevant window was captured from tmux pane `0:8.0` beginning at the
command marker `for g in 300 900 3600 21600`. The loop launched
`podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app`.
Each one-off application container failed during database pool startup with
the sanitized error:

`could not translate host name "db" to address: Name or service not known`

The loop then repeated the same failure for many symbols and granularities.
No credentials or secret-bearing environment values are included here.

## Root-cause boundary

The C++ configuration loader gives `POSTGRES_DB_HOST` precedence over
`DB_HOST`, and otherwise accepts `DB_HOST`. Production compose uses the
`cohida-net` network and the database service alias `db`; the shared external
`db_prdnet` contract instead uses `postgres`. A production one-off container
must not inherit a stale hostname from `.env` or be treated as a shared-network
container.

## Implementation

Production compose now explicitly injects `DB_HOST=db` and `DB_PORT=5432` in
the application service after `env_file`, making the production network
selection deterministic. The operator procedure now enables shell fail-fast
behavior, waits for database health, and performs the same-network `db` DNS
probe before the retrieval loop.

## Verification boundary

Local CMake, Docker, and Podman application builds are intentionally not run.
Repository correctness is verified with static/configuration checks and the
exact pushed commit's GitHub Actions run. Runtime DNS/database smoke evidence
must be captured separately from CI build evidence.