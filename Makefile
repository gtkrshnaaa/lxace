.PHONY: all build test run clean

BUILD_DIR ?= build
export CXX ?= g++

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release .. && $(MAKE) --no-print-directory

test:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug .. && $(MAKE) --no-print-directory lxace_tests
	@./$(BUILD_DIR)/lxace_tests

run: build
	@./$(BUILD_DIR)/lxace

clean:
	@rm -rf $(BUILD_DIR) bin
