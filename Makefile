BIN_NAME = choochoos.elf

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

SRCS = $(shell find $(SRC_DIR) -name '*.c' -or -name '*.cc' -or -name '*.s')
OBJS = $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,\
		$(patsubst %.c,%.o,\
		$(patsubst %.cc,%.o,\
		$(patsubst %.s,%.o,\
			$(SRCS)))))
HEADERS = $(shell find ./include -name '*.h')
DEPS = $(OBJS:.o=.d)

WARNING_FLAGS = -Wall -Wextra -Wconversion

COMMON_FLAGS = -fPIC -mcpu=arm920t -msoft-float -MP -MMD $(WARNING_FLAGS)
INCLUDES = -I. -I./include

CCFLAGS = $(COMMON_FLAGS) -std=c11
CXXFLAGS = $(COMMON_FLAGS) -std=c++17 -fno-rtti -fno-exceptions

OPTIMIZE_FLAGS = -Og -g
RELEASE_FLAGS = -O3

LDFLAGS =                             \
	-static                           \
	-nmagic                           \
	-Tts7200_redboot.ld               \
	--orphan-handling=place           \
	-Map $(BUILD_DIR)/$(BIN_NAME).map \
	-L $(XLIBDIR1)                    \
	-L $(XLIBDIR2)
LIBRARIES = -lstdc++ -lc -lgcc

.PHONY: dirs all
all: $(BIN_DIR)/$(BIN_NAME)

release: OPTIMIZE_FLAGS = $(RELEASE_FLAGS)
release: all

.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(BIN_DIR)

.PHONY: dirs
dirs: $(BIN_DIR)

$(BIN_DIR)/$(BIN_NAME): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBRARIES)

-include $(DEPS)

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
	$(AS) $(ASFLAGS) -o $@ $<

.PHONY: test
test: test/test.cc $(HEADERS)
	@mkdir -p $(BIN_DIR)
	g++ -std=c++17 -fno-rtti -fno-exceptions -I $(INCLUDES) \
	 	-Wall -Wextra -Werror $< -o $(BIN_DIR)/test && $(BIN_DIR)/test
