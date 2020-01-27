CURRENT_ASSIGNMENT = k1

XBINDIR = /u/cs452/public/xdev/bin
XLIBDIR1 = /u/cs452/public/xdev/lib/gcc/arm-none-eabi/9.2.0
XLIBDIR2 = /u/cs452/public/xdev/arm-none-eabi/lib

CC = $(XBINDIR)/arm-none-eabi-gcc
CXX = $(XBINDIR)/arm-none-eabi-g++
AS = $(XBINDIR)/arm-none-eabi-as
LD = $(XBINDIR)/arm-none-eabi-ld

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include

# XXX: this ain't great, and aught to be rewritten
ifeq ($(MAKECMDGOALS),)
	USER_FOLDER = $(CURRENT_ASSIGNMENT)
else
	USER_FOLDER = $(MAKECMDGOALS)
endif
ALL_USER_SRCS_GLOB = $(SRC_DIR)/user/*/**
USER_SRC_DIR = $(SRC_DIR)/user/$(USER_FOLDER)

SRCS = $(shell find $(SRC_DIR) \
						 	\( -name '*.c' -or -name '*.cc' -or -name '*.s' \) \
							! -path "$(ALL_USER_SRCS_GLOB)" \
							-or -path "$(USER_SRC_DIR)/**" )
OBJS = $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,\
		$(patsubst %.c,%.o,\
		$(patsubst %.cc,%.o,\
		$(patsubst %.s,%.o,\
			$(SRCS)))))
DEPS = $(shell find $(BUILD_DIR) -name '*.d')

WARNING_FLAGS = -Wall -Wextra -Wconversion

COMMON_FLAGS = -fPIC -mcpu=arm920t -msoft-float -MP -MMD -MT $@ $(WARNING_FLAGS)

ifdef ENABLE_CACHES
	COMMON_FLAGS += -DENABLE_CACHES
endif

INCLUDES = -I. -I./include
CCFLAGS = $(COMMON_FLAGS) -std=c11
CXXFLAGS = $(COMMON_FLAGS) -std=c++17 -fno-rtti -fno-exceptions -fno-unwind-tables

OPTIMIZE_FLAGS = -O3 -g -Werror -DRELEASE_MODE

ifdef DEBUG
	OPTIMIZE_FLAGS = -Og -g
endif

LDFLAGS =                             \
	-static                           \
	-nmagic                           \
	-Tts7200_redboot.ld               \
	--orphan-handling=place           \
	-Map $(BUILD_DIR)/$(BIN_NAME).map \
	-L $(XLIBDIR1)                    \
	-L $(XLIBDIR2)
LIBRARIES = -lstdc++ -lc -lgcc

.PHONY: current_assignment
current_assignment: $(CURRENT_ASSIGNMENT).elf
	cp $(BIN_DIR)/$(CURRENT_ASSIGNMENT).elf $(CURRENT_ASSIGNMENT).elf

-include $(DEPS)

.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(BIN_DIR)
	$(RM) *.elf

.PHONY: dirs
dirs: $(BIN_DIR)

$(CURRENT_ASSIGNMENT).elf: $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(LD) $(LDFLAGS) $(OBJS) -o $(BIN_DIR)/$@ $(LIBRARIES)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CCFLAGS) $(INCLUDES) $(OPTIMIZE_FLAGS) -c -S $< -o $@.s
	$(AS) $(ASFLAGS) -o $@ $@.s

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) $(CXXFLAGS) $(INCLUDES) $(OPTIMIZE_FLAGS) -c -S $< -o $@.s
	$(AS) $(ASFLAGS) -o $@ $@.s

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -g -o $@ $<

.PHONY: unit_tests
unit_tests: $(BIN_DIR)/test

$(BIN_DIR)/test: test/test.cc
	@mkdir -p $(BIN_DIR) $(BUILD_DIR)/test
	g++ -std=c++17 -fno-rtti -fno-exceptions -I $(INCLUDES) \
		$(WARNING_FLAGS) -MMD -MF $(BUILD_DIR)/test/test.d -Werror $< -o $(BIN_DIR)/test && $(BIN_DIR)/test

k1.pdf: docs/k1/kernel.md docs/k1/output.md
	pandoc --from markdown --to pdf $^ > k1.pdf
