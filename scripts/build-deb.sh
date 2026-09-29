#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${QPLAY_BUILD_DIR:-${repo_root}/build-deb}"
dist_dir="${QPLAY_DIST_DIR:-${repo_root}/dist}"
version="$(sed -nE '/^project\(qPlay/,/^\)/ s/^[[:space:]]*VERSION[[:space:]]+([0-9]+(\.[0-9]+)*)[[:space:]]*$/\1/p' "${repo_root}/CMakeLists.txt" | head -n 1)"
platform="$(uname -s)"
architecture="$(uname -m)"

if [[ -z "${version}" ]]; then
    echo "Could not read qPlay version from CMakeLists.txt" >&2
    exit 1
fi

artifact_name="qPlay-${version}-${platform}-${architecture}"
mkdir -p "${dist_dir}"

cmake -S "${repo_root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCPACK_GENERATOR=DEB \
    -DCPACK_PACKAGE_DIRECTORY="${dist_dir}" \
    -DCPACK_PACKAGE_FILE_NAME="${artifact_name}" \
    -DCPACK_DEBIAN_FILE_NAME="${artifact_name}.deb"
cmake --build "${build_dir}" --parallel
cpack --config "${build_dir}/CPackConfig.cmake" -G DEB
install -m 755 "${build_dir}/bin/qPlay" "${dist_dir}/${artifact_name}.bin"

echo "Created ${dist_dir}/${artifact_name}.deb"
echo "Created ${dist_dir}/${artifact_name}.bin"
