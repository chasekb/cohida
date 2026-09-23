#!/usr/bin/env bash
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "$script_dir/.." && pwd)"
state_root="${COHIDA_RETRIEVAL_STATE_ROOT:-$root/outputs/retrieval-runs}"
freshness_cutoff="${COHIDA_EVIDENCE_MAX_AGE_SECONDS:-300}"
requested_run="${1:-}"

if ! [[ "$freshness_cutoff" =~ ^[1-9][0-9]*$ ]] || ((freshness_cutoff > 86400)); then
  printf 'terminal_outcome=BLOCKED failing_stage=worker_monitor_lifecycle owner_action=operator: configure a freshness cutoff from 1 to 86400 seconds evidence_path=%s rollback_stop_boundary=stop on invalid monitor policy\n' "$state_root"
  exit 1
fi

valid_run_id() { [[ "$1" =~ ^[0-9]{8}T[0-9]{6}Z-[0-9]+$ ]]; }
fail() {
  printf 'terminal_outcome=BLOCKED failing_stage=%s owner_action=%s evidence_path=%s rollback_stop_boundary=%s\n' "$1" "$2" "$3" "$4"
  exit 1
}

if [[ -n "$requested_run" ]]; then
  valid_run_id "$requested_run" || fail worker_monitor_lifecycle 'operator: inspect the requested run identity' "$requested_run" 'reject unsafe run paths'
  run_id="$requested_run"
elif [[ -f "$state_root/.active/run_id" ]]; then
  active_contents="$(<"$state_root/.active/run_id")"
  valid_run_id "$active_contents" || fail worker_monitor_lifecycle 'operator: repair invalid active run identity' "$state_root/.active/run_id" 'reject unsafe run paths'
  run_id="$active_contents"
else
  fail worker_monitor_lifecycle 'operator: select an existing run evidence directory' "$state_root" 'do not start a new run without durable evidence'
fi

run_dir="$state_root/$run_id"
metadata_file="$run_dir/metadata.env"
terminal_file="$run_dir/terminal.env"
valid_granularity() { case "$1" in 300|900|3600|21600|86400) return 0 ;; *) return 1 ;; esac; }

field() {
  local file="$1" wanted="$2" line key value found=0
  while IFS= read -r line || [[ -n "$line" ]]; do
    [[ "$line" =~ ^([a-z_]+)=([^[:cntrl:]]*)$ ]] || return 1
    key="${BASH_REMATCH[1]}"
    value="${BASH_REMATCH[2]}"
    case "$file:$key" in
      "$metadata_file:run_id"|"$metadata_file:started_at"|"$metadata_file:provenance"|"$metadata_file:compose_file"|"$metadata_file:state_dir"|"$metadata_file:expected_granularities") ;;
      "$terminal_file:run_id"|"$terminal_file:terminal_outcome"|"$terminal_file:failing_stage"|"$terminal_file:owner_action"|"$terminal_file:evidence_path"|"$terminal_file:rollback_stop_boundary") ;;
      *:granularity|*:outcome|*:stage|*:application_exit_status|*:wrapper_status|*:evidence_path) ;;
      *) return 1 ;;
    esac
    if [[ "$key" == "$wanted" ]]; then
      ((found == 0)) || return 1
      printf '%s' "$value"
      found=1
    fi
  done <"$file"
  ((found == 1))
}

fresh_file() {
  local file="$1" mtime now
  [[ -f "$file" ]] || return 1
  mtime="$(stat -c %Y -- "$file" 2>/dev/null || true)"
  now="$(date +%s)"
  [[ "$mtime" =~ ^[0-9]+$ ]] || return 1
  ((mtime <= now && now - mtime <= freshness_cutoff))
}

[[ -d "$run_dir" ]] || fail worker_monitor_lifecycle 'operator: inspect the requested run identity' "$run_dir" 'do not start a duplicate run or kill an unknown run'
[[ -f "$metadata_file" ]] || fail worker_monitor_lifecycle 'operator: restore missing run provenance' "$metadata_file" 'do not treat an unproven run as successful'
[[ -f "$run_dir/stdout.log" && -f "$run_dir/stderr.log" ]] || fail worker_monitor_lifecycle 'operator: retain both log streams' "$run_dir" 'do not treat partial logs as successful'
[[ -f "$terminal_file" ]] || fail worker_monitor_lifecycle 'operator: wait for terminal evidence' "$terminal_file" 'do not accept an incomplete run'
for evidence_file in "$run_dir/stdout.log" "$run_dir/stderr.log" "$terminal_file"; do
  fresh_file "$evidence_file" || fail worker_monitor_lifecycle 'operator: resolve stale or future-dated evidence' "$evidence_file" 'stop on stale evidence'
done
terminal_mtime="$(stat -c %Y -- "$terminal_file")"
for log_file in "$run_dir/stdout.log" "$run_dir/stderr.log"; do
  log_mtime="$(stat -c %Y -- "$log_file")"
  ((log_mtime <= terminal_mtime)) || fail worker_monitor_lifecycle 'operator: resolve stale or partial log evidence written after terminal closure' "$log_file" 'stop on partial evidence'
