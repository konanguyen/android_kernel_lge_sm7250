# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/include/uapi
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sensor_module/cam_sensor_io
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sensor_module/cam_res_mgr
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sensor_module/cam_sensor_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sensor_module/cam_cci
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core/

obj-$(CONFIG_SPECTRA_CAMERA) += cam_ois_dev.o cam_ois_core.o cam_ois_soc.o
obj-$(CONFIG_MACH_LITO_CAYMANLM) += lgit_onsemi/lgit_s5kgw1_onsemi_ois.o
obj-$(CONFIG_MACH_LITO_ACELM) += lgit_onsemi/lgit_s5kgw1_onsemi_ois.o
obj-$(CONFIG_MACH_LAGOON_ACEXLM) += lgit_onsemi/lgit_s5kgw1_onsemi_ois.o
obj-$(CONFIG_MACH_LITO_WINGLM) += lgit_onsemi/lgit_s5kgw1_onsemi_ois.o
obj-$(CONFIG_MACH_LAGOON_SMASHJLM) += sunny_onsemi/sunny_imx363_onsemi_ois.o sunny_onsemi/PhoneUpdate.o sunny_onsemi/OisCmd.o
