CURRENT_ASSIGNMENT = k1

TARGET ?= $(CURRENT_ASSIGNMENT)

#------------- Cross Compilation Setup -------------#

XDIR ?= /u/cs452/public/xdev
XBINDIR = $(XDIR)/bin
XLIBDIR1 = $(XDIR)/lib/gcc/arm-none-eabi/*
XLIBDIR2 = $(XDIR)/arm-none-eabi/lib

CC = $(XBINDIR)/arm-none-eabi-gcc
CXX = $(XBINDIR)/arm-none-eabi-g++
AS = $(XBINDIR)/arm-none-eabi-as
LD = $(XBINDIR)/arm-none-eabi-ld

#------------- Flags -------------#

WARNING_FLAGS = -Wall -Wextra -Wconversion
COMMON_INCLUDES = -I. -I./include
COMMON_FLAGS = -fPIC -mcpu=arm920t -msoft-float -MP -MMD -MT $@ $(WARNING_FLAGS) $(COMMON_INCLUDES)

ifdef ENABLE_CACHES
    COMMON_FLAGS += -DENABLE_CACHES
endif

ifdef DEBUG
    COMMON_FLAGS += -Og -g
else
    COMMON_FLAGS += -O3 -g -Werror -DRELEASE_MODE
endif

CCFLAGS = $(COMMON_FLAGS) -std=c11
CXXFLAGS = $(COMMON_FLAGS) -std=c++17 -fno-rtti -fno-exceptions -fno-unwind-tables

LDFLAGS =                   \
    -static                 \
    -nmagic                 \
    -Tts7200_redboot.ld     \
    --orphan-handling=place \
    -L $(XLIBDIR1)          \
    -L $(XLIBDIR2)
LIBRARIES = -lstdc++ -lc -lgcc # order matters!

#------------- [*src|*include|build|bin] dirs -------------#

SRC_DIR = src
BUILD_DIR = build

ALL_USER_SRCS_GLOB = $(SRC_DIR)/assignments/*/**
USER_SRC_DIR = $(SRC_DIR)/assignments/$(TARGET)

SRCS = $(shell find $(SRC_DIR) \
                            \( -name '*.c' -or -name '*.cc' -or -name '*.s' \) \
                            ! -path "$(ALL_USER_SRCS_GLOB)" \
                            -or -path "$(USER_SRC_DIR)/**" )
OBJS = $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,\
        $(patsubst %.c,%.o,\
        $(patsubst %.cc,%.o,\
        $(patsubst %.s,%.o,\
            $(SRCS)))))
DEPS = $(shell find $(BUILD_DIR) -name '*.d' 2> /dev/null)

#------------- Targets -------------#

.PHONY: build
build: $(TARGET).elf

-include $(DEPS)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf *.elf

$(TARGET).elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBRARIES) -Map $(BUILD_DIR)/$@.map
	rm -rf latest.elf && ln -s $@ latest.elf

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CCFLAGS) -c -S $< -o $@.s
	$(AS) $(ASFLAGS) -o $@ $@.s

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c -S $< -o $@.s
	$(AS) $(ASFLAGS) -o $@ $@.s

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -g -o $@ $<

################################################################################

.PHONY: unit_tests
unit_tests: $(BUILD_DIR)/test

$(BUILD_DIR)/test: test/test.cc
	@mkdir -p $(BUILD_DIR)/test
	g++ -std=c++17 -fno-rtti -fno-exceptions \
		$(COMMON_INCLUDES) $(WARNING_FLAGS) \
		-MMD -MF $(BUILD_DIR)/test/test.d \
		-Werror \
		$< -o $(BUILD_DIR)/test/test
	$(BUILD_DIR)/test/test

k1.pdf: docs/k1/kernel.md docs/k1/output.md
	pandoc --from markdown --to pdf $^ > k1.pdf
