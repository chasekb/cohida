#!/usr/bin/env bash
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "$script_dir/.." && pwd)"
state_root="${COHIDA_RETRIEVAL_STATE_ROOT:-$root/outputs/retrieval-runs}"
requested_run="${1:-}"

if [[ -n "$requested_run" ]]; then
  run_id="$requested_run"
elif [[ -f "$state_root/.active/run_id" ]]; then
  run_id="$(<"$state_root/.active/run_id")"
else
  printf 'terminal_outcome=NOT_TESTED failing_stage=worker_monitor_lifecycle owner_action=operator\: select an existing run evidence directory evidence_path=%s rollback_stop_boundary=do not start a new run without durable evidence\n' "$state_root"
  exit 1
fi

run_dir="$state_root/$run_id"
terminal_file="$run_dir/terminal.env"
metadata_file="$run_dir/metadata.env"
fail() {
  printf 'terminal_outcome=BLOCKED failing_stage=%s owner_action=%s evidence_path=%s rollback_stop_boundary=%s\n' "$1" "$2" "$3" "$4"
  exit 1
}

[[ -d "$run_dir" ]] || fail worker_monitor_lifecycle 'operator: inspect the requested run identity' "$run_dir" 'do not duplicate or kill an unknown run'
[[ -f "$metadata_file" ]] || fail worker_monitor_lifecycle 'operator: restore missing run provenance' "$metadata_file" 'do not treat an unproven run as successful'
[[ -s "$run_dir/stdout.log" && -s "$run_dir/stderr.log" ]] || fail worker_monitor_lifecycle 'operator: retain both sanitized log streams' "$run_dir" 'do not treat partial logs as successful'
[[ -f "$terminal_file" ]] || fail worker_monitor_lifecycle 'operator: wait for terminal evidence or investigate the interrupted run' "$run_dir" 'do not start a duplicate run while terminal evidence is absent'
terminal_mtime="$(stat -c %Y "$terminal_file" 2>/dev/null || true)"
for log_file in "$run_dir/stdout.log" "$run_dir/stderr.log"; do
  log_mtime="$(stat -c %Y "$log_file" 2>/dev/null || true)"
  [[ "$terminal_mtime" =~ ^[0-9]+$ && "$log_mtime" =~ ^[0-9]+$ && "$log_mtime" -le "$terminal_mtime" ]] || \
    fail worker_monitor_lifecycle 'operator: resolve stale or partial log evidence' "$log_file" 'stop on stale logs'
done

# shellcheck disable=SC1090
source "$metadata_file"
# shellcheck disable=SC1090
source "$terminal_file"
[[ "${run_id:-}" == "$requested_run" || -z "$requested_run" ]] || fail worker_monitor_lifecycle 'operator: resolve contradictory run identity' "$metadata_file" 'stop on provenance contradiction'

for granularity in 300 900 3600 21600 86400; do
  status_file="$run_dir/status-$granularity.env"
  [[ -f "$status_file" ]] || fail failure_signaling "operator: restore missing terminal outcome for $granularity" "$status_file" 'stop before accepting the run'
  # shellcheck disable=SC1090
  source "$status_file"
  case "${outcome:-}" in
    SUCCEEDED|BLOCKED|NOT_TESTED) ;;
    *) fail failure_signaling "operator: resolve contradictory outcome for $granularity" "$status_file" 'stop on ambiguous evidence' ;;
  esac
done

if [[ "${terminal_outcome:-}" == SUCCEEDED ]]; then
  for granularity in ${expected_granularities:-300 900 3600 21600 86400}; do
    status_file="$run_dir/status-$granularity.env"
    # shellcheck disable=SC1090
    source "$status_file"
    [[ "$outcome" == SUCCEEDED && "$application_exit_status" == 0 && "$wrapper_status" == 0 ]] || \
      fail failure_signaling "operator: resolve contradictory success for $granularity" "$status_file" 'stop and retain the run evidence'
  done
elif [[ "$terminal_outcome" != BLOCKED && "$terminal_outcome" != NOT_TESTED ]]; then
  fail failure_signaling 'operator: resolve unknown terminal outcome' "$terminal_file" 'stop on ambiguous evidence'
fi

printf 'run_id=%s terminal_outcome=%s failing_stage=%s owner_action=%s evidence_path=%s rollback_stop_boundary=%s\n' \
  "$run_id" "$terminal_outcome" "${failing_stage:-none}" "${owner_action:-operator: inspect}" \
  "${evidence_path:-$run_dir}" "${rollback_stop_boundary:-stop on missing evidence}"
if [[ "$terminal_outcome" == SUCCEEDED ]]; then exit 0; fi
exit 1