done

metadata_run_id="$(field "$metadata_file" run_id)" || fail worker_monitor_lifecycle 'operator: parse restricted run metadata' "$metadata_file" 'reject untrusted evidence'
[[ "$metadata_run_id" == "$run_id" ]] || fail worker_monitor_lifecycle 'operator: resolve contradictory run identity' "$metadata_file" 'stop on provenance contradiction'
expected="$(field "$metadata_file" expected_granularities)" || fail worker_monitor_lifecycle 'operator: parse requested granularities' "$metadata_file" 'reject untrusted evidence'
read -r -a expected_granularities <<<"$expected"
((${#expected_granularities[@]} > 0)) || fail failure_signaling 'operator: restore requested granularities' "$metadata_file" 'stop on missing evidence'
for granularity in "${expected_granularities[@]}"; do
  valid_granularity "$granularity" || fail failure_signaling 'operator: resolve invalid requested granularity' "$metadata_file" 'reject untrusted evidence'
done
terminal_run_id="$(field "$terminal_file" run_id)" || fail failure_signaling 'operator: parse terminal provenance' "$terminal_file" 'reject untrusted evidence'
terminal_outcome="$(field "$terminal_file" terminal_outcome)" || fail failure_signaling 'operator: parse terminal outcome' "$terminal_file" 'reject untrusted evidence'
[[ "$terminal_run_id" == "$run_id" ]] || fail failure_signaling 'operator: resolve terminal run identity' "$terminal_file" 'stop on provenance contradiction'

for granularity in "${expected_granularities[@]}"; do
  status_file="$run_dir/status-$granularity.env"
  [[ -f "$status_file" ]] || fail failure_signaling "operator: restore terminal outcome for $granularity" "$status_file" 'stop before accepting the run'
  status_mtime="$(stat -c %Y -- "$status_file" 2>/dev/null || true)"
  [[ "$status_mtime" =~ ^[0-9]+$ ]] && ((status_mtime <= terminal_mtime)) || fail failure_signaling "operator: resolve status evidence written after terminal closure for $granularity" "$status_file" 'stop on partial evidence'
  status_granularity="$(field "$status_file" granularity)" || fail failure_signaling 'operator: parse restricted status evidence' "$status_file" 'reject untrusted evidence'
  status_outcome="$(field "$status_file" outcome)" || fail failure_signaling 'operator: parse restricted status outcome' "$status_file" 'reject untrusted evidence'
  application_exit="$(field "$status_file" application_exit_status)" || fail failure_signaling 'operator: parse application exit evidence' "$status_file" 'reject untrusted evidence'
  wrapper_status="$(field "$status_file" wrapper_status)" || fail failure_signaling 'operator: parse wrapper exit evidence' "$status_file" 'reject untrusted evidence'
  [[ "$status_granularity" == "$granularity" ]] || fail failure_signaling 'operator: resolve contradictory granularity' "$status_file" 'stop on ambiguous evidence'
  case "$status_outcome" in SUCCEEDED|BLOCKED|NOT_TESTED) ;; *) fail failure_signaling 'operator: resolve contradictory outcome' "$status_file" 'stop on ambiguous evidence' ;; esac
  [[ "$application_exit" =~ ^(0|[1-9][0-9]*)$|^NOT_OBSERVED$ ]] || fail failure_signaling 'operator: resolve invalid application exit evidence' "$status_file" 'stop on ambiguous evidence'
  [[ "$wrapper_status" =~ ^(0|[1-9][0-9]*)$|^NOT_STARTED$|^NOT_COMPLETE$ ]] || fail failure_signaling 'operator: resolve invalid wrapper exit evidence' "$status_file" 'stop on ambiguous evidence'
  if [[ "$terminal_outcome" == SUCCEEDED ]]; then
    [[ "$status_outcome" == SUCCEEDED && "$application_exit" == 0 && "$wrapper_status" == 0 ]] || fail failure_signaling "operator: resolve contradictory success for $granularity" "$status_file" 'stop and retain the run evidence'
  fi
done

case "$terminal_outcome" in
  SUCCEEDED) ;;
  BLOCKED|NOT_TESTED) ;;
  *) fail failure_signaling 'operator: resolve unknown terminal outcome' "$terminal_file" 'stop on ambiguous evidence' ;;
esac

printf 'run_id=%s terminal_outcome=%s failing_stage=%s owner_action=%s evidence_path=%s rollback_stop_boundary=%s\n' \
  "$run_id" "$terminal_outcome" "$(field "$terminal_file" failing_stage)" "$(field "$terminal_file" owner_action)" \
  "$(field "$terminal_file" evidence_path)" "$(field "$terminal_file" rollback_stop_boundary)"
[[ "$terminal_outcome" == SUCCEEDED ]]
