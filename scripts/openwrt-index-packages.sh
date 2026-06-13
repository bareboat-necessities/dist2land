#!/usr/bin/env bash
set -euo pipefail

INPUT_DIR="${1:?usage: openwrt-index-packages.sh INPUT_DIR OUTPUT_DIR [flat]}"
OUTPUT_DIR="${2:?usage: openwrt-index-packages.sh INPUT_DIR OUTPUT_DIR [flat]}"
LAYOUT="${3:-flat}"

rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

if [[ "${LAYOUT}" == "flat" ]]; then
  find "${INPUT_DIR}" -type f -name '*.ipk' -exec cp -v {} "${OUTPUT_DIR}/" \;
else
  find "${INPUT_DIR}" -type f -name '*.ipk' -print0 | while IFS= read -r -d '' pkg; do
    rel="${pkg#${INPUT_DIR}/}"
    mkdir -p "${OUTPUT_DIR}/$(dirname "${rel}")"
    cp -v "${pkg}" "${OUTPUT_DIR}/${rel}"
  done
fi

if ! compgen -G "${OUTPUT_DIR}/*.ipk" >/dev/null && [[ "${LAYOUT}" == "flat" ]]; then
  echo "No .ipk files found under ${INPUT_DIR}" >&2
  exit 1
fi

(
  cd "${OUTPUT_DIR}"
  if command -v opkg-make-index >/dev/null 2>&1; then
    opkg-make-index . > Packages
  else
    for pkg in *.ipk; do
      [ -f "${pkg}" ] || continue
      tmpdir="$(mktemp -d)"
      (cd "${tmpdir}" && ar x "${OLDPWD}/${pkg}" control.tar.gz && tar -xzf control.tar.gz ./control && cat control)
      rm -rf "${tmpdir}"
      echo "Filename: ${pkg}"
      echo "Size: $(stat -c%s "${pkg}")"
      echo
    done > Packages
  fi
  gzip -9c Packages > Packages.gz
)
