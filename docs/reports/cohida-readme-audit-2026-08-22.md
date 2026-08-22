# Cohida README audit

Audit date: 2026-08-22

## Sources checked

- `README.md`
- `env.example`
- `cpp-cohida/CMakeLists.txt`
- `cpp-cohida/src/config/Config.cpp`
- `cpp-cohida/src/main.cpp`
- `cpp-cohida/vcpkg.json`
- `cpp-cohida/docker-compose.yml`
- `podman-compose.prod.yml`
- `scripts/retrieve-production.sh`
- `scripts/run-production.sh`
- `.github/workflows/deploy.yml`
- `docs/recommended_github_actions.md`

## Confirmed discrepancies and corrections

1. README build commands assumed a root-level CMake project and root-level `bin/` output. The CMake project and output directory are under `cpp-cohida`.
2. README local Compose instructions omitted the named database-network prerequisite and implied the root directory had the application Compose file. The source Compose file is `cpp-cohida/docker-compose.yml`, and it attaches to the named `cohida-net` network.
3. README production guidance recommended `run --no-deps`, which bypasses database dependency startup. The production scripts now use normal Compose runs after their explicit database preflight.
4. README environment examples used `POSTGRES_HOST` and `POSTGRES_DB`, while the C++ loader and production Compose contract use `POSTGRES_DB_HOST`, `POSTGRES_DB_PORT`, `POSTGRES_DB_NAME`, `POSTGRES_DB_USER`, `POSTGRES_DB_PASSWORD`, `POSTGRES_DB_SCHEMA`, and `POSTGRES_DB_TABLE`.
5. README described XGBoost and LightGBM as unconditionally integrated, while CMake treats both as optional and `vcpkg.json` does not list them as required dependencies.
6. `docs/recommended_github_actions.md` described a nonexistent `cpp-build.yml` workflow and a separate compose test job. It was replaced with documentation for the committed `.github/workflows/deploy.yml` workflow.

## Verification boundary

No local CMake, Docker image, or Podman image build was run. Verification is limited to source/documentation consistency, shell syntax, link/path checks, diff checks, and the exact pushed GitHub Actions run.
