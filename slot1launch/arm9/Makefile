#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------


TWLM_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
include $(TWLM_ROOT)/universal/blocksds.mk

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories containing binary files embedded using bin2o
#---------------------------------------------------------------------------------
UNIVERSAL	:=	../../universal
TARGET		:=	slot1launch
BUILD		:=	build
SOURCES		:=	source $(UNIVERSAL)/source/common $(UNIVERSAL)/source/tonccpy
INCLUDES	:=	include $(UNIVERSAL)/include
DATA		:=	../data

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-mthumb -mthumb-interwork

CFLAGS	:=	-g -Wall -O2\
 		-march=armv5te -mtune=arm946e-s -fomit-frame-pointer\
		-ffast-math \
		$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM9 -DSTANDALONE -D_NO_MAIN_ALL
CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS	:=	-g $(ARCH) $(INCLUDE)
LDFLAGS	=	-specs=../$(UNIVERSAL)/arm9/ds_arm9_twlm.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project (order is important)
#---------------------------------------------------------------------------------
LIBS	:= -lnds9

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(LIBNDS)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

# export ARM9BIN	:=	$(TOPDIR)/$(TARGET).arm9
export ARM9ELF := $(CURDIR)/$(TARGET).elf
export DEPSDIR := $(CURDIR)/$(BUILD)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export ASSET_OBJECTS := $(addsuffix .o,$(BINFILES)) $(BMPFILES:.bmp=.o) $(PNGFILES:.png=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) *.elf *.nds* *.bin 
	@rm -fr $(DATA) *.elf *.nds* *.bin 

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
# $(ARM9BIN)	:	$(ARM9ELF) 
# 	@$(OBJCOPY) -O binary $< $@
# 	@echo built ... $(notdir $@)

$(ARM9ELF)	:	$(OFILES)
	@echo linking $(notdir $@)
	@$(LD) $(CPUFLAGS) $(LDFLAGS) $(BLOCKSDS_LDFLAGS) $(OFILES) $(LIBPATHS) $(BLOCKSDS_LIBPATHS) $(STORAGE_WRAPS) -Wl,--start-group $(if $(findstring g++,$(LD)),-lstdc++) $(LIBS) -lc -Wl,--end-group -o $@

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data 
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
