# ------------------------------------------------------------

# Project configuration

# ------------------------------------------------------------

TARGET      := main
SRC_DIRS    := . IOuring mpi openmp file_system Network
BUILD_DIR   := build
DEBUG_DIR   := $(BUILD_DIR)/debug
RELEASE_DIR := $(BUILD_DIR)/release
TEST_FILES := $(wildcard resource/test_data/test_file_*.txt)



# ------------------------------------------------------------
# Tools & Libraries
# ------------------------------------------------------------

CXX      := mpicxx
FORMAT   := clang-format

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


#files
input = ./resource/data.txt


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


readv: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Test run: (readv & writev Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 1 $(input) 1

readv2: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Test run: (readv & writev Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 1 $(input) 2

readvAll: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Test run: (readv & writev Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 1 $(input) 4

uring: $(OUT_DIR)/$(TARGET)
	@echo -e "$(GREEN)[Test run: (IO_Uring readv & writev Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 2 $(input) 1

uring2: $(OUT_DIR)/$(TARGET)
	@echo -e "$(GREEN)[Test run: (IO_Uring readv & writev Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 2 $(input) 2


uringAll: $(OUT_DIR)/$(TARGET)
	@echo -e "$(GREEN)[Test run: (IO_Uring readv & writev Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 2 $(input) 4


splice: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Test run: (IO_Uring splice Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 3 $(input) 1
splice2: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Test run: (IO_Uring splice Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 3 $(input) 2
spliceAll: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Test run: (IO_Uring splice Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 3 $(input) 4

tee: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Test run: (IO_Uring tee & splice Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 4 $(input) 1
tee2: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Test run: (IO_Uring tee & splice Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 4 $(input) 2
teeAll: $(OUT_DIR)/$(TARGET)
	@echo -e "\n$(GREEN)[Test run: (IO_Uring tee & splice Performance)]$(RESET)"
	@./$(OUT_DIR)/$(TARGET) 4 $(input) 4


clean:
	@echo -e "$(RED)[Clean]$(RESET) Removing $(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)


.PHONY: all clean format test readv memcheck
