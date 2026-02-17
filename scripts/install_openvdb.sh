#!/usr/bin/env bash
set -euo pipefail

# 1) Install build tools, CA certs, Zlib headers, and pkg-config
sudo apt-get update \
  && sudo apt-get install -y --no-install-recommends \
       ca-certificates \
       build-essential \
       cmake \
       ninja-build \
       git \
       pkg-config \
       zlib1g-dev \
       libboost-iostreams-dev \
       libtbb-dev \
       libblosc-dev \
  && sudo rm -rf /var/lib/apt/lists/*

# 2) Clone the OpenVDB repository
git clone --depth 1 https://github.com/AcademySoftwareFoundation/openvdb.git /tmp/openvdb
cd /tmp/openvdb

# 3) Configure & build
mkdir -p build && cd build
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      ..
ninja

# 4) Install into /usr/local
sudo ninja install

# 5) Cleanup
cd /
rm -rf /tmp/openvdb

echo "OpenVDB has been successfully built and installed to /usr/local"
