#!/usr/bin/env bash
set -Eeuo pipefail

sed -E \
  -e 's/([A-Za-z_][A-Za-z0-9_]*(API_KEY|API_SECRET|API_PASSPHRASE|DB_PASSWORD))=["'"'']?[^[:space:]"'"'']+["'"'']?/\1=[REDACTED]/Ig' \
  -e 's/(["'"'']?[Aa][Uu][Tt][Hh][Oo][Rr][Ii][Zz][Aa][Tt][Ii][Oo][Nn]["'"'']?[[:space:]]*:[[:space:]]*["'"'']?)([Bb][Ee][Aa][Rr][Ee][Rr]|[Bb][Aa][Ss][Ii][Cc])[[:space:]]+[^"'"'']+["'"'']?/\1\2 [REDACTED]/g' \
  -e 's/([Aa][Uu][Tt][Hh][Oo][Rr][Ii][Zz][Aa][Tt][Ii][Oo][Nn][[:space:]]*:[[:space:]]*)([Bb][Ee][Aa][Rr][Ee][Rr]|[Bb][Aa][Ss][Ii][Cc])[[:space:]]+[^[:space:]"'"'']+/\1\2 [REDACTED]/g'
