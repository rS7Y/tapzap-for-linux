# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
.DEFAULT_GOAL := help
PYTHON ?= python3
VARIANT ?= x11

.PHONY: help check test verify syntax build format check-format scan-secrets

help:
	@printf '%s\n' \
	  'make check                   Run source, tests, syntax, format and secret checks' \
	  'make test                    Run tests without accessing a display' \
	  'make build VARIANT=x11       Build a native Linux variant and run self-tests' \
	  '                             Variants: x11, gnome-wayland, plasma-wayland,' \
	  '                                       sway, omarchy-hyprland' \
	  'make format                  Format project C/C++ and Python; refresh hashes' \
	  'make check-format            Check formatting without modifying files' \
	  'make verify                  Check source hashes, licenses and private paths' \
	  'make scan-secrets            Scan files and complete local Git history'

check: verify test syntax check-format scan-secrets

test:
	$(PYTHON) -m unittest discover -s tests -v

verify:
	$(PYTHON) scripts/verify-source-release.py

syntax:
	$(PYTHON) -m compileall -q src tests scripts
	git ls-files -z '*.sh' | xargs -0 -n1 bash -n

build:
	bash scripts/ci-build-variant.sh "$(VARIANT)"

format:
	git ls-files -z '*.c' '*.cpp' '*.h' '*.hpp' | xargs -0 clang-format -i
	ruff format src tests scripts
	$(PYTHON) scripts/update-source-manifest.py

check-format:
	git ls-files -z '*.c' '*.cpp' '*.h' '*.hpp' | xargs -0 clang-format --dry-run --Werror
	ruff format --check src tests scripts

scan-secrets:
	gitleaks dir --no-banner --redact --ignore-gitleaks-allow --gitleaks-ignore-path /dev/null .
	gitleaks git --log-opts=--all --no-banner --redact --ignore-gitleaks-allow --gitleaks-ignore-path /dev/null .
