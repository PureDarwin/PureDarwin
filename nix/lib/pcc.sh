# pcc.sh - compile jobs for nix builders, $NIX_BUILD_CORES of them at once
# pcc CMD... runs CMD in the background, pcc_wait waits for them all and fails if any one failed

pcc_dir=$(mktemp -d)
pcc_limit=${NIX_BUILD_CORES:-1}
[ "$pcc_limit" -gt 0 ] 2>/dev/null || pcc_limit=$(nproc)

pcc() {
  while [ "$(jobs -rp | wc -l)" -ge "$pcc_limit" ]; do
    wait -n || true
  done
  { "$@" || { echo "pcc: failed: $*" >&2; touch "$pcc_dir/failed"; }; } &
}

pcc_wait() {
  wait
  if [ -e "$pcc_dir/failed" ]; then
    rm -f "$pcc_dir/failed"
    return 1
  fi
}
