# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/display/config/konadisp.conf
endif

ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/konadispconf.h
endif

ifeq ($(CONFIG_ARCH_LITO), y)
include $(srctree)/techpack/display/config/saipdisp.conf
endif

ifeq ($(CONFIG_ARCH_LITO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/saipdispconf.h
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
include $(srctree)/techpack/display/config/bengaldisp.conf
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/bengaldispconf.h
endif

# For LGE Device
ifeq ($(CONFIG_MACH_LITO_CAYMANLM), y)
include $(srctree)/techpack/display/config/caymanlm_saipdisp.conf
endif

ifeq ($(CONFIG_MACH_LITO_CAYMANLM), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/caymanlm_saipdispconf.h
endif

ifeq ($(CONFIG_MACH_LITO_WINGLM), y)
include $(srctree)/techpack/display/config/winglm_saipdisp.conf
endif

ifeq ($(CONFIG_MACH_LITO_WINGLM), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/winglm_saipdispconf.h
endif

ifeq ($(CONFIG_MACH_LITO_VICTORIALM), y)
include $(srctree)/techpack/display/config/victorialm_saipdisp.conf
endif

ifeq ($(CONFIG_MACH_LITO_VICTORIALM), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/victorialm_saipdispconf.h
endif

ifeq ($(CONFIG_MACH_LITO_ACELM), y)
include $(srctree)/techpack/display/config/acelm_saipdisp.conf
endif

ifeq ($(CONFIG_MACH_LITO_ACELM), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/acelm_saipdispconf.h
endif

ifeq ($(CONFIG_MACH_LAGOON_ACEXLM), y)
include $(srctree)/techpack/display/config/acexlm_saipdisp.conf
endif

ifeq ($(CONFIG_MACH_LAGOON_ACEXLM), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/acexlm_saipdispconf.h
endif

obj-$(CONFIG_DRM_MSM) += msm/
obj-$(CONFIG_MSM_SDE_ROTATOR) += rotator/
obj-$(CONFIG_QCOM_MDSS_PLL) += pll/
