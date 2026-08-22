# Cohida historical retrieval DNS recurrence — 2026-08-22

## Runtime evidence

The latest relevant window was captured from tmux pane `0:8.0`, beginning at the last command marker:

`for g in 300 900 3600 21600 86400; do podman-compose -f podman-compose.prod.yml run --rm --no-deps cohida-app ...`

The loop launched one-off `cohida-app` containers using the production compose file. Each container failed during database pool initialization with the sanitized error:

`could not translate host name "db" to address: Name or service not known`

The failure repeated across many symbols and granularities until the pane was interrupted. No credentials or secret-bearing environment values are included here.

## Root-cause boundary

At capture time, the live host had a standalone database on an external network, while the production compose file forced `DB_HOST=db` and attached one-off application containers to `cohida-net`, where no matching database provider was running. A raw `podman-compose ... run --no-deps cohida-app` therefore created an application container on a network without its configured database hostname.

The production compose contract now owns both services on the named `cohida-net` network and uses the database alias `cohida-db`. The retired external-network contract is no longer used.

## Implementation

`scripts/retrieve-production.sh` is now the production entrypoint. It starts the production database, waits for `cohida-db-prod` to report healthy with a bounded timeout, probes `cohida-db` from a same-network one-off application container, runs the application database test, and only then starts the existing all-symbol retrieval loop. Granularities and symbol selection remain unchanged.

`scripts/validate-production-contract.sh` performs non-build CI validation of the compose host/alias precedence and preflight ordering. The production README now points operators to the guarded entrypoint instead of an unguarded raw loop.

## Verification boundary

The prior external-network preflight resolved the standalone database and
confirmed that container was healthy, but the application test then failed with
the sanitized runtime error `password authentication failed for user
"postgres"`. The retrieval loop was correctly not started. That obsolete
external-network failure was removed from the production contract.

The corrected production preflight then started `cohida-db-prod`, observed it
healthy, resolved `cohida-db` on `cohida-net`, and completed the application
database connection test successfully. No secret values were read, changed,
or recorded.

Local CMake, Docker, and Podman application builds are intentionally not run.
Static shell/configuration checks and the exact pushed commit's GitHub Actions
run are repository correctness evidence. A full historical retrieval is not run as part of this bounded preflight; the
retrieval loop remains separately operator-controlled and is not claimed by CI
alone.
