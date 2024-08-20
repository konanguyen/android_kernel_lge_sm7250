#define pr_fmt(fmt)	"[Display][lge-backlight:%s:%d] " fmt, __func__, __LINE__

#include "msm_drv.h"
#include "sde_dbg.h"
#include "sde_kms.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include <linux/backlight.h>
#include <linux/leds.h>
#include "dsi_drm.h"
#include "dsi_display.h"
#include "sde_crtc.h"

#include "lge_brightness_def.h"
#include "lge_brightness.h"
#include "lge_dsi_panel.h"

#if IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
#include "../secondary/lge_backlight_secondary.h"
#endif

#if IS_ENABLED(CONFIG_LGE_DUAL_SCREEN)
#include "../ds3/lge_backlight_ds3.h"
extern bool is_ds_connected(void);
#endif

extern int lge_ddic_dsi_panel_tx_cmd_set(struct dsi_panel *panel,
				enum lge_ddic_dsi_cmd_set_type type);
extern char* get_payload_addr(struct dsi_panel *panel, enum lge_ddic_dsi_cmd_set_type type, int position);

char *blmap_names[] = {
	"lge,blmap",
	"lge,blmap-ve",
	"lge,blmap-ex",
	"lge,dynamic-blmap-brighter",
	"lge,blmap-hdr",
	"lge,blmap-vr",
	"lge,blmap-daylight",
	"lge,blmap-hdr-daylight",
	"lge,blmap-br-sync",
	"lge,blmap-hdr-br-sync"
};

static const int blmap_names_num = sizeof(blmap_names)/sizeof(blmap_names[0]);

inline static void lge_blmap_free_sub(struct lge_blmap *blmap)
{
	if (blmap->map) {
		kfree(blmap->map);
		blmap->map = NULL;
		blmap->size = 0;
	}
}

void lge_dsi_panel_blmap_free(struct dsi_panel *panel)
{
	int i;
	if (panel->lge.blmap_list) {
		for (i = 0; i < panel->lge.blmap_list_size; ++i) {
			lge_blmap_free_sub(&panel->lge.blmap_list[i]);
		}
		kfree(panel->lge.blmap_list);
		panel->lge.blmap_list_size = 0;
	}
}

static int lge_dsi_panel_parse_blmap_sub(struct device_node *of_node, const char* blmap_name, struct lge_blmap *blmap)
{
	struct property *data;
	int rc = 0;

	if (!blmap) {
		return -EINVAL;
	}

	blmap->size = 0;
	data = of_find_property(of_node, blmap_name, &blmap->size);
	if (!data) {
		pr_err("can't find %s\n", blmap_name);
		return -EINVAL;
	}
	blmap->size /= sizeof(u32);
	pr_info("%s blmap_size = %d\n", blmap_name, blmap->size);
	blmap->map = kzalloc(sizeof(u32) * blmap->size, GFP_KERNEL);
	if (!blmap->map) {
		blmap->size = 0;
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node, blmap_name, blmap->map,
					blmap->size);
	if (rc) {
		lge_blmap_free_sub(blmap);
	} else {
		pr_info("%s has been successfully parsed. \n", blmap_name);
	}

	return rc;
}


int lge_dsi_panel_parse_brightness(struct dsi_panel *panel,
	struct device_node *of_node)
{
	int rc = 0;

	rc = of_property_read_u32(of_node, "lge,default-brightness", &panel->lge.default_brightness);
	if (rc) {
		return rc;
	} else {
		pr_info("default brightness=%d \n", panel->lge.default_brightness);
	}

	return rc;
};

int lge_dsi_panel_parse_blmap(struct dsi_panel *panel,
	struct device_node *of_node)
{
	int rc = 0;
	int i;

	panel->lge.use_dynamic_brightness = of_property_read_bool(of_node, "lge,use-dynamic-brightness");
	pr_info("use dynamic brightness supported=%d\n", panel->lge.use_dynamic_brightness);

	panel->lge.use_br_sync_mode = of_property_read_bool(of_node, "lge,use-br-sync-mode");
	pr_info("use brightness sync mode supported=%d\n", panel->lge.use_br_sync_mode);

