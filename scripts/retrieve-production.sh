#!/usr/bin/env bash
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "$script_dir/.." && pwd)"
compose_file="${COHIDA_COMPOSE_FILE:-$root/podman-compose.prod.yml}"
db_container="${COHIDA_DB_CONTAINER:-cohida-db-prod}"
db_network="${COHIDA_DB_NETWORK:-cohida-net}"
db_host="${COHIDA_DB_HOST:-cohida-db}"
health_timeout="${COHIDA_DB_HEALTH_TIMEOUT_SECONDS:-300}"
capture_timeout="${COHIDA_CAPTURE_TIMEOUT_SECONDS:-86400}"
state_root="${COHIDA_RETRIEVAL_STATE_ROOT:-$root/outputs/retrieval-runs}"

if ! [[ "$health_timeout" =~ ^[1-9][0-9]*$ ]] || ((health_timeout > 1800)); then
  printf 'error: COHIDA_DB_HEALTH_TIMEOUT_SECONDS must be an integer from 1 to 1800\n' >&2
  exit 2
fi
if ! [[ "$capture_timeout" =~ ^[1-9][0-9]*$ ]] || ((capture_timeout > 86400)); then
  printf 'error: COHIDA_CAPTURE_TIMEOUT_SECONDS must be an integer from 1 to 86400\n' >&2
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
if ! command -v timeout >/dev/null 2>&1; then
  printf 'error: timeout is required\n' >&2
  exit 127
fi

if (($# > 0)); then
  granularities=("$@")
else
  granularities=(300 900 3600 21600 86400)
fi
for granularity in "${granularities[@]}"; do
  case "$granularity" in
    300|900|3600|21600|86400) ;;
    *) printf 'error: unsupported retrieval granularity: %s\n' "$granularity" >&2; exit 2 ;;
  esac
done

mkdir -p "$state_root"
active_lock="$state_root/.active"
if ! mkdir "$active_lock" 2>/dev/null; then
  active_run="$(<"$active_lock/run_id" 2>/dev/null || true)"
  printf 'error: retrieval already active; refusing to duplicate run (run_id=%s, evidence=%s)\n' \
    "${active_run:-unknown}" "$active_lock" >&2
  exit 1
fi

run_id="$(date -u +%Y%m%dT%H%M%SZ)-${BASHPID}"
run_dir="$state_root/$run_id"
mkdir -p "$run_dir"
{
  printf 'run_id=%q\nstarted_at=%q\nprovenance=%q\ncompose_file=%q\nstate_dir=%q\nexpected_granularities=%q\n' \
    "$run_id" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" 'cohida/scripts/retrieve-production.sh' "$compose_file" "$run_dir" \
    "${granularities[*]}"
} >"$run_dir/metadata.env"

sanitize_stream() {
  sed -E \
    -e 's/COINBASE_API_KEY=[^[:space:]]*/COINBASE_API_KEY=[REDACTED]/g' \
    -e 's/COINBASE_API_SECRET=[^[:space:]]*/COINBASE_API_SECRET=[REDACTED]/g' \
    -e 's/COINBASE_API_PASSPHRASE=[^[:space:]]*/COINBASE_API_PASSPHRASE=[REDACTED]/g' \
    -e 's/(POSTGRES_DB_PASSWORD|DB_PASSWORD)=[^[:space:]]*/\1=[REDACTED]/g' \
    -e 's/Authorization: Bearer [^[:space:]]*/Authorization: Bearer [REDACTED]/g'
}

exec > >(sanitize_stream | tee "$run_dir/stdout.log")
exec 2> >(sanitize_stream | tee "$run_dir/stderr.log" >&2)

write_status() {
  local granularity="$1" outcome="$2" stage="$3" application_exit="$4" wrapper_status="$5" evidence="$6"
  printf 'granularity=%q\noutcome=%q\nstage=%q\napplication_exit_status=%q\nwrapper_status=%q\nevidence_path=%q\n' \
    "$granularity" "$outcome" "$stage" "$application_exit" "$wrapper_status" "$evidence" \
    >"$run_dir/status-$granularity.env"
}

for granularity in 300 900 3600 21600 86400; do
  write_status "$granularity" NOT_TESTED not_started NOT_OBSERVED NOT_STARTED "$run_dir/status-$granularity.env"
done

terminal_outcome=BLOCKED
terminal_stage=initialization
terminal_owner_action='operator: inspect durable run evidence'
terminal_rollback='do not start another retrieval while this run evidence is incomplete'
terminal_evidence="$run_dir"
finish_status=1

finish() {
  local wrapper_status="$finish_status"
  if [[ "$terminal_outcome" == SUCCEEDED ]]; then wrapper_status=0; fi
  printf 'run_id=%q\nterminal_outcome=%q\nfailing_stage=%q\nowner_action=%q\nevidence_path=%q\nrollback_stop_boundary=%q\n' \
    "$run_id" "$terminal_outcome" "$terminal_stage" "$terminal_owner_action" \
    "$terminal_evidence" "$terminal_rollback" >"$run_dir/terminal.env"
  printf 'terminal_outcome=%s failing_stage=%s owner_action=%s evidence_path=%s rollback_stop_boundary=%s\n' \
    "$terminal_outcome" "$terminal_stage" "$terminal_owner_action" "$terminal_evidence" "$terminal_rollback"
  rm -rf "$active_lock"
  exit "$wrapper_status"
}
trap finish EXIT

