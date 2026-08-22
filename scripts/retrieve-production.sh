#!/usr/bin/env bash
set -Eeuo pipefail

compose_file="${COHIDA_COMPOSE_FILE:-podman-compose.prod.yml}"
db_container="${COHIDA_DB_CONTAINER:-cohida-db-prod}"
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
podman-compose -f "$compose_file" run --rm --no-deps cohida-app getent hosts db >/dev/null

printf 'Checking application database connectivity\n'
podman-compose -f "$compose_file" run --rm --no-deps cohida-app ./bin/cohida test

for granularity in "${granularities[@]}"; do
  printf 'Retrieving all symbols at granularity %s\n' "$granularity"
  podman-compose -f "$compose_file" run --rm --no-deps cohida-app sh -c \
    './bin/cohida symbols --list | xargs -I {} ./bin/cohida retrieve-all -s {} -g "$1"' \
    sh "$granularity"
done