	panel->lge.blmap_list = kzalloc(sizeof(struct lge_blmap) * blmap_names_num, GFP_KERNEL);
	if (!panel->lge.blmap_list)
		return -ENOMEM;
	panel->lge.blmap_list_size = blmap_names_num;

	if (panel->lge.use_dynamic_brightness) {
		blmap_names[LGE_BLMAP_DEFAULT] = "lge,dynamic-blmap-normal";
	}

	for (i = 0; i < blmap_names_num; ++i) {
		lge_dsi_panel_parse_blmap_sub(of_node, blmap_names[i], &panel->lge.blmap_list[i]);
	}

	return rc;
};

char *lge_get_blmapname(enum lge_blmap_type type)
{
	if (type >= 0 && type < LGE_BLMAP_TYPE_MAX)
		return blmap_names[type];
	else
		return blmap_names[LGE_BLMAP_DEFAULT];
}

struct lge_blmap *lge_get_blmap(struct dsi_panel *panel, enum lge_blmap_type type)
{
	struct lge_blmap *blmap;

	if (type < 0 || type > panel->lge.blmap_list_size)
		type = LGE_BLMAP_DEFAULT;

	blmap = &panel->lge.blmap_list[type];
	if (!blmap || blmap->size == 0)
		blmap = &panel->lge.blmap_list[LGE_BLMAP_DEFAULT];

	return blmap;
}

int lge_update_backlight(struct dsi_panel *panel)
{
	struct dsi_display *display;
	struct sde_connector *c_conn;
	struct backlight_device *bd;
	int rc = 0;

	if (panel == NULL || panel->host == NULL)
		return -EINVAL;

	bd = panel->bl_config.raw_bd;
	display = container_of(panel->host, struct dsi_display, host);

	if (bd == NULL || display == NULL)
		return -EINVAL;

	if (panel->lge.allow_bl_update || !lge_dsi_panel_is_power_on_interactive(panel))
		return 0;

	c_conn = bl_get_data(bd);

	if (panel->bl_config.type != DSI_BACKLIGHT_WLED)
		mutex_lock(&bd->ops_lock);

	mutex_lock(&display->display_lock);
	if (panel->lge.bl_lvl_unset < 0) {
		rc = 0;
		goto exit;
	}


	rc = dsi_display_set_backlight(&c_conn->base, display, panel->lge.bl_lvl_unset);
	if (!rc) {
		pr_info("[%s] <--%pS unset=%d\n", panel->type, __builtin_return_address(0), panel->lge.bl_lvl_unset);
	}

	panel->lge.allow_bl_update = true;
	panel->lge.bl_lvl_unset = -1;

exit:
	mutex_unlock(&display->display_lock);
	if (panel->bl_config.type != DSI_BACKLIGHT_WLED)
		mutex_unlock(&bd->ops_lock);
	return rc;
}

int lge_backlight_device_update_status(struct backlight_device *bd)
{
	int brightness;
	struct lge_blmap *blmap;
	struct dsi_panel *panel;
	struct dsi_display *display;
	struct sde_connector *c_conn;
	int bl_lvl;
	struct drm_event event = {0,};
	enum lge_blmap_type bl_type;
	int rc = 0;

	brightness = bd->props.brightness;

	c_conn = bl_get_data(bd);
	display = (struct dsi_display *) c_conn->display;
	panel = display->panel;

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT)
	if (lge_get_bootreason_with_lcd_dimming() && !is_blank_called() && brightness > 0) {
		brightness = 1;
		pr_info("[%s] lcd dimming mode. set value = %d\n", panel->type, brightness);
	} else if (is_factory_cable()  && !is_blank_called() && brightness > 0) {
		brightness =  1;
		pr_info("[%s] Detect factory cable. set value = %d\n", panel->type, brightness);
	}
