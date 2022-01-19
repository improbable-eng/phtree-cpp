#!/usr/bin/env bash

source ci/includes/os.sh

# Main function that should be used by scripts sourcing this file.
function runBazel() {
  BAZEL_SUBCOMMAND="$1"
  shift
  "$(pwd)/tools/bazel" "$BAZEL_SUBCOMMAND" ${BAZEL_CI_CONFIG:-} "$@"
}

function getBazelVersion() {
  echo "4.2.2"
}
