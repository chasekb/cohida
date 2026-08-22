# Cohida historical retrieval DNS recurrence — 2026-08-22

## Runtime evidence

The relevant window was captured from tmux pane `0:8.0`, beginning at the last command marker:

`for g in 300 900 3600 21600 86400; do podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app ...`

The loop launched one-off `cohida-app` containers without first starting the production `db` service. Each container failed during database pool initialization with the sanitized error:

`could not translate host name "db" to address: Name or service not known`

The failure repeated across many symbols and granularities until the pane was interrupted. No credentials or secret-bearing environment values are included here.

## Root-cause boundary

The checked-in production compose contract puts the application and database on `cohida-net`, with database alias `db`. At capture time, the live host had the standalone `db-postgres` service healthy on the separate external `db_prdnet` network, while no `cohida-db-prod` production database container was running. A raw `podman-compose ... run --no-deps cohida-app` therefore created an application container on the production network with no database provider attached.

The shared C++ compose contract remains separate: it joins `db_prdnet` and uses the standalone database alias `postgres`. The two hostname contracts must not be mixed.

## Implementation

`scripts/retrieve-production.sh` is now the production entrypoint. It starts the declared production database, waits for `cohida-db-prod` to report healthy with a bounded timeout, probes `db` from a same-network one-off application container, runs the application database test, and only then starts the existing all-symbol retrieval loop. Granularities and symbol selection remain unchanged.

`scripts/validate-production-contract.sh` performs non-build CI validation of the compose host/alias precedence and preflight ordering. The production README now points operators to the guarded entrypoint instead of an unguarded raw loop.

## Verification boundary

Local CMake, Docker, and Podman application builds are intentionally not run. Static shell/configuration checks and the exact pushed commit's GitHub Actions run are the repository correctness evidence. A fresh live runtime retrieval remains a separate operational verification gate and is not claimed by CI alone.