#endif

	if ((bd->props.power != FB_BLANK_UNBLANK) ||
			(bd->props.state & BL_CORE_FBBLANK) ||
			(bd->props.state & BL_CORE_SUSPENDED)) {
		if (!panel->lge.panel_dead && !c_conn->panel_dead) {
			brightness = 0;
			bd->props.brightness = 0;
		}
	}

	if ((panel->lge.lp_state == LGE_PANEL_LP2) || (panel->lge.lp_state == LGE_PANEL_LP1)) {
		bl_type = LGE_BLMAP_EX;
	} else if (panel->lge.vr_lp_mode) {
		bl_type = LGE_BLMAP_VR;
	} else if (panel->lge.hdr_mode) {
		bl_type = LGE_BLMAP_HDR;
		if (panel->lge.daylight_mode)
			bl_type = LGE_BLMAP_HDR_DAYLIGHT;
		else if (panel->lge.use_br_sync_mode && panel->lge.br_sync_mode)
			bl_type = LGE_BLMAP_HDR_BR_SYNC;
	} else if (panel->lge.use_color_manager && panel->lge.video_enhancement) {
		bl_type = LGE_BLMAP_VE;
	} else if (panel->lge.use_br_sync_mode && panel->lge.br_sync_mode) {
		bl_type = LGE_BLMAP_BR_SYNC;
	} else if (panel->lge.use_dynamic_brightness && panel->lge.brightness_table) {
		bl_type = LGE_BLMAP_BRIGHTER;
	} else if (panel->lge.daylight_mode){
		bl_type = LGE_BLMAP_DAYLIGHT;
	} else {
		bl_type = LGE_BLMAP_DEFAULT;
	}
	blmap = lge_get_blmap(panel, bl_type);

#if defined(CONFIG_LGE_DUAL_SCREEN)
	if (panel->lge.br_offset != 0 && is_ds_connected()) {
		brightness = br_to_offset_br_ds3(panel, brightness, blmap->size - 1, BR_MD);
	}
#endif

#if IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
	if (!primary_display->panel->lge.daylight_mode && panel->lge.br_offset &&
			lge_dsi_panel_is_power_on_interactive(secondary_display->panel))
		brightness = br_to_offset_br_sec(panel, brightness, blmap->size - 1);
#endif

	if (blmap) {
		// DUMMY panel doesn't have blmap, so this code is mandatory
		if (blmap->size == 0)	return -EINVAL;
		if (brightness >= blmap->size) {
			pr_warn("brightness=%d is bigger than blmap size (%d)\n", brightness, blmap->size);
			brightness = blmap->size-1;
		}

		bl_lvl = blmap->map[brightness];
	} else {
		if (brightness > display->panel->bl_config.bl_max_level)
			brightness = display->panel->bl_config.bl_max_level;

		/* map UI brightness into driver backlight level with rounding */
		bl_lvl = mult_frac(brightness, display->panel->bl_config.bl_max_level,
				display->panel->bl_config.brightness_max_level);

		if (!bl_lvl && brightness)
			bl_lvl = 1;

		if (display->panel->bl_config.bl_update ==
				BL_UPDATE_DELAY_UNTIL_FIRST_FRAME && !c_conn->allow_bl_update) {
			c_conn->unset_bl_level = bl_lvl;
			return 0;
		}
	}

	mutex_lock(&display->display_lock);
	if (panel->lge.allow_bl_update) { //TODO : Discuss condition state.
		panel->lge.bl_lvl_unset = -1;
		if (c_conn->ops.set_backlight) {
			/* skip notifying user space if bl is 0 */
			if (brightness != 0) {
				event.type = DRM_EVENT_SYS_BACKLIGHT;
				event.length = sizeof(u32);
				msm_mode_object_event_notify(&c_conn->base.base,
						c_conn->base.dev, &event, (u8 *)&brightness);
			}
			rc = c_conn->ops.set_backlight(&c_conn->base, c_conn->display, bl_lvl);
			c_conn->unset_bl_level = 0;
			pr_info("[%s] BR:%d BL:%d %s\n", panel->type, brightness, bl_lvl, lge_get_blmapname(bl_type));
		}
	} else if (!panel->lge.allow_bl_update) {
		panel->lge.bl_lvl_unset = bl_lvl;
		pr_info("[%s] brightness = %d, bl_lvl = %d -> differed (not allow) %s\n", panel->type, brightness, bl_lvl, lge_get_blmapname(bl_type));
	} else {
		panel->lge.bl_lvl_unset = bl_lvl;
		pr_info("[%s] brightness = %d, bl_lvl = %d -> differed %s\n", panel->type, brightness, bl_lvl, lge_get_blmapname(bl_type));
	}
	mutex_unlock(&display->display_lock);

	return rc;
}

