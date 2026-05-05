# Include environment variables from .env if it exists
ifneq ("$(wildcard .env)","")
    include .env
    export
endif

# Default value for PREFIX_PATH if not set in .env (Right now it must be set in the .env)
PREFIX_PATH ?= /default/path/to/your/env
# Use TARGET_PREFIX_PATH from .env if available
TARGET_PREFIX_PATH ?= $(PREFIX_PATH)

# Conda env for conda run targets (override: CONDA_ENV=myenv make test, or set in .env)
CONDA_ENV ?= aetherion-312

clean-build-run:
	rm -rf build && mkdir build &&  \
	cmake -S . -B ./build -DCMAKE_PREFIX_PATH="$(PREFIX_PATH)" &&  \
	cd build && make &&  \
	cd .. &&  \
	cp build/lib/libmy_module.so my_module.so &&  \
	python test.py

.PHONY: build-test
build-test: build test
	@echo "Build and test completed."


.PHONY: test
test:
	@echo "Running aetherion tests with pytest..."
	conda run --no-capture-output -n $(CONDA_ENV) pytest tests

# Agent-friendly test target. Identical to `test`, except it preloads the
# system libtinfo + libreadline so pytest>=8.4's `_readline_workaround`
# (which calls `import readline`) does not segfault against the broken
# readline/ncurses pair shipped in the conda env.
# See .claude/docs/analysis/2026-05-02-make-test-segfault.md
AGENT_LD_PRELOAD := /lib/x86_64-linux-gnu/libtinfo.so.6 /lib/x86_64-linux-gnu/libreadline.so.8

.PHONY: agent-test
agent-test:
	@echo "Running aetherion tests with pytest (agent-safe, LD_PRELOAD workaround)..."
	conda run --no-capture-output -n $(CONDA_ENV) \
		bash -c 'LD_PRELOAD="$(AGENT_LD_PRELOAD)" pytest tests $(PYTEST_ARGS)'

# Run a single test, file, or directory in isolation. Same LD_PRELOAD safety
# as `agent-test`. Adds `-x -v -p no:cacheprovider` for tight focused runs.
#
# Usage:
#   make agent-test-one TEST=tests/world/test_world_manager_lifecycle.py
#   make agent-test-one TEST=tests/world/test_world_manager_lifecycle.py::test_update_only_runs_when_status_running
#   make agent-test-one TEST=tests/world PYTEST_ARGS="-k status"
.PHONY: agent-test-one
agent-test-one:
	@if [ -z "$(TEST)" ]; then \
		echo "ERROR: TEST=<path-or-nodeid> is required."; \
		echo "Example: make agent-test-one TEST=tests/world/test_world_manager_lifecycle.py::test_update_only_runs_when_status_running"; \
		exit 2; \
	fi
	@echo "Running isolated test: $(TEST)"
	conda run --no-capture-output -n $(CONDA_ENV) \
		bash -c 'LD_PRELOAD="$(AGENT_LD_PRELOAD)" pytest -x -v -p no:cacheprovider "$(TEST)" $(PYTEST_ARGS)'

.PHONY: coverage
coverage:
	@echo "Running tests with coverage report..."
	conda run --no-capture-output -n $(CONDA_ENV) \
		pytest tests \
		--cov=aetherion \
		--cov-report=term-missing \
		--cov-report=html
	@echo "Coverage HTML report generated at htmlcov/index.html"

.PHONY: test-ci
test-ci:
	@echo "Running canonical local test command (coverage + status)..."
	@STATUS_FILE=".test_status"; \
	if conda run --no-capture-output -n $(CONDA_ENV) \
		pytest tests \
		--cov=aetherion \
		--cov-report=term-missing \
		--cov-report=html \
		--cov-report=xml:coverage.xml; then \
		echo "passing" > $$STATUS_FILE; \
		echo "Test status saved to $$STATUS_FILE"; \
	else \
		RESULT=$$?; \
		echo "failing" > $$STATUS_FILE; \
		echo "Test status saved to $$STATUS_FILE"; \
		exit $$RESULT; \
	fi

.PHONY: badges
badges:
	@echo "Generating local badges from test artifacts..."
	conda run --no-capture-output -n $(CONDA_ENV) python scripts/generate_badges.py \
		--coverage coverage.xml \
		--status .test_status \
		--output _images

.PHONY: test-badges
test-badges:
	@echo "Running tests and refreshing local badges..."
	@RESULT=0; \
	$(MAKE) test-ci || RESULT=$$?; \
	$(MAKE) badges; \
	exit $$RESULT

device-info:
	nvidia-smi

.PHONY: build
build:
	@echo "Building aetherion package with python -m build..."
	conda run --no-capture-output -n $(CONDA_ENV) python -m build

.PHONY: install
install:
	@echo "Installing latest aetherion wheel from dist/..."
	@WHEEL=$$(ls -t dist/aetherion-*.whl 2>/dev/null | head -n 1); \
	if [ -z "$$WHEEL" ]; then \
		echo "No wheel found in dist/. Run 'make build' first."; \
		exit 1; \
	fi; \
	echo "Using $$WHEEL"; \
	conda run --no-capture-output -n $(CONDA_ENV) pip install --force-reinstall "$$WHEEL"

.PHONY: build-install-test
build-install-test: build install test
	@echo "Build, install, and test completed."

# Agent-friendly variant: uses `agent-test` (LD_PRELOAD readline workaround)
# instead of `test`. See .claude/docs/analysis/2026-05-02-make-test-segfault.md
.PHONY: agent-build-install-test
agent-build-install-test: build install agent-test
	@echo "Build, install, and agent-test completed."

.PHONY: clang-format
clang-format:
	@echo "Formatting C++ files with clang-format..."
	find ./src ./webclient/wasm -type f -regex '.*\.\(cpp\|hpp\|h\|cxx\|cc\)' | \
	xargs clang-format -i

.PHONY: clang-format-check
clang-format-check:
	@echo "Checking C++ formatting with clang-format..."
	find ./src ./webclient/wasm -type f -regex '.*\.\(cpp\|hpp\|h\|cxx\|cc\)' | \
	xargs -r clang-format --dry-run --Werror

.PHONY: python-format
python-format:
	@echo "Formatting and sorting imports with Ruff..."
	conda run --no-capture-output -n $(CONDA_ENV) ruff format . && conda run --no-capture-output -n $(CONDA_ENV) ruff check --fix .

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


.PHONY: wheel-install
wheel-install: install

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

.PHONY: cpp-tests
cpp-tests:
	@echo "Building and running C++ tests..."
	@rm -rf build-tests && mkdir build-tests
	@cd build-tests && \
	cmake ../tests/cpp \
		-DCMAKE_PREFIX_PATH="$(PREFIX_PATH)" \
		-DCMAKE_BUILD_TYPE=Debug && \
	make test_water_simulation
	@echo "Running water simulation test..."
	@./build-tests/test_water_simulation

.PHONY: cpp-tests-only
cpp-tests-only:
	@echo "Running existing C++ tests..."
	@if [ -d "build-tests" ] && [ -f "build-tests/test_water_simulation" ]; then \
		./build-tests/test_water_simulation; \
	else \
		echo "No test build found. Run 'make cpp-tests' first."; \
	fi
