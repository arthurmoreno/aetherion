
ARG OPENVDB_BASE_IMAGE=aetherion-openvdb:openvdb-11.0.0
FROM ${OPENVDB_BASE_IMAGE}

# avoid prompts during apt installs
ARG DEBIAN_FRONTEND=noninteractive

# 1) Install system build tools + dev‐libs
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        wget \
        curl \
        zip \
        unzip \
        tar \
        bzip2 \
        ca-certificates \
        git \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        libopengl-dev \
        libglvnd-dev \
        mesa-common-dev \
        libgl1-mesa-dev \
        libfmt-dev \
        libeigen3-dev \
        libtbb-dev \
        libsdl2-dev \
        libsdl2-image-dev \
        libsdl2-ttf-dev \
        libmsgpack-dev \
        libmsgpack-cxx-dev \
        liblmdb-dev \
        libspdlog-dev \
        libboost-iostreams-dev \
        libboost-all-dev \
        zlib1g-dev \
        libblosc-dev \
    && rm -rf /var/lib/apt/lists/*

# 2) Build & install FlatBuffers
ARG FLATBUFFERS_VERSION=24.3.25
RUN git clone --depth 1 -b v${FLATBUFFERS_VERSION} \
      https://github.com/google/flatbuffers.git /tmp/flatbuffers \
 && mkdir /tmp/flatbuffers/build \
 && cd /tmp/flatbuffers/build \
 && cmake \
      -DFLATBUFFERS_BUILD_TESTS=OFF \
      -DFLATBUFFERS_BUILD_CPP17=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DFLATBUFFERS_ENABLE_PCH=ON \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DCMAKE_INSTALL_LIBDIR=lib \
      .. \
 && make -j"$(nproc)" install \
 && rm -rf /tmp/flatbuffers

# ensure flatc is on PATH
ENV PATH="/usr/local/bin:${PATH}"

# 3) Cacheable vcpkg manifest bootstrap (materializes entt/flatbuffers/nanobind under /opt/aetherion-libs)
ENV VCPKG_ROOT=/opt/vcpkg
ENV VCPKG_CACHE_ROOT=/opt/vcpkg-user-cache
WORKDIR /opt/aetherion-bootstrap
COPY vcpkg.json vcpkg-configuration.json vcpkg-lock.json third_party.lock ./
COPY scripts/install_third_party_libs.sh ./scripts/install_third_party_libs.sh
RUN chmod +x ./scripts/install_third_party_libs.sh \
 && VCPKG_ROOT="$VCPKG_ROOT" VCPKG_CACHE_ROOT="$VCPKG_CACHE_ROOT" \
    ./scripts/install_third_party_libs.sh /opt/aetherion-libs

# 4) Install Miniconda
ENV CONDA_DIR=/opt/conda

RUN wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-$(uname -m).sh \
      -O /tmp/miniconda.sh \
 && bash /tmp/miniconda.sh -b -p $CONDA_DIR \
 && rm /tmp/miniconda.sh \
 # config conda for strict, non-interactive installs
 && $CONDA_DIR/bin/conda config --set always_yes yes \
 && $CONDA_DIR/bin/conda config --set channel_priority strict

ENV PATH="$CONDA_DIR/bin:$PATH"

# 5) Copy project sources
WORKDIR /project
COPY . /project/
RUN mkdir -p /project/libs \
 && [ -e /project/libs/entt ] || ln -s /opt/aetherion-libs/entt /project/libs/entt \
 && [ -e /project/libs/flatbuffers ] || ln -s /opt/aetherion-libs/flatbuffers /project/libs/flatbuffers \
 && [ -e /project/libs/nanobind ] || ln -s /opt/aetherion-libs/nanobind /project/libs/nanobind

# 6) Create the exact Conda env you use locally
# First, accept the Terms of Service for the default channels
RUN conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/main \
 && conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/r

# 7) Create the exact Conda env you use locally
RUN conda env create -f environment.yml \
 && conda clean -afy

RUN conda init bash
RUN echo "conda activate aetherion-312" > ~/.bashrc

RUN conda run -n aetherion-312 pip install -r dev-requirements.txt \
    && conda run -n aetherion-312 pip install --upgrade build scikit-build-core

# 8) Default to your env in any following RUN/SHELL
SHELL ["conda", "run", "-n", "aetherion-312", "/bin/bash", "-lc"]

# 9) By default drop you into bash with env active
CMD ["bash"]
