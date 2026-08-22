#!/usr/bin/env bash
set -Eeuo pipefail

if (($# == 0)); then
  printf 'usage: %s <application command> [args...]\n' "$0" >&2
  exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "$script_dir/.." && pwd)"
compose_file="${COHIDA_COMPOSE_FILE:-$root/podman-compose.prod.yml}"

# The preflight starts/reuses PostgreSQL, waits for its healthcheck, verifies
# cohida-db DNS, and confirms application connectivity before this one-off run.
COHIDA_PREFLIGHT_ONLY=1 "$script_dir/retrieve-production.sh"
podman-compose -f "$compose_file" run --rm cohida-app "$@"
