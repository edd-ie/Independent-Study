# ------------------------------------------------------------

# Project configuration

# ------------------------------------------------------------

TARGET      := main
SRC_DIRS    := . IOuring mpi openmp util
BUILD_DIR   := build
DEBUG_DIR   := $(BUILD_DIR)/debug
RELEASE_DIR := $(BUILD_DIR)/release
TEST_FILES := $(wildcard resource/test_data/test_file_*.txt)



# ------------------------------------------------------------
# Tools & Libraries
# ------------------------------------------------------------

CXX      := mpicxx
FORMAT   := clang-format

# Add -luring if you are using io_uring specifically
LIBS     := -luring


# ------------------------------------------------------------
# Colors
# ------------------------------------------------------------
GREEN  := \033[1;32m
BLUE   := \033[1;34m
YELLOW := \033[1;33m
RED    := \033[1;31m
RESET  := \033[0m

# ------------------------------------------------------------
# Build mode selection
# ------------------------------------------------------------
MODE ?= release


# Base flags for all modes
COMMON_FLAGS := -std=c++23 -I. -Wall -Wextra -Wpedantic

ifeq ($(MODE),debug)
    OUT_DIR   := $(DEBUG_DIR)
    CXXFLAGS  := $(COMMON_FLAGS) -O0 -g -DDEBUG
    LDFLAGS   :=
    BUILD_MSG := "Building in DEBUG mode"
else
    OUT_DIR   := $(RELEASE_DIR)
    CXXFLAGS  := $(COMMON_FLAGS) -fopenmp -O3 -march=native
    LDFLAGS   := -fopenmp
    BUILD_MSG := "Building in RELEASE mode"
endif


# ------------------------------------------------------------
# Source discovery
# ------------------------------------------------------------
SRCS := $(shell find $(SRC_DIRS) -maxdepth 1 -name "*.cpp" 2>/dev/null)
OBJS := $(SRCS:%.cpp=$(OUT_DIR)/%.o)
DEPS := $(OBJS:.o=.d)


# ------------------------------------------------------------
# Primary Rules
# ------------------------------------------------------------
all: $(OUT_DIR)/$(TARGET)
	@echo -e "$(GREEN)Build Complete: $(OUT_DIR)/$(TARGET)$(RESET)"


# High-level build message - only shows if something actually needs to be built
$(OUT_DIR)/$(TARGET): $(OBJS)
	@echo -e "$(BLUE)$(BUILD_MSG)$(RESET)"
	@echo -e "$(GREEN)[Link]$(RESET) $@"
	@$(CXX) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)



# Compile with automatic dependency generation
$(OUT_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)[Compile]$(RESET) $<"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)



# ------------------------------------------------------------
# Utility Targets
# ------------------------------------------------------------
# Format all source and header files

format:
	@echo -e "$(GREEN)[Format]$(RESET) Applying clang-format"
	@find $(SRC_DIRS) -name "*.cpp" -o -name "*.hpp" | xargs $(FORMAT) -i

# Run MPI test
n ?= 4
test: $(OUT_DIR)/$(TARGET)	@echo -e "$(GREEN)[Test]$(RESET) Running with $(n) processes..."
	@mpirun -np $(n) ./$(OUT_DIR)/$(TARGET)



# Specialized test for your readv-io_uring/cat logic
read1: $(OUT_DIR)/$(TARGET)
	@echo -e "$(GREEN)[Standard Run]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) ./resource/test_input.txt


read1Q: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Quiet Run (Raw I/O)]$(RESET)"
	@time ./$(OUT_DIR)/$(TARGET) -q ./resource/test_input.txt


readAll: $(OUT_DIR)/$(TARGET)
	@echo -e "$(GREEN)[Standard Run - All Files]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) $(TEST_FILES)


readAllQ: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Quiet Run (Raw I/O Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) -q $(TEST_FILES)



# Check for memory leaks (Tests the unique_ptr cleanup for multiple files)
memcheck: $(OUT_DIR)/$(TARGET)
	@echo -e "$(BLUE)[Memcheck]$(RESET) Verifying RAII cleanup for $(words $(TEST_FILES)) files..."
	@valgrind --leak-check=full --show-leak-kinds=all -s ./$(OUT_DIR)/$(TARGET) -q $(TEST_FILES)

clean:
	@echo -e "$(RED)[Clean]$(RESET) Removing $(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)


.PHONY: all clean format test readv memcheck
