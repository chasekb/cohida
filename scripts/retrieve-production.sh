#!/usr/bin/env bash
set -Eeuo pipefail

compose_file="${COHIDA_COMPOSE_FILE:-podman-compose.prod.yml}"
db_container="${COHIDA_DB_CONTAINER:-cohida-db-prod}"
db_network="${COHIDA_DB_NETWORK:-cohida-net}"
db_host="${COHIDA_DB_HOST:-cohida-db}"
health_timeout="${COHIDA_DB_HEALTH_TIMEOUT_SECONDS:-300}"

if ! [[ "$health_timeout" =~ ^[1-9][0-9]*$ ]] || ((health_timeout > 1800)); then
  printf 'error: COHIDA_DB_HEALTH_TIMEOUT_SECONDS must be an integer from 1 to 1800\n' >&2
  exit 2
fi

if ! command -v podman-compose >/dev/null 2>&1; then
  printf 'error: podman-compose is required\n' >&2
  exit 127
fi
if ! command -v podman >/dev/null 2>&1; then
  printf 'error: podman is required\n' >&2
  exit 127
fi

if (($# > 0)); then
  granularities=("$@")
else
  granularities=(300 900 3600 21600 86400)
fi

printf 'Starting production database service from %s\n' "$compose_file"
podman-compose -f "$compose_file" up -d db
podman network inspect "$db_network" >/dev/null

printf 'Waiting for %s to become healthy (timeout: %ss)\n' "$db_container" "$health_timeout"
deadline=$((SECONDS + health_timeout))
while ((SECONDS < deadline)); do
  health_status="$(podman inspect "$db_container" --format '{{.State.Health.Status}}' 2>/dev/null || true)"
  if [[ "$health_status" == "healthy" ]]; then
    break
  fi
  remaining=$((deadline - SECONDS))
  ((remaining > 0)) || break
  sleep_for=$((remaining < 5 ? remaining : 5))
  sleep "$sleep_for"
done

health_status="$(podman inspect "$db_container" --format '{{.State.Health.Status}}' 2>/dev/null || true)"
if [[ "$health_status" != "healthy" ]]; then
  printf 'error: %s did not become healthy (status: %s)\n' "$db_container" "${health_status:-missing}" >&2
  exit 1
fi

printf 'Checking db DNS on the production compose network\n'
podman-compose -f "$compose_file" run --rm --no-deps cohida-app getent hosts "$db_host" >/dev/null

printf 'Checking application database connectivity\n'
if ! test_output="$(podman-compose -f "$compose_file" run --rm --no-deps cohida-app ./bin/cohida test 2>&1)"; then
  printf '%s\n' "$test_output"
  printf 'error: application connectivity test command failed\n' >&2
  exit 1
fi
printf '%s\n' "$test_output"
if ! grep -Fq 'Database Connection Successful' <<<"$test_output"; then
  printf 'error: application connectivity test did not confirm database success\n' >&2
  exit 1
fi

if [[ "${COHIDA_PREFLIGHT_ONLY:-0}" == "1" ]]; then
  printf 'Production database preflight passed; retrieval loop not requested\n'
  exit 0
fi

for granularity in "${granularities[@]}"; do
  printf 'Retrieving all symbols at granularity %s\n' "$granularity"
  podman-compose -f "$compose_file" run --rm --no-deps cohida-app sh -c \
    './bin/cohida symbols --list | xargs -I {} ./bin/cohida retrieve-all -s {} -g "$1"' \
    sh "$granularity"
done
