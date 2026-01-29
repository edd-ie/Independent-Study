# ------------------------------------------------------------
# Project configuration
# ------------------------------------------------------------
TARGET	  := main
SRC_DIRS    := . Comm
BUILD_DIR   := build
DEBUG_DIR   := $(BUILD_DIR)/debug
RELEASE_DIR := $(BUILD_DIR)/release

# ------------------------------------------------------------
# Tools
# ------------------------------------------------------------
CXX      := mpicxx
FORMAT   := clang-format
FORMAT_FLAGS := -i

# ------------------------------------------------------------
# Colors
# ------------------------------------------------------------
GREEN  := \033[1;32m
BLUE   := \033[1;34m
YELLOW := \033[1;33m
RESET  := \033[0m

# ------------------------------------------------------------
# Build mode selection
# ------------------------------------------------------------
MODE ?= release

ifeq ($(MODE),debug)
    OUT_DIR   := $(DEBUG_DIR)
    CXXFLAGS  := -std=c++23 -I. -O0 -g -Wall -Wextra -Wpedantic
    BUILD_MSG := "Building in DEBUG mode"
else
    OUT_DIR   := $(RELEASE_DIR)
    CXXFLAGS  := -std=c++23 -I. -O3 -march=native
    BUILD_MSG := "Building in RELEASE mode"
endif

# ------------------------------------------------------------
# Source discovery
# ------------------------------------------------------------
SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
OBJS := $(patsubst %.cpp,$(OUT_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ------------------------------------------------------------ 
# Default rule 
# ------------------------------------------------------------ 
all: msg $(OUT_DIR)/$(TARGET) 

msg: 
	@echo -e "$(BLUE)$(BUILD_MSG)$(RESET)"

# ------------------------------------------------------------
# Link
# ------------------------------------------------------------
$(OUT_DIR)/$(TARGET): $(OBJS)
	@echo -e "$(GREEN)[Link]$(RESET) $@"
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $@

# ------------------------------------------------------------
# Compile with automatic dependency generation
# ------------------------------------------------------------
$(OUT_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)[Compile]$(RESET) $<"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

# ------------------------------------------------------------
# Format target (clang-format)
# ------------------------------------------------------------
format:
	@echo -e "$(GREEN)[Format]$(RESET) Applying clang-format"
	@$(FORMAT) $(FORMAT_FLAGS) $(SRCS) $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.hpp))

# ------------------------------------------------------------
# Test target (runs MPI program)
# ------------------------------------------------------------
n ?= 4

test: $(OUT_DIR)/$(TARGET)
	@echo -e "$(GREEN)[Test]$(RESET) mpirun -np $(n) ./$(OUT_DIR)/$(TARGET)"
	@mpirun -np $(n) ./$(OUT_DIR)/$(TARGET)

run:
	$(OUT_DIR)/$(TARGET)
	@mpirun -np $(n) ./$(OUT_DIR)/$(TARGET)

# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------
clean:
	@echo -e "$(YELLOW)[Clean]$(RESET)"
	@rm -rf $(BUILD_DIR)



.PHONY: all clean msg format test