fail_preflight() {
  terminal_stage="$1"
  terminal_owner_action="$2"
  terminal_evidence="$run_dir"
  terminal_rollback='stop before application retrieval; preserve logs and inspect the cited evidence path'
  for granularity in 300 900 3600 21600 86400; do
    write_status "$granularity" BLOCKED "$terminal_stage" NOT_OBSERVED 1 "$run_dir/status-$granularity.env"
  done
  exit 1
}

printf 'run_id=%s state_dir=%s\n' "$run_id" "$run_dir"
printf 'Starting production database service from %s\n' "$compose_file"
if ! podman-compose -f "$compose_file" up -d db; then fail_preflight scheduler_coordination 'operator: repair compose database startup'; fi
if ! podman network inspect "$db_network" >/dev/null; then fail_preflight scheduler_coordination 'operator: repair production network contract'; fi

printf 'Waiting for %s to become healthy (timeout: %ss)\n' "$db_container" "$health_timeout"
deadline=$((SECONDS + health_timeout))
while ((SECONDS < deadline)); do
  health_status="$(podman inspect "$db_container" --format '{{.State.Health.Status}}' 2>/dev/null || true)"
  if [[ "$health_status" == healthy ]]; then break; fi
  remaining=$((deadline - SECONDS))
  ((remaining > 0)) || break
  sleep_for=$((remaining < 5 ? remaining : 5))
  sleep "$sleep_for"
done
health_status="$(podman inspect "$db_container" --format '{{.State.Health.Status}}' 2>/dev/null || true)"
if [[ "$health_status" != healthy ]]; then
  fail_preflight worker_monitor_lifecycle 'operator: inspect database container health and retain this run evidence'
fi

printf 'Checking db DNS on the production compose network\n'
if ! podman-compose -f "$compose_file" run --rm cohida-app getent hosts "$db_host" >/dev/null; then
  fail_preflight worker_monitor_lifecycle 'operator: repair application-to-database DNS';
fi

printf 'Checking application database connectivity\n'
if ! test_output="$(podman-compose -f "$compose_file" run --rm cohida-app ./bin/cohida test 2>&1)"; then
  printf '%s\n' "$test_output"
  fail_preflight persistence_readback 'operator: inspect application connectivity evidence'
fi
printf '%s\n' "$test_output"
if ! grep -Fq 'Database Connection Successful' <<<"$test_output"; then
  fail_preflight persistence_readback 'operator: repair database connectivity before retrieval'
fi

if [[ "${COHIDA_PREFLIGHT_ONLY:-0}" == 1 ]]; then
  terminal_outcome=NOT_TESTED
  terminal_stage=application_retrieval
  terminal_owner_action='operator: run the canonical retrieval lane after preflight'
  terminal_rollback='no retrieval was started by this preflight-only invocation'
  terminal_evidence="$run_dir"
  finish_status=0
  exit 1
fi

for granularity in "${granularities[@]}"; do
  printf 'Retrieving all symbols at granularity %s\n' "$granularity"
  write_status "$granularity" RUNNING application_retrieval NOT_OBSERVED NOT_COMPLETE "$run_dir/status-$granularity.env"
  set +e
  timeout --foreground --signal=TERM --kill-after=30 "$capture_timeout" \
    podman-compose -f "$compose_file" run --rm cohida-app sh -c \
    './bin/cohida symbols --list | xargs -I {} ./bin/cohida retrieve-all -s {} -g "$1"' \
    sh "$granularity"
  application_status=$?
  set -e
  if ((application_status == 124)); then
    write_status "$granularity" BLOCKED application_retrieval NOT_OBSERVED 124 "$run_dir/status-$granularity.env"
    terminal_stage=worker_monitor_lifecycle
    terminal_owner_action='operator: inspect capture timeout and resume only after application exit evidence exists'
    terminal_rollback='stop at the timed-out granularity; do not retry while the existing run may still be active'
    terminal_evidence="$run_dir/status-$granularity.env"
    exit 1
  elif ((application_status != 0)); then
    write_status "$granularity" BLOCKED application_retrieval "$application_status" "$application_status" "$run_dir/status-$granularity.env"
    terminal_stage=application_retrieval
    terminal_owner_action='operator: inspect application/container exit evidence before any retry'
    terminal_rollback='stop before the next granularity; preserve the failed run logs'
    terminal_evidence="$run_dir/status-$granularity.env"
    exit 1
  fi
  write_status "$granularity" SUCCEEDED application_retrieval 0 0 "$run_dir/status-$granularity.env"
done

terminal_outcome=SUCCEEDED
terminal_stage=none
terminal_owner_action='none: all requested granularities returned application exit 0'
terminal_rollback='no rollback required; retain run-scoped evidence for audit'
terminal_evidence="$run_dir/terminal.env"