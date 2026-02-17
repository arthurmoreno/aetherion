Installation
===============

Welcome to the Getting Started guide!

This guide explains how to install the Aetherion game engine on Ubuntu 24.04 LTS by manually building required system dependencies, OpenVDB, FlatBuffers, and setting up the Python environment.


#################
Ubuntu 24.04 LTS
#################

The examples below target Ubuntu 24.04 LTS. Other distributions may require
adjustments to package names or paths. All commands are intended to be run from
the terminal.

Ensure you have Python 3.12 available::

   python3 --version

After installing the ``lifesimcore`` package you can verify it with::

   python -c "import lifesimcore, sys; print('lifesimcore', lifesimcore.__version__, 'on', sys.version)"

=================
Installing OpenVDB v11.0
=======================


1. Installing pre compiled OpenVDB v11.0
-----------------

Download and install pre compiled OpenVDB v11.0:

.. code-block:: bash
   
   curl -sSL https://arthurmoreno.github.io/aetherion-docs/wheel-html-index/system-libs/openvdb-11-0-1-1_amd64.deb -o /tmp/openvdb.deb \
      && sudo dpkg -i /tmp/openvdb.deb && sudo apt-get install -f -y && rm /tmp/openvdb.deb


2. Compiling and Installing OpenVDB v11.0
-----------------

Install system dependencies for OpenVDB v11.0:

.. code-block:: bash

   sudo apt-get update && sudo apt-get install -y --no-install-recommends \
         ca-certificates \
         build-essential \
         cmake \
         ninja-build \
         git \
         pkg-config \
         zlib1g-dev \
         libboost-iostreams-dev \
         libtbb-dev \
         libblosc-dev

Clone & build and install OpenVDB v11.0:

.. code-block:: bash

   git clone --depth 1 https://github.com/AcademySoftwareFoundation/openvdb.git /tmp/openvdb \
      && cd /tmp/openvdb \
      && git fetch origin --tags && git checkout v11.0.0 \
      && mkdir /tmp/openvdb/build \
      && cd /tmp/openvdb/build \
      && cmake .. \
      && make -j4 && make install

Clone & build and install OpenVDB v11.0:

.. code-block:: bash
   
   curl -sSL https://arthurmoreno.github.io/aetherion-docs/wheel-html-index/system-libs/openvdb-11-0-1-1_amd64.deb -o /tmp/openvdb.deb \
      && sudo dpkg -i /tmp/openvdb.deb && sudo apt-get install -f -y && rm /tmp/openvdb.deb


Installing Aetherion Game Engine system dependencies
----------------------------------------------------

Install system build tools + dev‐libs:

.. code-block:: bash

   sudo apt-get update \
      && sudo apt-get install -y --no-install-recommends \
         wget \
         bzip2 \
         ca-certificates \
         git \
         build-essential \
         cmake \
         ninja-build \
         pkg-config \
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
         libblosc-dev


Build and installing flatbuffers:

.. code-block:: bash

   FLATBUFFERS_VERSION=24.3.25 && \
   git clone --depth 1 -b "v${FLATBUFFERS_VERSION}" https://github.com/google/flatbuffers.git /tmp/flatbuffers && \
   mkdir -p /tmp/flatbuffers/build && \
   cd /tmp/flatbuffers/build && \
   cmake \
      -DFLATBUFFERS_BUILD_TESTS=OFF \
      -DFLATBUFFERS_BUILD_CPP17=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DFLATBUFFERS_ENABLE_PCH=ON \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DCMAKE_INSTALL_LIBDIR=lib \
      .. && \
   make -j"$(nproc)" && \
   sudo make install && \
   rm -rf /tmp/flatbuffers

Installing miniconda on Ubuntu. (`Also see the official miniconda install tutorial <https://www.anaconda.com/docs/getting-started/miniconda/install#linux-terminal-installer>`_)

.. code-block:: bash

   CONDA_DIR=/opt/conda && \
   wget --quiet https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O /tmp/miniconda.sh && \
   bash /tmp/miniconda.sh -b -p "${CONDA_DIR}" && \
   rm /tmp/miniconda.sh && \
   "${CONDA_DIR}/bin/conda" config --set always_yes yes --set changeps1 no && \
   "${CONDA_DIR}/bin/conda" config --set channel_priority strict && \
   "${CONDA_DIR}/bin/conda" update -q conda


Creating a conda environment
----------------------------
To create a conda environment for the project, use the following command:

.. code-block:: bash

   conda create --name aetherion-312 python=3.12

.. code-block:: bash

   conda activate aetherion-312

Installing the game engine python package
-----------------------------------------

The --index-url option instructs pip to use our custom Aetherion wheel index on GitHub Pages so that you get pre-built binary packages optimized for your platform, avoiding long build times and compilation errors.
To install the game engine python package, use the following command:

::

   pip install --index-url https://arthurmoreno.github.io/aetherion-docs/wheel-html-index/ aetherion



