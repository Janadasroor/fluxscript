.PHONY: build build-debug test test-all clean bootstrap rebuild

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
