BUILD_DIR ?= build
BUILD_TYPE ?= Release
JOBS ?= 4
CMAKE ?= cmake

.PHONY: all release

all: release

release:
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)"
	$(CMAKE) --build "$(BUILD_DIR)" --parallel "$(JOBS)"
