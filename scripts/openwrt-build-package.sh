#!/usr/bin/env bash
set -euo pipefail

OPENWRT_VERSION="${OPENWRT_VERSION:-24.10.6}"
OPENWRT_TARGET="${OPENWRT_TARGET:?OPENWRT_TARGET must be set, for example x86/64}"
OPENWRT_PACKAGE_VERSION="${OPENWRT_PACKAGE_VERSION:-0.0.0}"
OPENWRT_PACKAGE_RELEASE="${OPENWRT_PACKAGE_RELEASE:-1}"
OPENWRT_JOBS="${OPENWRT_JOBS:-$(nproc)}"

PKG_NAME="dist2land"
REPO_URL="${GITHUB_SERVER_URL:-https://github.com}/${GITHUB_REPOSITORY:-bareboat-necessities/dist2land}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="${ROOT_DIR}/.openwrt-sdk"
DIST_DIR="${ROOT_DIR}/dist/openwrt/${OPENWRT_TARGET//\//-}"
TARGET_DASH="${OPENWRT_TARGET//\//-}"
SDK_BASENAME="openwrt-sdk-${OPENWRT_VERSION}-${TARGET_DASH}_gcc-13.3.0_musl.Linux-x86_64"
SDK_ARCHIVE="${SDK_BASENAME}.tar.zst"
SDK_URL="https://downloads.openwrt.org/releases/${OPENWRT_VERSION}/targets/${OPENWRT_TARGET}/${SDK_ARCHIVE}"
SDK_DIR="${WORK_DIR}/${SDK_BASENAME}"

rm -rf "${DIST_DIR}"
mkdir -p "${WORK_DIR}" "${DIST_DIR}"

if [[ ! -d "${SDK_DIR}" ]]; then
  rm -f "${WORK_DIR}/${SDK_ARCHIVE}"
  echo "Downloading OpenWrt SDK: ${SDK_URL}"
  curl -fsSL --retry 3 --retry-delay 2 -o "${WORK_DIR}/${SDK_ARCHIVE}" "${SDK_URL}"
  tar --use-compress-program=unzstd -xf "${WORK_DIR}/${SDK_ARCHIVE}" -C "${WORK_DIR}"
fi

PACKAGE_DIR="${SDK_DIR}/package/${PKG_NAME}"
rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}/src"

rsync -a --delete \
  --exclude '.git' \
  --exclude '.github' \
  --exclude '.openwrt-sdk' \
  --exclude 'build' \
  --exclude 'dist' \
  "${ROOT_DIR}/" "${PACKAGE_DIR}/src/"

cat > "${PACKAGE_DIR}/Makefile" <<EOF_MAKE
include \$(TOPDIR)/rules.mk

PKG_NAME:=dist2land
PKG_VERSION:=${OPENWRT_PACKAGE_VERSION}
PKG_RELEASE:=${OPENWRT_PACKAGE_RELEASE}
PKG_BUILD_DIR:=\$(BUILD_DIR)/\$(PKG_NAME)-\$(PKG_VERSION)

include \$(INCLUDE_DIR)/package.mk
include \$(INCLUDE_DIR)/cmake.mk

define Package/dist2land
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Distance-to-land command-line tool
  URL:=${REPO_URL}
  DEPENDS:=+libstdcpp +libcurl +libarchive +libgdal
endef

define Package/dist2land/description
  Compute distance to nearest land from a GPS coordinate using provider datasets.
endef

define Build/Prepare
	mkdir -p \$(PKG_BUILD_DIR)
	\$(CP) ${PACKAGE_DIR}/src/. \$(PKG_BUILD_DIR)/
endef

CMAKE_OPTIONS += -DCMAKE_BUILD_TYPE=Release

define Package/dist2land/install
	\$(INSTALL_DIR) \$(1)/usr/bin
	\$(INSTALL_BIN) \$(PKG_BUILD_DIR)/dist2land \$(1)/usr/bin/dist2land
	\$(INSTALL_DIR) \$(1)/usr/share/dist2land
	\$(INSTALL_DATA) \$(PKG_BUILD_DIR)/share/dist2land/providers.ini \$(1)/usr/share/dist2land/providers.ini
endef

\$(eval \$(call BuildPackage,dist2land))
EOF_MAKE

pushd "${SDK_DIR}" >/dev/null
./scripts/feeds update -a
./scripts/feeds install libcurl libarchive gdal || ./scripts/feeds install -a
popd >/dev/null

make -C "${SDK_DIR}" defconfig
make -C "${SDK_DIR}" package/${PKG_NAME}/compile V=s -j"${OPENWRT_JOBS}"

find "${SDK_DIR}/bin/packages" "${SDK_DIR}/bin/targets" -type f -name "${PKG_NAME}_*.ipk" -exec cp -v {} "${DIST_DIR}/" \;
if ! compgen -G "${DIST_DIR}/${PKG_NAME}_*.ipk" >/dev/null; then
  echo "No ${PKG_NAME} OpenWrt package was produced" >&2
  exit 1
fi
