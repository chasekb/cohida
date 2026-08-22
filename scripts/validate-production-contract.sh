#!/usr/bin/env bash
set -Eeuo pipefail

root="${COHIDA_CONTRACT_ROOT:-.}"
compose_file="$root/podman-compose.prod.yml"
entrypoint="$root/scripts/retrieve-production.sh"

require_fixed() {
  local file="$1"
  local text="$2"
  if ! grep -Fq -- "$text" "$file"; then
    printf 'missing required contract in %s: %s\n' "$file" "$text" >&2
    exit 1
  fi
}

[[ -f "$compose_file" ]] || { printf 'missing %s\n' "$compose_file" >&2; exit 1; }
[[ -f "$entrypoint" ]] || { printf 'missing %s\n' "$entrypoint" >&2; exit 1; }
bash -n "$entrypoint"

require_fixed "$compose_file" 'DB_HOST: db'
require_fixed "$compose_file" 'DB_PORT: 5432'
require_fixed "$compose_file" 'POSTGRES_DB_HOST: db'
require_fixed "$compose_file" 'POSTGRES_DB_PORT: 5432'
require_fixed "$compose_file" '          - db'
require_fixed "$compose_file" 'cohida-net:'

require_fixed "$entrypoint" 'podman-compose -f "$compose_file" up -d db'
require_fixed "$entrypoint" 'COHIDA_DB_HEALTH_TIMEOUT_SECONDS:-300'
require_fixed "$entrypoint" 'health_timeout > 1800'
require_fixed "$entrypoint" 'deadline=$((SECONDS + health_timeout))'
require_fixed "$entrypoint" 'while ((SECONDS < deadline)); do'
require_fixed "$entrypoint" 'health_status="$(podman inspect "$db_container"'
require_fixed "$entrypoint" '[[ "$health_status" == "healthy" ]]'
require_fixed "$entrypoint" '[[ "$health_status" != "healthy" ]]'
require_fixed "$entrypoint" 'exit 1'
require_fixed "$entrypoint" 'podman-compose -f "$compose_file" run --rm --no-deps cohida-app getent hosts db'
require_fixed "$entrypoint" 'for granularity in "${granularities[@]}"; do'
require_fixed "$entrypoint" 'retrieve-all -s {} -g'

network_count=$(grep -Fc 'cohida-net:' "$compose_file")
if ((network_count < 3)); then
  printf 'expected app, db, and top-level cohida-net declarations\n' >&2
  exit 1
fi

up_line=$(grep -nF 'up -d db' "$entrypoint" | head -n1 | cut -d: -f1)
dns_line=$(grep -nF 'getent hosts db' "$entrypoint" | head -n1 | cut -d: -f1)
loop_line=$(grep -nF 'for granularity in' "$entrypoint" | head -n1 | cut -d: -f1)
if ! ((up_line < dns_line && dns_line < loop_line)); then
  printf 'preflight ordering is invalid: up=%s dns=%s loop=%s\n' "$up_line" "$dns_line" "$loop_line" >&2
  exit 1
fi

printf 'production database/network contract validation passed\n'
