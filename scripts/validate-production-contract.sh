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

require_fixed "$compose_file" 'DB_HOST: cohida-db'
require_fixed "$compose_file" 'DB_PORT: 5432'
require_fixed "$compose_file" 'POSTGRES_DB_HOST: cohida-db'
require_fixed "$compose_file" 'POSTGRES_DB_PORT: 5432'
require_fixed "$compose_file" '    depends_on:'
require_fixed "$compose_file" '      db:'
require_fixed "$compose_file" '        condition: service_healthy'
require_fixed "$compose_file" '      cohida-net:'
require_fixed "$compose_file" '          - cohida-db'
require_fixed "$compose_file" '    name: cohida-net'

require_fixed "$entrypoint" 'db_container="${COHIDA_DB_CONTAINER:-cohida-db-prod}"'
require_fixed "$entrypoint" 'db_network="${COHIDA_DB_NETWORK:-cohida-net}"'
require_fixed "$entrypoint" 'podman-compose -f "$compose_file" up -d db'
require_fixed "$entrypoint" 'podman network inspect "$db_network"'
require_fixed "$entrypoint" 'COHIDA_DB_HEALTH_TIMEOUT_SECONDS:-300'
require_fixed "$entrypoint" 'health_timeout > 1800'
require_fixed "$entrypoint" 'deadline=$((SECONDS + health_timeout))'
require_fixed "$entrypoint" 'while ((SECONDS < deadline)); do'
require_fixed "$entrypoint" 'health_status="$(podman inspect "$db_container"'
require_fixed "$entrypoint" '[[ "$health_status" == "healthy" ]]'
require_fixed "$entrypoint" '[[ "$health_status" != "healthy" ]]'
require_fixed "$entrypoint" 'exit 1'
require_fixed "$entrypoint" 'podman-compose -f "$compose_file" run --rm --no-deps cohida-app getent hosts "$db_host"'
require_fixed "$entrypoint" 'Database Connection Successful'
require_fixed "$entrypoint" 'application connectivity test did not confirm database success'
require_fixed "$entrypoint" 'COHIDA_PREFLIGHT_ONLY:-0'
require_fixed "$entrypoint" 'for granularity in "${granularities[@]}"; do'
require_fixed "$entrypoint" 'retrieve-all -s {} -g'

network_count=$(grep -Fc 'cohida-net:' "$compose_file")
if ((network_count < 2)); then
  printf 'expected app, db, and top-level cohida-net declarations\n' >&2
  exit 1
fi

depends_on_line=$(grep -nF '    depends_on:' "$compose_file" | head -n1 | cut -d: -f1)
db_service_line=$(grep -nF '  db:' "$compose_file" | head -n1 | cut -d: -f1)
network_line=$(grep -nF 'up -d db' "$entrypoint" | head -n1 | cut -d: -f1)
dns_line=$(grep -nF 'getent hosts "$db_host"' "$entrypoint" | head -n1 | cut -d: -f1)
loop_line=$(grep -nF 'for granularity in' "$entrypoint" | head -n1 | cut -d: -f1)
if ! ((depends_on_line < db_service_line)); then
  printf 'compose startup ordering is invalid: depends_on=%s db=%s\n' "$depends_on_line" "$db_service_line" >&2
  exit 1
fi
if ! ((network_line < dns_line && dns_line < loop_line)); then
  printf 'preflight ordering is invalid: network=%s dns=%s loop=%s\n' "$network_line" "$dns_line" "$loop_line" >&2
  exit 1
fi

printf 'production database/network contract validation passed\n'
