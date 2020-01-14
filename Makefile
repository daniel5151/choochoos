XBINDIR = /u/cs452/public/xdev/bin
XLIBDIR1 = /u/cs452/public/xdev/lib/gcc/arm-none-eabi/9.2.0
XLIBDIR2 = /u/cs452/public/xdev/arm-none-eabi/lib

CC = $(XBINDIR)/arm-none-eabi-gcc
CXX = $(XBINDIR)/arm-none-eabi-g++
AS = $(XBINDIR)/arm-none-eabi-as
LD = $(XBINDIR)/arm-none-eabi-ld

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

BIN_NAME = choochoos.elf

SRCS = $(shell find $(SRC_DIR) -name '*.c' -or -name '*.cc' -or -name '*.s')
OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(addsuffix .o,$(basename $(SRCS)))))
DEPS = $(OBJS:.o=.d)

INCLUDES = -I. -I./include
CFLAGS = -fPIC -mcpu=arm920t -msoft-float -Wall -Wextra
CCFLAGS = -std=c11
CXXFLAGS = -std=c++17 -fno-rtti -fno-exceptions

EXTRA_FLAGS = -O1 -g

LDFLAGS = -static -nmagic -Tts7200_redboot.ld --orphan-handling=place -Map $(BUILD_DIR)/$(BIN_NAME).map -L $(XLIBDIR1) -L $(XLIBDIR2)
LIBRARIES = -lstdc++ -lc -lgcc

.PHONY: dirs all
all: $(BIN_DIR)/$(BIN_NAME)

release: EXTRA_FLAGS = -03
release: all

.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(BIN_DIR)

.PHONY: dirs
dirs: $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/$(BIN_NAME): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBRARIES)

-include $(DEPS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CCFLAGS) $(INCLUDES) $(EXTRA_FLAGS) -MP -MMD -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc | $(BUILD_DIR)
	$(CXX) $(CFLAGS) $(CXXFLAGS) $(INCLUDES) $(EXTRA_FLAGS) -MP -MMD -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -o $@ $< -MP -MMD

