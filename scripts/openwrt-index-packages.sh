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

extract_control() {
  local pkg="${1:?package path required}"
  local tmpdir="${2:?temporary directory required}"
  local control_archive=""

  if ar t "${pkg}" >/dev/null 2>&1; then
    control_archive="$(ar t "${pkg}" | awk '/^control\.tar(\.|$)/ { print; exit }')"
    if [[ -z "${control_archive}" ]]; then
      echo "OpenWrt package is missing a control archive: ${pkg}" >&2
      return 1
    fi
    (cd "${tmpdir}" && ar x "${pkg}" "${control_archive}")
    tar -xaf "${tmpdir}/${control_archive}" -C "${tmpdir}" ./control
  elif tar -tf "${pkg}" ./control >/dev/null 2>&1; then
    tar -xf "${pkg}" -C "${tmpdir}" ./control
  elif control_archive="$(tar -tf "${pkg}" 2>/dev/null | awk '/^\.?\/?control\.tar(\.|$)/ { print; exit }')" && [[ -n "${control_archive}" ]]; then
    tar -xf "${pkg}" -C "${tmpdir}" "${control_archive}"
    tar -xaf "${tmpdir}/${control_archive#./}" -C "${tmpdir}" ./control
  else
    echo "Unsupported OpenWrt package archive format: ${pkg}" >&2
    return 1
  fi

  cat "${tmpdir}/control"
}

(
  cd "${OUTPUT_DIR}"
  if command -v opkg-make-index >/dev/null 2>&1; then
    opkg-make-index . > Packages
  else
    for pkg in *.ipk; do
      [ -f "${pkg}" ] || continue
      tmpdir="$(mktemp -d)"
      extract_control "${PWD}/${pkg}" "${tmpdir}"
      rm -rf "${tmpdir}"
      echo "Filename: ${pkg}"
      echo "Size: $(stat -c%s "${pkg}")"
      echo
    done > Packages
  fi
  gzip -9c Packages > Packages.gz
)
