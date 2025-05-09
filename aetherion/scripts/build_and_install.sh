#!/usr/bin/env bash
set -e

# Build and test
echo "Building and testing..."
rm -rf build && mkdir build && cd build
cmake -DCMAKE_CXX_FLAGS="-fdiagnostics-color=always" -DBUILD_WORLD_TEST=OFF -G Ninja -DCMAKE_PREFIX_PATH="${PREFIX_PATH:-/default/path/to/your/env}" ..
stdbuf -oL ninja
cd .. 

# Generate stubs
# TODO: Fix here >> This needed to be removed because it is not correctly placed in the correct place in the build script.
# echo "Generating stubs for aetherion..."
# PYTHONPATH=build python -m nanobind.stubgen -m site-packages.aetherion -M py.typed -o aetherion.pyi

# Install package into local site-packages
echo "Installing package into site-packages/aetherion..."
rm -rf site-packages/aetherion
mkdir -p site-packages/aetherion
cp aetherion/__init__.py site-packages/aetherion/
cp build/_aetherion*.so site-packages/aetherion/
if [ -f aetherion.pyi ]; then cp aetherion.pyi site-packages/aetherion/; fi
if [ -f py.typed ]; then cp py.typed site-packages/aetherion/; else touch site-packages/aetherion/py.typed; fi
echo "Package installed in site-packages/aetherion"

# Install package into conda environment
echo "Installing aetherion package into your conda environment..."

if [ -n "$TARGET_PREFIX_PATH" ]; then
    SITE_PACKAGES="$TARGET_PREFIX_PATH/lib/python3.12/site-packages"
else
    SITE_PACKAGES=$(python -c "import site; print(site.getsitepackages()[0])")
fi

echo "Found site-packages directory: $SITE_PACKAGES"
rm -rf "$SITE_PACKAGES/aetherion"
cp -r site-packages/aetherion "$SITE_PACKAGES/"
echo "Installed aetherion into $SITE_PACKAGES/aetherion"

# Use the python from the target environment directly instead of trying to use conda activate
if [ -n "$TARGET_PREFIX_PATH" ]; then
    echo "Running tests with Python from $TARGET_PREFIX_PATH"
    "$TARGET_PREFIX_PATH/bin/python" -m pytest tests
else
    echo "Running tests with current Python"
    pytest tests
fi
