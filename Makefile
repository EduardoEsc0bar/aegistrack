SHELL := /bin/zsh

COMPOSE ?= docker compose

.PHONY: cmake-configure build ctest up down logs seed test lint format api-test web-test

cmake-configure:
	cmake -S . -B build

build:
	cmake --build build -j

ctest:
	ctest --test-dir build --output-on-failure

up:
	$(COMPOSE) --profile core up --build

down:
	$(COMPOSE) down --remove-orphans

logs:
	$(COMPOSE) logs -f

seed:
	$(COMPOSE) run --rm api python -m app.scripts.seed_db

api-test:
	cd apps/api && pytest

web-test:
	cd apps/web && npm test

test: api-test
	$(MAKE) ctest

lint:
	cd apps/api && ruff check .
	cd apps/web && npm run lint

format:
	cd apps/api && ruff format .
	cd apps/web && npm run format
