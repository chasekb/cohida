#!/usr/bin/env bash
set -Eeuo pipefail

sed -E \
  -e "s/(COINBASE_API_KEY|COINBASE_API_SECRET|COINBASE_API_PASSPHRASE|POSTGRES_DB_PASSWORD|DB_PASSWORD)=[\"'][^[:space:]\"']+[\"']?/\\1=[REDACTED]/Ig" \
  -e "s/([\"']?[Aa][Uu][Tt][Hh][Oo][Rr][Ii][Zz][Aa][Tt][Ii][Oo][Nn][\"']?[[:space:]]*:[[:space:]]*[\"']?)([Bb][Ee][Aa][Rr][Ee][Rr]|[Bb][Aa][Ss][Ii][Cc])[[:space:]]+[^\"']+[\"']?/\\1\\2 [REDACTED]/g" \
  -e "s/([Aa][Uu][Tt][Hh][Oo][Rr][Ii][Zz][Aa][Tt][Ii][Oo][Nn][[:space:]]*:[[:space:]]*)([Bb][Ee][Aa][Rr][Ee][Rr]|[Bb][Aa][Ss][Ii][Cc])[[:space:]]+[^[:space:]\"']+/\\1\\2 [REDACTED]/g"
