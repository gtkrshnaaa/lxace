.PHONY: all build test run clean docker-build docker-run docker-test

BUILD_DIR ?= build
export CXX ?= g++

# 50% Host resource caps
HOST_CPUS ?= $(shell nproc 2>/dev/null || echo 2)
DOCKER_CPUS ?= $(shell echo $$(( ($(HOST_CPUS) + 1) / 2 )))
DOCKER_MEM ?= 3500M

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release .. && $(MAKE) --no-print-directory

test:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug .. && $(MAKE) --no-print-directory lxace_tests
	@sync
	@./$(BUILD_DIR)/lxace_tests

run: build
	@./$(BUILD_DIR)/lxace

docker-build:
	@docker build -t lxace-engine:latest .

docker-run: docker-build
	@docker run --rm -it \
		--name lxace-engine-runtime \
		--cpus $(DOCKER_CPUS) \
		--memory $(DOCKER_MEM) \
		-p 8080:8080 \
		-v $$(pwd):/workspace \
		lxace-engine:latest

docker-test: docker-build
	@docker run --rm \
		--name lxace-engine-test \
		--cpus $(DOCKER_CPUS) \
		--memory $(DOCKER_MEM) \
		--entrypoint /usr/local/bin/lxace_tests \
		lxace-engine:latest

clean:
	@rm -rf $(BUILD_DIR) bin
