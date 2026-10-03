#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NANOPB="$ROOT/nanopb-0.4.9/generator-bin/protoc"

echo "This script requires https://jpa.kapsi.fi/nanopb/download/ version 0.4.9 to be located in the"
echo "firmware root directory if the following step fails, you should download the correct"
echo "prebuilt binaries for your computer into nanopb-0.4.9"

# Meshtastic upstream protos (protobufs submodule). The nanopb tool expects .options beside .proto (cwd here).
cd "$ROOT/protobufs"
"$NANOPB" --experimental_allow_proto3_optional "--nanopb_out=-S.cpp -v:$ROOT/src/mesh/generated/" -I="$ROOT/protobufs" meshtastic/*.proto

# LoBBS: per-app protos under src/modules/LoBBS/apps/ (not part of meshtastic/protobufs).
LOBBS="$ROOT/src/modules/LoBBS"
for app_proto in Auth/auth Mail/mail News/news Yarn/yarn Wall/wall; do
  app_dir="${app_proto%%/*}"
  proto_base="${app_proto##*/}"
  cd "$LOBBS/apps/$app_dir"
  "$NANOPB" --experimental_allow_proto3_optional "--nanopb_out=-S.cpp -v:." -I=. "${proto_base}.proto"
done

# LoDB: diagnostics proto embedded in the firmware source tree.
cd "$ROOT/src/lodb"
"$NANOPB" --experimental_allow_proto3_optional "--nanopb_out=-S.cpp -v:." -I=. diagnostics.proto
