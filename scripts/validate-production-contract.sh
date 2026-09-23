#!/usr/bin/env bash
set -Eeuo pipefail

root="${COHIDA_CONTRACT_ROOT:-.}"
compose_file="$root/podman-compose.prod.yml"
entrypoint="$root/scripts/retrieve-production.sh"
runner="$root/scripts/run-production.sh"
monitor="$root/scripts/monitor-retrieval.sh"
sanitizer="$root/scripts/sanitize-retrieval-output.sh"
workflow="$root/.github/workflows/deploy.yml"

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
[[ -f "$runner" ]] || { printf 'missing %s\n' "$runner" >&2; exit 1; }
[[ -f "$monitor" ]] || { printf 'missing %s\n' "$monitor" >&2; exit 1; }
[[ -x "$sanitizer" ]] || { printf 'missing executable %s\n' "$sanitizer" >&2; exit 1; }
[[ -f "$workflow" ]] || { printf 'missing %s\n' "$workflow" >&2; exit 1; }
bash -n "$entrypoint"
bash -n "$runner"
bash -n "$monitor"
bash -n "$sanitizer"

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
require_fixed "$entrypoint" '[[ "$health_status" == healthy ]]'
require_fixed "$entrypoint" '[[ "$health_status" != healthy ]]'
require_fixed "$entrypoint" 'exit 1'
require_fixed "$entrypoint" 'podman-compose -f "$compose_file" run --rm cohida-app getent hosts "$db_host"'
require_fixed "$entrypoint" 'Database Connection Successful'
require_fixed "$entrypoint" 'application connectivity test did not confirm database success'
require_fixed "$entrypoint" 'COHIDA_PREFLIGHT_ONLY:-0'
require_fixed "$entrypoint" 'for granularity in "${granularities[@]}"; do'
require_fixed "$entrypoint" 'retrieve-all -s {} -g'
require_fixed "$entrypoint" 'COHIDA_RETRIEVAL_STATE_ROOT:-$root/outputs/retrieval-runs'
require_fixed "$entrypoint" 'run_id="$(date -u +%Y%m%dT%H%M%SZ)-${BASHPID}"'
require_fixed "$entrypoint" 'mv -f -- "$active_run_tmp" "$active_lock/run_id"'
require_fixed "$entrypoint" 'refusing to duplicate run'
require_fixed "$entrypoint" 'stdout.log'
require_fixed "$entrypoint" 'stderr.log'
require_fixed "$entrypoint" 'application_exit_status'
require_fixed "$entrypoint" 'capture_timeout="${COHIDA_CAPTURE_TIMEOUT_SECONDS:-86400}"'
require_fixed "$entrypoint" 'application_status == 124'
require_fixed "$entrypoint" 'terminal_outcome=SUCCEEDED'
require_fixed "$entrypoint" 'finish_status=1'
require_fixed "$entrypoint" 'for granularity in 300 900 3600 21600 86400; do'
require_fixed "$entrypoint" 'write_status "$granularity" NOT_TESTED'
require_fixed "$runner" 'if (($# == 0)); then'
require_fixed "$runner" 'COHIDA_PREFLIGHT_ONLY=1 "$script_dir/retrieve-production.sh"'
require_fixed "$runner" 'podman-compose -f "$compose_file" run --rm cohida-app "$@"'
require_fixed "$monitor" 'do not start a duplicate run'
require_fixed "$monitor" 'terminal_outcome=BLOCKED'
require_fixed "$monitor" 'partial logs'
require_fixed "$monitor" 'stale or partial log evidence'
require_fixed "$monitor" 'contradictory outcome'
require_fixed "$monitor" 'application_exit_status"'
require_fixed "$monitor" 'COHIDA_EVIDENCE_MAX_AGE_SECONDS:-300'
require_fixed "$monitor" 'valid_run_id()'
if grep -Eq '(^|[[:space:]])source[[:space:]]' "$monitor"; then
  printf 'monitor must not source caller-controlled evidence\n' >&2
  exit 1
fi
require_fixed "$entrypoint" 'sanitize-retrieval-output.sh'
require_fixed "$workflow" 'github.event.pull_request.head.sha || github.sha'
require_fixed "$workflow" 'exact_checkout_sha=%s\\n'
require_fixed "$workflow" 'type=raw,value=ci-${{ github.event_name == '\''pull_request'\'' && github.event.pull_request.head.sha || github.sha }}'

sanitizer_fixture=$(mktemp)
sanitizer_output=$(mktemp)
trap 'rm -f "$sanitizer_fixture" "$sanitizer_output"' EXIT
key_value='quoted-key-fixture'
secret_value='plain-secret-fixture'
lower_token='lower-case-fixture'
structured_token='structured-fixture'
application_token='application-fixture'
printf '%s\n' \
  "COINBASE_API_KEY=\"$key_value\" COINBASE_API_SECRET=$secret_value" \
  "authorization: Bearer $lower_token" \
  "{\"Authorization\":\"Basic $structured_token\"}" \
  "Authorization: Bearer $application_token" >"$sanitizer_fixture"
"$sanitizer" <"$sanitizer_fixture" >"$sanitizer_output"
if grep -Eq "$key_value|$secret_value|$lower_token|$structured_token|$application_token" "$sanitizer_output"; then
  printf 'sanitizer leaked an adversarial secret fixture\n' >&2
  exit 1
fi
if [[ $(grep -oF '[REDACTED]' "$sanitizer_output" | wc -l) -lt 5 ]]; then
  printf 'sanitizer did not redact every adversarial fixture\n' >&2
  exit 1
fi

network_count=$(grep -Fc 'cohida-net:' "$compose_file")
if ((network_count < 2)); then
  printf 'expected app, db, and top-level cohida-net declarations\n' >&2
  exit 1
fi

depends_on_line=$(grep -nF '    depends_on:' "$compose_file" | head -n1 | cut -d: -f1)
db_service_line=$(grep -nF '  db:' "$compose_file" | head -n1 | cut -d: -f1)
network_line=$(grep -nF 'up -d db' "$entrypoint" | head -n1 | cut -d: -f1)
dns_line=$(grep -nF 'getent hosts "$db_host"' "$entrypoint" | head -n1 | cut -d: -f1)
loop_line=$(grep -nF "printf 'Retrieving all symbols at granularity %s\\n'" "$entrypoint" | cut -d: -f1)
if ! ((depends_on_line < db_service_line)); then
  printf 'compose startup ordering is invalid: depends_on=%s db=%s\n' "$depends_on_line" "$db_service_line" >&2
  exit 1
fi
if ! ((network_line < dns_line && dns_line < loop_line)); then
  printf 'preflight ordering is invalid: network=%s dns=%s loop=%s\n' "$network_line" "$dns_line" "$loop_line" >&2
  exit 1
fi

printf 'production database/network contract validation passed\n'
