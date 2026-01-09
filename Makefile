
BEAR ?= bear
CC ?= clang-22
CXX ?= clang++-22
LLVM_CONFIG ?= llvm-config-22
CLANG_FORMAT ?= clang-format-22
GOBLINT ?= goblint

SRC_DIR ?= src
INC_DIR ?= headers
EXAMPLES_DIR ?= examples
BUILD_DIR ?= build

WITNESS_INJECT ?= witness_inject
COMPILE_COMMANDS_JSON ?= compile_commands.json

CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -Wno-unused-parameter -I$(INC_DIR)
CXXFLAGS += $(shell $(LLVM_CONFIG) --cxxflags) -MMD -MP

LDFLAGS ?= -lclang-cpp
LDFLAGS += $(shell $(LLVM_CONFIG) --ldflags --system-libs --libs all)

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
EXAMPLES := $(wildcard $(EXAMPLES_DIR)/*.c)

OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
EXAMPLES_DONE := $(patsubst $(EXAMPLES_DIR)/%.c,$(BUILD_DIR)/examples/%/done,$(EXAMPLES))

ARTIFACTS := $(patsubst $(EXAMPLES_DIR)/%.c,$(BUILD_DIR)/examples/%/injected.c,$(EXAMPLES))
ARTIFACTS += $(patsubst $(EXAMPLES_DIR)/%.c,$(BUILD_DIR)/examples/%/original.c,$(EXAMPLES))
ARTIFACTS += $(patsubst $(EXAMPLES_DIR)/%.c,$(BUILD_DIR)/examples/%/witness.yml,$(EXAMPLES))
ARTIFACTS += $(patsubst $(EXAMPLES_DIR)/%.c,$(BUILD_DIR)/examples/%/test,$(EXAMPLES))

all: $(WITNESS_INJECT)

$(WITNESS_INJECT): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/examples/%/original.c: $(EXAMPLES_DIR)/%.c
	mkdir -p "$(shell dirname $@)"
	cp $< $@

.SECONDEXPANSION:
$(BUILD_DIR)/examples/%/witness.yml: EXAMPLE_DIR_NAME=$(patsubst $(BUILD_DIR)/examples/%/witness.yml,$(BUILD_DIR)/examples/%,$@)
$(BUILD_DIR)/examples/%/witness.yml: $(BUILD_DIR)/examples/%/original.c
	cp $< $(EXAMPLE_DIR_NAME)/injected.c
	cd $(EXAMPLE_DIR_NAME) && \
		$(GOBLINT) --enable witness.yaml.enabled injected.c
	rm $(EXAMPLE_DIR_NAME)/injected.c

$(BUILD_DIR)/examples/%/injected.c: EXAMPLE_DIR_NAME=$(shell dirname $@)
$(BUILD_DIR)/examples/%/injected.c: $(BUILD_DIR)/examples/%/witness.yml $(WITNESS_INJECT)
	cp $(EXAMPLE_DIR_NAME)/original.c $(EXAMPLE_DIR_NAME)/injected.c
	$(PWD)/$(WITNESS_INJECT) -std=c17 --witness-yaml $< --assert-fn __WITNESS_ASSERT "$@"
	$(CLANG_FORMAT) -i "$@"

$(BUILD_DIR)/examples/%/test: $(BUILD_DIR)/examples/%/injected.c
	$(CC) -std=c17 -include $(EXAMPLES_DIR)/assert.h $< -o "$@"

$(BUILD_DIR)/examples/%/done: $(BUILD_DIR)/examples/%/test
	$<
	touch "$@"

-include $(OBJS:.o=.d)

test: $(EXAMPLES_DONE)

compdb:
	$(BEAR) --output "$(COMPILE_COMMANDS_JSON)" -- $(MAKE) -B

clean:
	rm -rf $(BUILD_DIR) $(WITNESS_INJECT) $(COMPILE_COMMANDS_JSON)

.ARTIFACTS: $(ARTIFACTS)

.PHONY: all test clean compdb .ARTIFACTS
