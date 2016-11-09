# Makefile for ESP8266 projects
#
# Thanks to:
# - zarya
# - Jeroen Domburg (Sprite_tm)
# - Christian Klippel (mamalala)
# - Tommie Gannert (tommie)
#
# Changelog:
# - 2014-10-06: Changed the variables to include the header file directory
# - 2014-10-06: Added global var for the Xtensa tool root
# - 2014-11-23: Updated for SDK 0.9.3
# - 2014-12-25: Replaced esptool by esptool.py

# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE	= build
FW_BASE		= firmware

# specify html directory patch for libesphttpd (This is Tinkerforge WIFI Extension 2.0 related)
LIBESPHTTPD_HTML = ../../software/web_interface/build

# specify libesphttpd path (This is Tinkerforge WIFI Extension 2.0 related)
LIBESPHTTPD_PATH = ../toolchain/libesphttpd

# base directory of the ESP8266 SDK package, absolute
SDK_BASE	?= ../toolchain/esp-open-sdk/sdk

WEBPAGE     ?= $(LIBESPHTTPD_PATH)/webpages.espfs

# esptool.py path and port
ESPTOOL		?= ../tests/tf_esptool.py # esptool.py
ESPPORT		?= /dev/ttyUSB0

# name for the target project
TARGET		= app

# which modules (subdirectories) of the project to include in compiling
MODULES		= src src/drivers
EXTRA_INCDIR    = include $(LIBESPHTTPD_PATH)/include

# libraries used in this project, mainly provided by the SDK
LIBS		= c gcc hal pp phy net80211 lwip wpa main esphttpd json mesh crypto

# compiler flags using during compilation of source files
CFLAGS		= -std=c99 -Os -g -O2 -Wpointer-arith -Wundef -Werror -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals  -D__ets__ -DICACHE_FLASH -DESP_MESH_SUPPORT

# linker flags used to generate the main object file
LDFLAGS		= -L$(LIBESPHTTPD_PATH)/ -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static

# linker script used for the above linker step
LD_SCRIPT	= wifi_v2.ld

# various paths from the SDK used in this project
SDK_LIBDIR	= lib
SDK_LDDIR	= ../../../software/src/ld
SDK_INCDIR	= include include/json

# we create two different files for uploading into the flash
# these are the names and options to generate them
FW_FILE_1_ADDR	= 0x00000
FW_FILE_2_ADDR	= 0x10000
FW_FILE_3_ADDR	= 0x100000
FW_FILE_4_ADDR	= 0x3FC000
FW_FILE_5_ADDR	= 0x3FE000

# select which tools to use as compiler, librarian and linker
CC		:= xtensa-lx106-elf-gcc
AR		:= xtensa-lx106-elf-ar
LD		:= xtensa-lx106-elf-gcc

####
#### no user configurable options below here
####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ		:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
TARGET_OUT	:= $(addprefix $(BUILD_BASE)/,$(TARGET).out)

LD_SCRIPT	:= $(addprefix -T$(SDK_BASE)/$(SDK_LDDIR)/,$(LD_SCRIPT))

INCDIR	:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

FW_FILE_1	:= $(addprefix $(FW_BASE)/,$(FW_FILE_1_ADDR).bin)
FW_FILE_2	:= $(addprefix $(FW_BASE)/,$(FW_FILE_2_ADDR).bin)
FW_FILE_3	:= $(addprefix $(FW_BASE)/,$(FW_FILE_3_ADDR).bin)
FW_FILE_4	:= $(addprefix $(FW_BASE)/,$(FW_FILE_4_ADDR).bin)
FW_FILE_5	:= $(addprefix $(FW_BASE)/,$(FW_FILE_5_ADDR).bin)
FW_FILE_Z   := $(addprefix $(FW_BASE)/,wifi_extension_v2_firmware.zbin)

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS) -c $$< -o $$@
endef

.PHONY: all checkdirs flash clean

all: updatelibmesh buildlibesphttpd checkdirs $(TARGET_OUT) $(FW_FILE_Z)

updatelibmesh:
	$(Q) rm -v $(SDK_BASE)/include/mesh.h
	$(Q) cp -v src/mesh/include/mesh.h $(SDK_BASE)/include/
	$(Q) rm -v $(SDK_BASE)/lib/liblwip.a
	$(Q) rm -v $(SDK_BASE)/lib/libmain.a
	$(Q) rm -v $(SDK_BASE)/lib/libmesh.a
	$(Q) rm -v $(SDK_BASE)/lib/libnet80211.a
	$(Q) rm -v $(SDK_BASE)/lib/libpp.a
	$(Q) rm -v $(SDK_BASE)/lib/libwpa.a

	$(Q) cp -v src/mesh/lib/liblwip.a $(SDK_BASE)/lib/
	$(Q) cp -v src/mesh/lib/libmain.a $(SDK_BASE)/lib/
	$(Q) cp -v src/mesh/lib/libmesh.a $(SDK_BASE)/lib/
	$(Q) cp -v src/mesh/lib/libnet80211.a $(SDK_BASE)/lib/
	$(Q) cp -v src/mesh/lib/libpp.a $(SDK_BASE)/lib/
	$(Q) cp -v src/mesh/lib/libwpa.a $(SDK_BASE)/lib/

buildlibesphttpd:
	make -C ./web_interface
	make -C $(LIBESPHTTPD_PATH) clean
	make -C $(LIBESPHTTPD_PATH) HTMLDIR=$(LIBESPHTTPD_HTML) SDK_BASE=../esp-open-sdk/sdk/ COMPRESS_W_YUI=yes

$(FW_BASE)/%.bin: $(TARGET_OUT) | $(FW_BASE)
	$(vecho) "FW $(FW_BASE)/"
	$(Q) $(ESPTOOL) elf2image -o $(FW_BASE)/ $(TARGET_OUT)

$(FW_FILE_3): $(WEBPAGE)
	$(Q) cp $(WEBPAGE) $(FW_FILE_3)

$(FW_FILE_4): $(SDK_BASE)/bin/esp_init_data_default.bin
	$(Q) cp $(SDK_BASE)/bin/esp_init_data_default.bin $(FW_FILE_4)

$(FW_FILE_5): $(SDK_BASE)/bin/blank.bin
	$(Q) cp $(SDK_BASE)/bin/blank.bin $(FW_FILE_5)

$(FW_BASE)/%.zbin: $(FW_FILE_1) $(FW_FILE_2) $(FW_FILE_3) $(FW_FILE_4) $(FW_FILE_5)
	$(vecho) "FZ $(FW_FILE_Z)"
	$(Q) zip -j $(FW_FILE_Z) $(FW_FILE_1) $(FW_FILE_2) $(FW_FILE_3) $(FW_FILE_4) $(FW_FILE_5)

$(TARGET_OUT): $(APP_AR)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) $(LD_SCRIPT) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR) $(FW_BASE)

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(FW_BASE):
	$(Q) mkdir -p $@

flash: $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL) --port $(ESPPORT) write_flash -fs 32m -fm qio -ff 40m $(FW_FILE_1_ADDR) $(FW_FILE_1) $(FW_FILE_2_ADDR) $(FW_FILE_2) $(FW_FILE_3_ADDR) $(FW_FILE_3)

clean:
	$(Q) rm -rf $(FW_BASE) $(BUILD_BASE)

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
