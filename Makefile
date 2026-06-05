.PHONY: build build-debug test test-seq test-all tidy tidy-ci clean bootstrap rebuild docker docker-build docker-shell

BUILD_DIR := build
CMAKE_FLAGS := -G Ninja -DCMAKE_BUILD_TYPE=Release
JOBS := $(shell nproc 2>/dev/null || echo 4)

build:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS) && \
	ninja -C $(BUILD_DIR) -j$(JOBS)

build-debug:
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Debug && \
	ninja -C $(BUILD_DIR) -j$(JOBS)

test:
	rm -rf ~/.cache/fluxscript && bash tests/integration/run_tests.sh -P $(shell nproc 2>/dev/null || echo 4)

test-seq:
	rm -rf ~/.cache/fluxscript && bash tests/integration/run_tests.sh

test-all:
	rm -rf ~/.cache/fluxscript && \
		cd $(BUILD_DIR) && ctest --output-on-failure -j$(JOBS)

clean:
	ninja -C $(BUILD_DIR) clean 2>/dev/null; true

rebuild: clean build

# ── bootstrap: build the self‑hosted compiler ──────────────────────────────
bootstrap: build
	rm -rf ~/.cache/fluxscript
	build/flux --cache=0 --emit=llvm src/fluxc/main.flux > /tmp/flux_stage2.ll
	/usr/lib/llvm-21/bin/opt -passes=verify /tmp/flux_stage2.ll -o /tmp/flux_stage2_verify.bc
	/usr/lib/llvm-21/bin/opt -passes=globaldce /tmp/flux_stage2_verify.bc -o /tmp/flux_stage2_gced.bc
	/usr/lib/llvm-21/bin/llc -O2 -relocation-model=pic /tmp/flux_stage2_gced.bc -o /tmp/flux_stage2.s
	g++ -O2 /tmp/flux_stage2.s -L$(BUILD_DIR) -lFluxScript -Wl,-rpath,$(BUILD_DIR) -o /tmp/fluxc2
	FLUX_INPUT=src/fluxc/main.flux /tmp/fluxc2 > /tmp/flux_stage3.ll
	/usr/lib/llvm-21/bin/opt -passes=verify /tmp/flux_stage3.ll -o /tmp/flux_stage3_verify.bc
	/usr/lib/llvm-21/bin/opt -passes=globaldce /tmp/flux_stage3_verify.bc -o /tmp/flux_stage3_gced.bc
	/usr/lib/llvm-21/bin/llc -O2 -relocation-model=pic /tmp/flux_stage3_gced.bc -o /tmp/flux_stage3.s
	g++ -O2 /tmp/flux_stage3.s -L$(BUILD_DIR) -lFluxScript -Wl,-rpath,$(BUILD_DIR) -o $(BUILD_DIR)/fluxc_bootstrap
	@echo "Bootstrap complete: $(BUILD_DIR)/fluxc_bootstrap"

# ── interactive: rebuild then open a REPL ──────────────────────────────────
run: build
	ninja -C $(BUILD_DIR) flux-run && $(BUILD_DIR)/flux-run

# ── clang-tidy: static analysis ────────────────────────────────────────────
CLANG_TIDY := $(shell command -v clang-tidy-21 2>/dev/null || command -v clang-tidy 2>/dev/null)
TIDY_SOURCES := $(shell find src/compiler/ include/ -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | sort)

tidy:
ifdef CLANG_TIDY
	@echo "Running clang-tidy on all source files..."
	$(CLANG_TIDY) -p $(BUILD_DIR) $(if $(FILE),$(FILE),$(TIDY_SOURCES)) 2>&1 | tee /tmp/clang-tidy.log
	@echo "Results written to /tmp/clang-tidy.log"
else
	@echo "clang-tidy not found. Install clang-tidy-21: sudo apt install clang-tidy-21"
endif

tidy-ci:
ifdef CLANG_TIDY
	$(CLANG_TIDY) -p $(BUILD_DIR) $(TIDY_SOURCES) --warnings-as-errors='*' 2>&1
else
	@echo "clang-tidy not found, skipping"
endif

# ── Docker dev environment ────────────────────────────────────────────────
DOCKER_IMAGE := fluxscript-dev

docker-build:
	docker build -t $(DOCKER_IMAGE) -f Dockerfile.dev .

docker-shell: docker-build
	docker run -it --rm \
		-v $(PWD):/workspace \
		-v $(HOME)/.cache/ccache:/root/.cache/ccache \
		$(DOCKER_IMAGE)
