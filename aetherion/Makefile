# Include environment variables from .env if it exists
ifneq ("$(wildcard .env)","")
    include .env
    export
endif

# Default value for PREFIX_PATH if not set in .env (Right now it must be set in the .env)
PREFIX_PATH ?= /default/path/to/your/env
# Use TARGET_PREFIX_PATH from .env if available
TARGET_PREFIX_PATH ?= $(PREFIX_PATH)

clean-build-run:
	rm -rf build && mkdir build &&  \
	cmake -S . -B ./build -DCMAKE_PREFIX_PATH="$(PREFIX_PATH)" &&  \
	cd build && make &&  \
	cd .. &&  \
	cp build/lib/libmy_module.so my_module.so &&  \
	python test.py

build-test:
	rm -rf build && \
	mkdir build && \
	cd build && \
	cmake -DCMAKE_CXX_FLAGS="-fdiagnostics-color=always" -DBUILD_WORLD_TEST=OFF -G Ninja -DCMAKE_PREFIX_PATH="$(PREFIX_PATH)" .. && \
	stdbuf -oL ninja && \
	cd .. && \
	pytest tests

device-info:
	nvidia-smi

.PHONY: clang-format
clang-format:
	@echo "Formatting C++ files with clang-format..."
	find ./src -type f -regex '.*\.\(cpp\|hpp\|h\|cxx\|cc\)' | \
	xargs clang-format -i

.PHONY: python-format
python-format:
	@echo "Formatting and sorting imports with Ruff..."
	ruff format . && ruff check --fix .

.PHONY: format
format: clang-format python-format
	@echo "All formatting complete."

.PHONY: generate-stubs
generate-stubs:
	@echo "Generating stubs for lifesimcore..."
	PYTHONPATH=build python -m nanobind.stubgen -m site-packages.lifesimcore -M py.typed -o lifesimcore.pyi

# TODO: Refactor to new way of installing the package (using pip or similar)
.PHONY: install-package
install-package:
	@echo "Installing package into site-packages/lifesimcore..."
# Remove any previous installation in our local site-packages folder
	rm -rf site-packages/lifesimcore
# Create the destination directory
	mkdir -p site-packages/lifesimcore
# Copy the package __init__.py from src/
	cp lifesimcore/__init__.py site-packages/lifesimcore/
# Copy the shared library (.so file) from build/ folder
	cp build/lifesimcore*.so site-packages/lifesimcore/
# Copy the stub file if it exists (ignore error if it doesn't)
	if [ -f lifesimcore.pyi ]; then cp lifesimcore.pyi site-packages/lifesimcore/; fi
# Create an empty py.typed marker file
	if [ -f py.typed ]; then cp py.typed site-packages/lifesimcore/; else touch site-packages/lifesimcore/py.typed; fi
	@echo "Package installed in site-packages/lifesimcore"

.PHONY: conda-install
conda-install:
	@echo "Installing lifesimcore package into your conda environment..."
	@SITE_PACKAGES=$$(python -c "import site; print(site.getsitepackages()[0])") && \
	echo "Found site-packages directory: $$SITE_PACKAGES" && \
	rm -rf $$SITE_PACKAGES/lifesimcore && \
	cp -r site-packages/lifesimcore $$SITE_PACKAGES/ && \
	echo "Installed lifesimcore into $$SITE_PACKAGES/lifesimcore"


.PHONY: build-and-install
build-and-install:
	@echo "Running build-and-install script..."
	@if [ -n "$(TARGET_PREFIX_PATH)" ]; then \
		echo "Using TARGET_PREFIX_PATH from environment: $(TARGET_PREFIX_PATH)"; \
		TARGET_PREFIX_PATH="$(TARGET_PREFIX_PATH)" bash ./scripts/build_and_install.sh; \
	else \
		echo "Using default TARGET_PREFIX_PATH from script"; \
		bash ./scripts/build_and_install.sh; \
	fi
