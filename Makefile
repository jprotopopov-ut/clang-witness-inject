
BEAR ?= bear
CC ?= clang-22
CXX ?= clang++-22
LLVM_CONFIG ?= llvm-config-22
CLANG_FORMAT ?= clang-format-22
GOBLINT ?= goblint

SRC_DIR ?= src
INC_DIR ?= headers
EXAMPLES_DIR ?= examples
FUZZ_HARNESS_DIR ?= fuzz_harness
BUILD_DIR ?= build
SKIP_INVALID_ASSERTIONS ?= $(PWD)/skip_invalid_assertions.py
RUN_TEST ?= $(PWD)/run_test.py

WITNESS_INJECT ?= witness_inject
COMPILE_COMMANDS_JSON ?= compile_commands.json

TEST_CC ?= $(CC)
TEST_CFLAGS ?= -std=c17

AFLPP_CC ?=
AFLPP_CFLAGS ?= $(TEST_CFLAGS)
AFLPP_LDFLAGS ?=

CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -Wno-unused-parameter -O2 -I$(INC_DIR)
CXXFLAGS += $(shell $(LLVM_CONFIG) --cxxflags) -MMD -MP

LDFLAGS ?= -lclang-cpp
LDFLAGS += $(shell $(LLVM_CONFIG) --ldflags --system-libs --libs all)

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

EXAMPLES := $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLES_NAMES := $(patsubst $(EXAMPLES_DIR)/%.c,%,$(EXAMPLES))
EXAMPLES_DONE := $(foreach x,$(EXAMPLES_NAMES),$(BUILD_DIR)/test/$(x)/$(x).done)
EXAMPLES_AFL := $(foreach x,$(EXAMPLES_NAMES),$(BUILD_DIR)/test/$(x)/$(x).afl)

all: $(WITNESS_INJECT)

$(foreach EXAMPLE,$(EXAMPLES_NAMES),\
	$(eval \
		$(BUILD_DIR)/test/$(EXAMPLE)/$(EXAMPLE).done: $(EXAMPLES_DIR)/$(EXAMPLE).c $(RUN_TEST) examples/generic.json $(WITNESS_INJECT); \
			mkdir -p $(BUILD_DIR)/test ; \
			$(RUN_TEST) --generic-conf examples/generic.json \
				--cc $(TEST_CC) \
				--workdir $(BUILD_DIR)/test/$(EXAMPLE) \
				--goblint $(GOBLINT) \
				--witness-inject $(shell realpath $(WITNESS_INJECT)) \
				--clang-format $(CLANG_FORMAT) \
				$(EXAMPLES_DIR)/$(EXAMPLE).c && \
			touch "$$@"))

ifneq ($(AFLPP_CC),)
$(foreach EXAMPLE,$(EXAMPLES_NAMES),\
	$(eval \
		$(BUILD_DIR)/test/$(EXAMPLE)/$(EXAMPLE).afl: $(BUILD_DIR)/test/$(EXAMPLE)/$(EXAMPLE).done; \
			$(AFLPP_CC) $(AFLPP_CFLAGS) -fPIC -DFUZZ_HARNESS_RAND_STDIN=1 \
				-DFUZZ_HARNESS_SANITIZER_HOOK=$(if $(AFL_USE_ASAN),1,0) \
				-include $(FUZZ_HARNESS_DIR)/fuzz_harness.h \
				-include  $(EXAMPLES_DIR)/assert.h \
				$(BUILD_DIR)/test/$(EXAMPLE)/$(EXAMPLE).injected.clean.c \
				$(FUZZ_HARNESS_DIR)/fuzz_harness.c \
				$(FUZZ_HARNESS_DIR)/fuzz_harness.s \
				-o "$$@" $(AFLPP_LDFLAGS) -Wl,-e,__fuzz_harness_start))

all-afl: $(EXAMPLES_AFL)
.PHONY: all-afl
endif

$(WITNESS_INJECT): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

test: $(EXAMPLES_DONE)

compdb:
	$(BEAR) --output "$(COMPILE_COMMANDS_JSON)" -- $(MAKE) -B

clean:
	rm -rf $(BUILD_DIR) $(WITNESS_INJECT) $(COMPILE_COMMANDS_JSON)

.PHONY: all test clean compdb
