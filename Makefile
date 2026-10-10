PYTHON ?= python3
JOBS   ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all game extras package clean distclean submodules help

all:
	$(PYTHON) scripts/build.py all -j$(JOBS)

game:
	$(PYTHON) scripts/build.py game -j$(JOBS)

extras:
	$(PYTHON) scripts/build.py extras

package:
	$(PYTHON) scripts/build.py package -j$(JOBS)

clean:
	$(PYTHON) scripts/build.py clean

distclean:
	$(PYTHON) scripts/build.py distclean

submodules:
	git submodule update --init --recursive

help:
	@echo "th06UtilityTool Build System"
	@echo "Usage:"
	@echo "  make              - Build all targets (th06util.exe, launcher.exe, patch, dll)"
	@echo "  make game         - Build th06util.exe only"
	@echo "  make extras       - Build companion tools (launcher, patch delta, dll)"
	@echo "  make package      - Build all targets and bundle into build/th06UtilityTool.zip"
	@echo "  make clean        - Remove game/tool build artifacts"
	@echo "  make distclean    - Remove all build files including FluidSynth library"
	@echo "  make submodules   - Initialize and clone git submodules"