static int lge_mipi_dsi_dcs_set_display_brightness_extension_short(struct dsi_panel *panel, u32 bl_lvl)
{
	char *bc_payload = NULL;
	int rc = 0;
	u32 count = 0;
	u32 bl_position = 0;

	count = panel->lge.lge_cmd_sets[LGE_DDIC_DSI_BRIGHTNESS_CTRL_EXT_COMMAND].count;

	if (!count) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	bl_position = (count > 1)? 1:0;
	bl_lvl &= 0xFF;

	bc_payload = get_payload_addr(panel, LGE_DDIC_DSI_BRIGHTNESS_CTRL_EXT_COMMAND, bl_position);

	if (!bc_payload) {
		pr_err("bc_payload null\n");
		return -EINVAL;
	}
	if (bl_position)
		bc_payload[1] = bl_lvl;
	else
		bc_payload[0] = bl_lvl;

	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_BRIGHTNESS_CTRL_EXT_COMMAND);

	return rc;
}

static int lge_mipi_dsi_dcs_set_display_brightness_short(struct mipi_dsi_device *dsi, u16 brightness)
{
	u8 payload[1] = { brightness & 0xff };
	ssize_t err;

	err = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_DISPLAY_BRIGHTNESS,
				 payload, sizeof(payload));
	if (err < 0)
		return err;

	return 0;
}

/* @Override */
int dsi_panel_update_backlight(struct dsi_panel *panel,
	u32 bl_lvl)
{
	int rc = 0;
	struct mipi_dsi_device *dsi;

	if (!panel || (bl_lvl > 0xffff)) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	pr_info("[%s]: bl_lvl(%d)\n", panel->type, bl_lvl);

	dsi = &panel->mipi_device;

	if (panel->lge.use_br_ctrl_ext &&
		((panel->lge.lp_state == LGE_PANEL_LP1) || (panel->lge.lp_state == LGE_PANEL_LP2))){
		rc = lge_mipi_dsi_dcs_set_display_brightness_extension_short(panel, bl_lvl);
	} else {
		if (panel->lge.use_dcs_brightness_short) {
			rc = lge_mipi_dsi_dcs_set_display_brightness_short(dsi, bl_lvl);
		} else {
			if (panel->lge.dcs_brightness_be) {
				bl_lvl = ((bl_lvl & 0xff00)>>8) | ((bl_lvl & 0xff)<<8);
				pr_debug("[byte-order] bl_lvl = 0x%X\n", bl_lvl);
			}
			rc = mipi_dsi_dcs_set_display_brightness(dsi, bl_lvl);
		}
	}

	if (rc < 0)
		pr_err("failed to update dcs backlight:%d\n", bl_lvl);

	return rc;
}

static ssize_t irc_brighter_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel;
	int ret = 0;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return ret;
	}

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->get_irc_state)
		ret = (int)panel->lge.ddic_ops->get_irc_state(panel);
	else
		pr_info("Not support\n");

	return sprintf(buf, "%d\n", ret);
}

static ssize_t irc_brighter_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;
	struct dsi_display *display;
	int input;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &input);
	pr_info("[%s] input data %d\n", panel->type, input);

	mutex_lock(&panel->panel_lock);
	if(!dsi_panel_initialized(panel) &&
			(panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl)) {
		panel->lge.irc_pending = true;
		panel->lge.irc_request_state = ((input == 1) ? LGE_IRC_OFF : LGE_IRC_ON);
		pr_err("panel not yet initialized. irc_ctrl is stored.\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}
	mutex_unlock(&panel->panel_lock);

	display = container_of(panel->host, struct dsi_display, host);;

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);
	if ((panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl) &&
			panel->lge.ddic_ops && panel->lge.ddic_ops->set_irc_state) {
		panel->lge.irc_pending = true;
		panel->lge.irc_request_state = ((input == 1) ? LGE_IRC_OFF : LGE_IRC_ON);
		mutex_unlock(&panel->panel_lock);
		panel->lge.ddic_ops->set_irc_state(panel, LGE_GLOBAL_IRC_HBM,
						panel->lge.irc_request_state);
	} else {
		mutex_unlock(&panel->panel_lock);
		pr_info("Not support\n");
	}

	return ret;
}
static DEVICE_ATTR(irc_brighter, S_IRUGO | S_IWUSR | S_IWGRP, irc_brighter_show, irc_brighter_store);

