# Cohida historical retrieval DNS recurrence — 2026-08-22

## Runtime evidence

The latest relevant window was captured from tmux pane `0:8.0`, beginning at the last command marker:

`for g in 300 900 3600 21600 86400; do podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app ...`

The loop launched one-off `cohida-app` containers using the production compose file. Each container failed during database pool initialization with the sanitized error:

`could not translate host name "db" to address: Name or service not known`

The failure repeated across many symbols and granularities until the pane was interrupted. No credentials or secret-bearing environment values are included here.

## Root-cause boundary

At capture time, the live host had the standalone `db-postgres` service healthy on external `db_prdnet` with alias `postgres`. The production compose file still forced `DB_HOST=db` and attached one-off application containers to `cohida-net`, where no database provider was running. A raw `podman-compose ... run --no-deps cohida-app` therefore created an application container on a network without its configured database hostname.

The production compose contract is now aligned with the shared C++ contract: it joins external `db_prdnet` and uses the standalone database alias `postgres`. The retired `db`/`cohida-net` production contract is no longer used.

## Implementation

`scripts/retrieve-production.sh` is now the production entrypoint. It verifies the external database network, waits for `db-postgres` to report healthy with a bounded timeout, probes `postgres` from a same-network one-off application container, runs the application database test, and only then starts the existing all-symbol retrieval loop. Granularities and symbol selection remain unchanged.

`scripts/validate-production-contract.sh` performs non-build CI validation of the compose host/alias precedence and preflight ordering. The production README now points operators to the guarded entrypoint instead of an unguarded raw loop.

## Verification boundary

The fresh shared-network preflight resolved `postgres` to `10.89.0.2` and confirmed
the database container was healthy, but the application test then failed with
the sanitized runtime error `password authentication failed for user
"postgres"`. The retrieval loop was correctly not started. This is a live
credential synchronization blocker, not a DNS or container-network failure;
secret values were not read, changed, or recorded.

Local CMake, Docker, and Podman application builds are intentionally not run.
Static shell/configuration checks and the exact pushed commit's GitHub Actions
run are repository correctness evidence. A successful live retrieval remains
blocked until the local Cohida `.env` credentials match the existing shared
PostgreSQL service, and is not claimed by CI alone.
