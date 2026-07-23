# Portable GNU Make entry point for Linux experiment servers.
# Override variables when needed, e.g. `make release JOBS=32 BUILD_DIR=build-gcc`.

BUILD_DIR ?= build
BUILD_TYPE ?= Release
JOBS ?= 4
PYTHON ?= python3
CMAKE ?= cmake

.PHONY: help configure build release paper-binaries tools test validate \
	validate-paper-binaries validate-all-binaries clean

help:
	@$(CMAKE) -E echo "make release              Configure, build all repository targets, run CTest"
	@$(CMAKE) -E echo "make paper-binaries       Build only abhss and pruneddp"
	@$(CMAKE) -E echo "make tools                Build data conversion/audit tools"
	@$(CMAKE) -E echo "make test                 Build and run local correctness tests"
	@$(CMAKE) -E echo "make validate             Validate manifests without optional third-party binaries"
	@$(CMAKE) -E echo "make validate-paper-binaries Require the two binaries used by three formal timing entries"
	@$(CMAKE) -E echo "make validate-all-binaries Validate including restored Basic+/SCIP-Jack"
	@$(CMAKE) -E echo "Variables: BUILD_DIR=$(BUILD_DIR) BUILD_TYPE=$(BUILD_TYPE) JOBS=$(JOBS) PYTHON=$(PYTHON)"

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)"

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel "$(JOBS)"

release: build
	$(CMAKE) --build "$(BUILD_DIR)" --target test --parallel "$(JOBS)"

paper-binaries: configure
	$(CMAKE) --build "$(BUILD_DIR)" --target abhss pruneddp --parallel "$(JOBS)"

tools: configure
	$(CMAKE) --build "$(BUILD_DIR)" --target prepare_gpu4gst build_imdb_graph audit_query_feasibility --parallel "$(JOBS)"

test: build
	ctest --test-dir "$(BUILD_DIR)" --build-config "$(BUILD_TYPE)" --output-on-failure

validate:
	$(PYTHON) tools/experiments/validate_environment.py

validate-paper-binaries: paper-binaries
	$(PYTHON) tools/experiments/validate_environment.py --require-performance-binaries

validate-all-binaries: build
	$(PYTHON) tools/experiments/validate_environment.py --require-binaries

clean:
	$(CMAKE) -E remove_directory "$(BUILD_DIR)"