static ssize_t irc_support_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel;
	int ret = 0;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return ret;
	}

	if (!(panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl)) {
		pr_err("irc control is not supported\n");
	} else {
		ret = panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl;
	}

	return sprintf(buf, "%d\n", ret);
}
static DEVICE_ATTR(irc_support, S_IRUGO, irc_support_show, NULL);

static ssize_t brightness_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel;
	int ret = 0;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return ret;
	}

	mutex_lock(&panel->panel_lock);
	ret = panel->lge.brightness_table;
	mutex_unlock(&panel->panel_lock);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t brightness_table_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;
	struct dsi_display *display;
	int input;
	struct backlight_device *bd;

	panel = dev_get_drvdata(dev);
	if (!panel || !panel->bl_config.raw_bd) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	bd = panel->bl_config.raw_bd;
	sscanf(buf, "%d", &input);
	pr_info("[%s] input data %d\n", panel->type, input);

	mutex_lock(&panel->panel_lock);
	if(!dsi_panel_initialized(panel)) {
		pr_err("panel not yet initialized..\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}
	mutex_unlock(&panel->panel_lock);

	display = container_of(panel->host, struct dsi_display, host);;

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);
	panel->lge.brightness_table = input;
	mutex_unlock(&panel->panel_lock);

	mutex_lock(&bd->ops_lock);
	lge_backlight_device_update_status(panel->bl_config.raw_bd);
	mutex_unlock(&bd->ops_lock);
	return ret;
}
static DEVICE_ATTR(brightness_table, S_IRUGO | S_IWUSR | S_IWGRP, brightness_table_show, brightness_table_store);

static ssize_t fp_lhbm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel;
	int ret = 0;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return ret;
	}

	mutex_lock(&panel->panel_lock);
	ret = panel->lge.fp_lhbm_mode;
	mutex_unlock(&panel->panel_lock);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t fp_lhbm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;
	struct dsi_display *display;
	int input;
	int retry_cnt = 0;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &input);
	pr_info("[%s] input data %d\n", panel->type, input);

retry_lhbm:
	mutex_lock(&panel->panel_lock);

	panel->lge.fp_lhbm_mode = input;

	if (!dsi_panel_initialized(panel) || (panel->lge.lp_state == LGE_PANEL_OFF)) {
		if (retry_cnt < 40) {
			retry_cnt++;
			mutex_unlock(&panel->panel_lock);
			msleep(10);
			goto retry_lhbm;
		}

		pr_err("not ready, initialized: %d, lp_state:%d\n", panel->panel_initialized, panel->lge.lp_state);
		panel->lge.need_fp_lhbm_set = true;
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}

	mutex_unlock(&panel->panel_lock);

	display = container_of(panel->host, struct dsi_display, host);

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	if (panel->lge.ddic_ops && panel->lge.ddic_ops->lge_set_fp_lhbm)
		panel->lge.ddic_ops->lge_set_fp_lhbm(panel, panel->lge.fp_lhbm_mode);
	mutex_unlock(&display->display_lock);

	return ret;
}
static DEVICE_ATTR(fp_lhbm, S_IRUGO | S_IWUSR | S_IWGRP, fp_lhbm_show, fp_lhbm_store);

static ssize_t fp_lhbm_br_lvl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel;
	int ret = 0;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return ret;
	}

	mutex_lock(&panel->panel_lock);
	ret = panel->lge.fp_lhbm_br_lvl;
	mutex_unlock(&panel->panel_lock);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t fp_lhbm_br_lvl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;
	struct dsi_display *display;
	int input;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &input);
	pr_info("[%s] input data %d\n", panel->type, input);

	mutex_lock(&panel->panel_lock);
	if(!dsi_panel_initialized(panel)) {
		pr_err("panel not yet initialized..\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}
	mutex_unlock(&panel->panel_lock);

	display = container_of(panel->host, struct dsi_display, host);

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->lge_set_fp_lhbm_br_lvl)
		panel->lge.ddic_ops->lge_set_fp_lhbm_br_lvl(panel, input);

	return ret;
}
static DEVICE_ATTR(fp_lhbm_br_lvl, S_IRUGO | S_IWUSR | S_IWGRP, fp_lhbm_br_lvl_show, fp_lhbm_br_lvl_store);


