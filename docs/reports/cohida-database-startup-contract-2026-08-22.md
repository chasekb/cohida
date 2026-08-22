# Cohida database startup contract investigation

## Runtime evidence

The requested target was tmux pane `0:8.0`. The pane list confirmed that target is the cohida project shell. A bounded capture of the retained pane scrollback was taken at `/tmp/cohida-tmux-0-8-0-since-retrieval.txt`.

The exact marker `for g in 300 900 3600 21600` was no longer present in the retained pane scrollback, so the capture could not delimit the requested historical loop from that marker. The current failure window nevertheless contains repeated application startup failures at approximately `2026-08-22 20:03:41` through `20:03:45`:

```text
Failed to initialize database connection pool: could not translate host name "cohida-db" to address: Name or service not known
Error in complete historical data retrieval: Database error: Failed to initialize database connection pool
Failed to retrieve all data: Database error: Failed to initialize database connection pool
```

The failure repeats for multiple symbols. This is a database network/startup contract failure, not evidence of a Coinbase retrieval or data-cleanup problem.

## Traced contract

- `podman-compose.prod.yml` owns the production `db` service, the `cohida-net` network, the `cohida-db` alias, and the PostgreSQL healthcheck.
- `scripts/retrieve-production.sh` explicitly starts `db`, waits for `cohida-db-prod` to become healthy, verifies `cohida-db` DNS from a same-network application container, tests application connectivity, and only then enters the retrieval loop.
- The production application previously had no Compose-level dependency declaration. A direct `podman-compose run` path could therefore launch application work without an explicit healthy-database dependency; passing `--no-deps` bypasses dependency startup entirely.

## Implemented contract hardening

- Added `cohida-app.depends_on.db.condition: service_healthy` to `podman-compose.prod.yml`.
- Extended `scripts/validate-production-contract.sh` to require the dependency and validate its ordering relative to the database service.
- Documented that raw application `podman-compose run` diagnostics must not use `--no-deps`; the guarded production entrypoint remains the required retrieval path.

No local CMake, Docker image, or Podman image build was run. Static shell, YAML, contract, and diff checks were used before remote CI.
