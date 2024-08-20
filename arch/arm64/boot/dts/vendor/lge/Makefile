ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
# Per Platform
ifeq ($(CONFIG_MACH_LITO_CAYMANLM),y)
DTS_PROJECT := lito-caymanlm
endif

# Per Device
ifeq ($(CONFIG_MACH_LITO_CAYMANLM_LAO_COM),y)
DTS_TARGET := lito-caymanlm_lao_com
endif

# Per Device
ifeq ($(CONFIG_MACH_LITO_CAYMANLM_DCM_JP),y)
DTS_TARGET := lito-caymanlm_dcm_jp
endif

# Per Device
ifeq ($(CONFIG_MACH_LITO_CAYMANLM_SB_JP),y)
DTS_TARGET := lito-caymanlm_sb_jp
endif

# Per Platform
ifeq ($(CONFIG_MACH_LITO_WINGLM),y)
DTS_PROJECT := lito-winglm
endif

# Per Device
ifeq ($(CONFIG_MACH_LITO_WINGLM_LAO_COM),y)
DTS_TARGET := lito-winglm_lao_com
endif

# Per Platform
ifeq ($(CONFIG_MACH_LITO_VICTORIALM),y)
DTS_PROJECT := lito-victorialm
endif

# Per Device
ifeq ($(CONFIG_MACH_LITO_VICTORIALM_LAO_COM),y)
DTS_TARGET := lito-victorialm_lao_com
endif

# Per Platform
ifeq ($(CONFIG_MACH_LITO_ACELM),y)
DTS_PROJECT := lito-acelm
endif

# Per Device
ifeq ($(CONFIG_MACH_LITO_ACELM_LAO_COM),y)
DTS_TARGET := lito-acelm_lao_com
endif

# Per Platform
ifeq ($(CONFIG_MACH_LAGOON_ACEXLM),y)
DTS_PROJECT := lagoon-acexlm
endif

# Per Device
ifeq ($(CONFIG_MACH_LAGOON_ACEXLM_LAO_COM),y)
DTS_TARGET := lagoon-acexlm_lao_com
endif

# Per Platform
ifeq ($(CONFIG_MACH_LAGOON_SMASHJLM),y)
DTS_PROJECT := lagoon-smashjlm
endif

# Per Device
ifeq ($(CONFIG_MACH_LAGOON_SMASHJLM_COM1_JP),y)
DTS_TARGET := lagoon-smashjlm_com1_jp
endif

ALL_DTS := $(notdir $(wildcard echo $(srctree)/$(src)/$(DTS_PROJECT)/$(DTS_TARGET)/*.dts))
dtbo-y += $(patsubst %.dts,%.dtbo,$(ALL_DTS))

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtbo,$(DTB_NAMES))
else
DTB_LIST := $(dtbo-y)
endif

always  := $(dtbo-y)

$(obj)/%.dtbo: $(src)/$(DTS_PROJECT)/$(DTS_TARGET)/%.dts FORCE
	$(call if_changed_dep,dtc)
	$(call if_changed,dtbo_verify)

clean-files := *.dtb
endif