static ssize_t tc_perf_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel;
	int ret = 0;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return ret;
	}

	mutex_lock(&panel->panel_lock);
	ret = panel->lge.tc_perf;
	mutex_unlock(&panel->panel_lock);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t tc_perf_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;
	struct dsi_display *display;
	int input;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &input);
	pr_info("[%s] input data %d\n", panel->type, input);

	mutex_lock(&panel->panel_lock);
	if(!dsi_panel_initialized(panel)) {
		pr_err("panel not yet initialized..\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}
	mutex_unlock(&panel->panel_lock);

	display = container_of(panel->host, struct dsi_display, host);

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}

	panel->lge.tc_perf = input;

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->lge_set_tc_perf)
		panel->lge.ddic_ops->lge_set_tc_perf(panel, input);

	return ret;
}
static DEVICE_ATTR(tc_perf, S_IRUGO | S_IWUSR | S_IWGRP, tc_perf_show, tc_perf_store);

#if IS_ENABLED(CONFIG_LGE_DUAL_SCREEN) || IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
static ssize_t br_offset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel;
	int ret = 0;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return ret;
	}
#if IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
	if (!secondary_display || !secondary_display->panel) {
		pr_err("secondary_display is NULL\n");
		return -EINVAL;
	}

	if (panel->lge.br_offset)
		return sprintf(buf, "%d\n", panel->lge.br_offset);
	else
		return sprintf(buf, "%d\n", secondary_display->panel->lge.br_offset);
#else
	return sprintf(buf, "%d\n", panel->lge.br_offset);
#endif
}

static ssize_t br_offset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;
	int data;
	bool connect = false;
#if IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
	struct backlight_device *bd;
#endif

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &data);

#if IS_ENABLED(CONFIG_LGE_DUAL_SCREEN)
	if (is_ds_connected()) {
#elif IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
	if (!secondary_display || !secondary_display->panel || !secondary_display->panel->bl_config.raw_bd) {
		pr_err("secondary_display is NULL\n");
		return -EINVAL;
	}
	bd = secondary_display->panel->bl_config.raw_bd;
	if (lge_dsi_panel_is_power_on_interactive(secondary_display->panel)) {
#endif
		connect = true;
	}

	if (connect && (data == BR_OFFSET_BYPASS)) {
		panel->lge.br_offset_bypass = true;
		pr_err("enable bypass\n");
		return ret;
	}

#if IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
	if(connect) {
		secondary_display->panel->lge.br_offset_update = true;
		secondary_display->panel->lge.br_offset = data;
		mutex_lock(&bd->ops_lock);
		lge_backlight_device_update_status(secondary_display->panel->bl_config.raw_bd);
		mutex_unlock(&bd->ops_lock);
		secondary_display->panel->lge.br_offset_update = false;
	}

	if (data > 0) {
		panel->lge.br_offset = data;
		secondary_display->panel->lge.br_offset = 0;
	} else {
		panel->lge.br_offset = 0;
		secondary_display->panel->lge.br_offset = data;
	}

	pr_info("request=[%d][%d]\n", panel->lge.br_offset, secondary_display->panel->lge.br_offset);
#else
	panel->lge.br_offset = data;
	pr_info("request=%d\n", panel->lge.br_offset);
#endif

	if (panel->lge.br_offset_bypass)
		panel->lge.br_offset_bypass = false;

	return ret;
}
static DEVICE_ATTR(br_offset, S_IRUGO | S_IWUSR | S_IWGRP, br_offset_show, br_offset_store);
#endif

#if IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
static ssize_t br_sync_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel;
	int ret = 0;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return ret;
	}

	if (!secondary_display || !secondary_display->panel) {
		pr_err("secondary_display is NULL\n");
		return -EINVAL;
	}

	return sprintf(buf, "%d\n", panel->lge.br_sync_mode);
}

