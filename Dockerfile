
# Stage: Build OpenVDB from source
FROM ubuntu:24.04 AS openvdb-build

# 1) Install system dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        build-essential \
        cmake \
        ninja-build \
        git \
        pkg-config \
        zlib1g-dev \
        libfmt-dev \
        libboost-iostreams-dev \
        libtbb-dev \
        libblosc-dev \
    && rm -rf /var/lib/apt/lists/*

# 2) Clone & configure OpenVDB
RUN git clone --depth 1 https://github.com/AcademySoftwareFoundation/openvdb.git /tmp/openvdb \
    && cd /tmp/openvdb \
    && git fetch origin --tags && git checkout v11.0.0 \
    && mkdir /tmp/openvdb/build \
    && cd /tmp/openvdb/build \
    && cmake .. \
    && make -j4 && make install

FROM ubuntu:24.04
COPY --from=openvdb-build /usr/local /usr/local

# avoid prompts during apt installs
ARG DEBIAN_FRONTEND=noninteractive

# 1) Install system build tools + dev‐libs
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        wget \
        bzip2 \
        ca-certificates \
        git \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
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

# 3) Install Miniconda
ENV CONDA_DIR=/opt/conda
RUN wget --quiet https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh \
      -O /tmp/miniconda.sh \
 && bash /tmp/miniconda.sh -b -p $CONDA_DIR \
 && rm /tmp/miniconda.sh \
 # config conda for strict, non-interactive installs
 && $CONDA_DIR/bin/conda config --set always_yes yes --set changeps1 no \
 && $CONDA_DIR/bin/conda config --set channel_priority strict \
 && $CONDA_DIR/bin/conda update -q conda

ENV PATH="$CONDA_DIR/bin:$PATH"

# 4) Copy in your environment spec
WORKDIR /project
COPY . /project/

# 5) Create the exact Conda env you use locally
RUN conda env create -f environment.yml \
 && conda clean -afy

RUN conda init bash
RUN echo "conda activate aetherion-312" > ~/.bashrc

RUN conda run -n aetherion-312 pip install -r dev-requirements.txt \
    && conda run -n aetherion-312 pip install --upgrade build scikit-build-core

# 6) Default to your env in any following RUN/SHELL
SHELL ["conda", "run", "-n", "aetherion-312", "/bin/bash", "-lc"]

# 7) By default drop you into bash with env active
CMD ["bash"]
