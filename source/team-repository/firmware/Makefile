# Standalone XC32 build; no generated IDE metadata or absolute user paths.
XC32_CC ?= xc32-gcc
XC32_BIN2HEX ?= xc32-bin2hex
DEVICE := 32MX170F256B
DFP_ROOT ?= $(lastword $(sort $(wildcard $(HOME)/.mchp_packs/Microchip/PIC32MX_DFP/*)))
BUILD_DIR := build
IMAGE := $(BUILD_DIR)/panic_scaffold.elf
HEAP_SIZE ?= 1024
STACK_SIZE ?= 2048

ifeq ($(strip $(DFP_ROOT)),)
$(error Set DFP_ROOT to the PIC32MX_DFP folder containing xc32/)
endif

include sources.mk

INCLUDE_DIRS := config app/include services/include drivers/include \
                drivers/reused vendor/es_framework/FrameworkHeaders
CPPFLAGS := $(addprefix -I,$(INCLUDE_DIRS))
TARGET_FLAGS := -mprocessor=$(DEVICE) -mdfp="$(DFP_ROOT)"
CFLAGS := $(TARGET_FLAGS) -std=gnu99 -O0 -g -Wall -Wextra \
          -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS := $(TARGET_FLAGS) -Wl,--gc-sections,-Map=$(BUILD_DIR)/panic_scaffold.map \
           -Wl,--defsym=_min_heap_size=$(HEAP_SIZE),--defsym=_min_stack_size=$(STACK_SIZE)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.c=.o))

.PHONY: all check clean
all: $(IMAGE)

$(IMAGE): $(OBJECTS)
	$(XC32_CC) $(LDFLAGS) $(OBJECTS) -o "$@"
	$(XC32_BIN2HEX) "$@"

$(BUILD_DIR)/%.o: %.c
	@mkdir -p "$(@D)"
	$(XC32_CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

check: all
	python3 tools/check_reuse.py

clean:
	rm -rf "$(BUILD_DIR)"

-include $(OBJECTS:.o=.d)