static ssize_t br_sync_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;
	int data;
	struct backlight_device *primary_bd;
	struct backlight_device *secondary_bd;

	panel = dev_get_drvdata(dev);
	if (!panel || !panel->bl_config.raw_bd) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	if (!secondary_display || !secondary_display->panel || !secondary_display->panel->bl_config.raw_bd) {
		pr_err("secondary_display is NULL\n");
		return -EINVAL;
	}

	primary_bd = panel->bl_config.raw_bd;
	secondary_bd = secondary_display->panel->bl_config.raw_bd;

	sscanf(buf, "%d", &data);

	panel->lge.br_sync_mode = !!data;

	pr_info("request=%d\n", panel->lge.br_sync_mode);

	mutex_lock(&primary_bd->ops_lock);
	lge_backlight_device_update_status(panel->bl_config.raw_bd);
	mutex_unlock(&primary_bd->ops_lock);

	if (secondary_display && secondary_display->panel &&
		lge_dsi_panel_is_power_on_interactive(secondary_display->panel) &&
		secondary_display->panel->lge.br_offset) {
		mutex_lock(&secondary_bd->ops_lock);
		lge_backlight_device_update_status(secondary_display->panel->bl_config.raw_bd);
		mutex_unlock(&secondary_bd->ops_lock);
	}
	return ret;
}
static DEVICE_ATTR(br_sync_mode, S_IRUGO | S_IWUSR | S_IWGRP, br_sync_mode_show, br_sync_mode_store);
#endif

static struct attribute *brightness_attrs[] = {
#if IS_ENABLED(CONFIG_LGE_DUAL_SCREEN) || IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
	&dev_attr_br_offset.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_SECONDARY_SCREEN)
	&dev_attr_br_sync_mode.attr,
#endif
	NULL,
};

static struct attribute *irc_attrs[] = {
	&dev_attr_irc_brighter.attr,
	&dev_attr_irc_support.attr,
	NULL,
};

static struct attribute *dyn_brightness_attrs[] = {
	&dev_attr_brightness_table.attr,
	NULL,
};

static struct attribute *fp_lhbm_attrs[] = {
	&dev_attr_fp_lhbm.attr,
	&dev_attr_fp_lhbm_br_lvl.attr,
	NULL,
};

static struct attribute *tc_perf_attrs[] = {
	&dev_attr_tc_perf.attr,
	NULL,
};

static const struct attribute_group brightness_attr_group = {
	.name	= "brightness",
	.attrs	= brightness_attrs,
};

static const struct attribute_group irc_attr_group = {
	.name	= "brightness",
	.attrs	= irc_attrs,
};

static const struct attribute_group dyn_brightness_attr_group = {
	.name	= "brightness",
	.attrs	= dyn_brightness_attrs,
};

static const struct attribute_group fp_lhbm_attr_group = {
	.name	= "brightness",
	.attrs	= fp_lhbm_attrs,
};

static const struct attribute_group tc_perf_attr_group = {
	.name	= "brightness",
	.attrs	= tc_perf_attrs,
};

int lge_brightness_create_sysfs(struct dsi_panel *panel, struct device *panel_sysfs_dev)
{
	int rc = 0;

	if (panel_sysfs_dev) {
		if ((rc = sysfs_create_group(&panel_sysfs_dev->kobj, &brightness_attr_group)) < 0)
			pr_err("create brightness group fail!");

		if (panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl) {
			if ((rc = sysfs_merge_group(&panel_sysfs_dev->kobj, &irc_attr_group)) < 0)
				pr_err("merge irc group fail!\n");
		}

		if (panel->lge.use_dynamic_brightness) {
			if ((rc = sysfs_merge_group(&panel_sysfs_dev->kobj, &dyn_brightness_attr_group)) < 0)
				pr_err("merge dynamic_brightness group fail!\n");
		}

		if (panel->lge.use_fp_lhbm) {
			if ((rc = sysfs_merge_group(&panel_sysfs_dev->kobj, &fp_lhbm_attr_group)) < 0)
				pr_err("merge fp_lhbm group fail!\n");
		}

		if (panel->lge.use_tc_perf) {
			if ((rc = sysfs_merge_group(&panel_sysfs_dev->kobj, &tc_perf_attr_group)) < 0)
				pr_err("merge tc_perf group fail!\n");
		}
	}
	return rc;
}
