#define pr_fmt(fmt)	"[Display][rm692C9-ops:%s:%d] " fmt, __func__, __LINE__

#include "dsi_panel.h"
#include "dsi_display.h"
#include "sde_connector.h"
#include "lge_ddic_ops_helper.h"
#include "cm/lge_color_manager.h"
#include "brightness/lge_brightness_def.h"

#define DDIC_REG_DEBUG 0

#define ADDR_PTLAR 0x30
#define HBM_BC_DIMMING_DEFAULT 0xE0
#define SHARPNESS_DEFAULT 0x00
#define CM_SATURATION_INDEX 0
#define CM_HUE_INDEX 13
#define CM_RED_INDEX 22
#define CM_GREEN_INDEX 28
#define CM_BLUE_INDEX 34
#define SATURATION_DEFAULT 0x0B
#define CGM_DEFAULT 0x10

#define FP_LHBM_DDIC_ON				0x01
#define FP_LHBM_DDIC_OFF			0x00

#define WORDS_TO_BYTE_ARRAY(w1, w2, b) do {\
		b[0] = WORD_UPPER_BYTE(w1);\
		b[1] = WORD_LOWER_BYTE(w1);\
		b[2] = WORD_UPPER_BYTE(w2);\
		b[3] = WORD_LOWER_BYTE(w2);\
} while(0)

extern int lge_ddic_dsi_panel_tx_cmd_set(struct dsi_panel *panel,
				enum lge_ddic_dsi_cmd_set_type type);
extern int lge_backlight_device_update_status(struct backlight_device *bd);
extern char* get_payload_addr(struct dsi_panel *panel, enum lge_ddic_dsi_cmd_set_type type, int position);
extern int get_payload_cnt(struct dsi_panel *panel, enum lge_ddic_dsi_cmd_set_type type, int position);

extern int get_cm_lut_payload_cnt(struct dsi_panel *panel, enum lge_cm_lut_type type, int position);
extern char* get_cm_lut_payload_addr(struct dsi_panel *panel, enum lge_cm_lut_type type, int position);

const struct drs_res_info rm692C9_res[1] = {
	{"fhd", 0, 1080, 2340},
};

static void dump_cm_dummy_reg_value(struct dsi_panel *panel)
{
	int i = 0;
	char *payload = NULL;
	int length = 0;

	length = get_cm_lut_payload_cnt(panel, LGE_CM_LUT_COLOR_MODE_SET, 0);
	pr_info("CM-DEBUG] =========================\n");
	for (i = 0; i < length; i++) {
		payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, i);
		if (!payload) {
			pr_err("dummy payload is null\n");
			return;
		}

		pr_info("[%d] 0x%02x : 0x%02x\n", i, *payload, *(payload + 1));
	}
	pr_info("CM-DEBUG] =========================\n");
}

static void lge_display_control_store_rm692C9(struct dsi_panel *panel, bool send_cmd)
{
	char *sharpness_payload = NULL;
	char *sharpness_level_payload = NULL;
	char *saturation_payload = NULL;
	char *cgm_payload = NULL;
	char *src = NULL;

	if(!panel) {
		pr_err("panel not exist\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	sharpness_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1, 1);
	sharpness_level_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1, 4);
	saturation_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, 1);
	cgm_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, 38);

	if (!sharpness_payload || !sharpness_level_payload || !saturation_payload || !cgm_payload) {
		pr_err("LGE_DDIC_DSI_DISP_CTRL_COMMAND is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	/* HDR_ON & ASE_ON */
	sharpness_payload[1] &= SHARPNESS_DEFAULT;
	sharpness_level_payload[1] &= SHARPNESS_DEFAULT;
	if (panel->lge.sharpness_status && !panel->lge.hdr_mode) {
		sharpness_payload[1] = 0x0F;
		src = get_cm_lut_payload_addr(panel, LGE_CM_LUT_SHARPNESS, panel->lge.sc_sha_step);
		if (src == NULL) {
			pr_err("get sat lut failed! set to default\n");
		} else {
			sharpness_level_payload[1] = src[0];
		}
	}

	saturation_payload[1] = SATURATION_DEFAULT;
	cgm_payload[1] = CGM_DEFAULT;
	if (panel->lge.hdr_mode) {
		saturation_payload[1] = 0x11;
		cgm_payload[1] = 0x00;
	}

	pr_info("ctrl-command-1 [SH]0x%02x [SL]0x%02x\n",
			sharpness_payload[1], sharpness_level_payload[1]);

	if (send_cmd) {
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY);
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1);
	}
	mutex_unlock(&panel->panel_lock);
	if(DDIC_REG_DEBUG)
		dump_cm_dummy_reg_value(panel);
	return;
}

int hdr_mode_set_rm692C9(struct dsi_panel *panel, int input)
{
	bool hdr_mode = ((input > 0) ? true : false);
	struct backlight_device *bd;

	if (!panel->bl_config.raw_bd) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}
	bd = panel->bl_config.raw_bd;

	pr_info("hdr=%s\n", (hdr_mode ? "set" : "unset"));

	lge_display_control_store_rm692C9(panel, true);

	mutex_lock(&bd->ops_lock);
	lge_backlight_device_update_status(panel->bl_config.raw_bd);
	mutex_unlock(&bd->ops_lock);
	return 0;
}

static int update_color_table(struct dsi_panel *panel, int idx)
{
	int i = 0;
	char *dst = NULL;
	char *src = NULL;
	int cmd_idx = 0;

	cmd_idx = get_cm_lut_payload_cnt(panel, LGE_CM_LUT_COLOR_MODE_SET, idx);
	src = get_cm_lut_payload_addr(panel, LGE_CM_LUT_COLOR_MODE_SET, idx);
	if (src == NULL) {
		pr_err("get cm set failed\n");
		return -EINVAL;
	}

	for (i = 0; i < cmd_idx; i++) {
		dst = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, i);
		if (!dst) {
			pr_err("get dst is failed\n");
			return -EINVAL;
		}
		*(++dst) = src[i];
	}

	return 0;
}

static void color_space_mapper(struct dsi_panel *panel, char *mode)
{
	int i = 0;

	if (!panel->lge.cm_lut_cnt) {
		pr_err("fail to update color mode table beacause cm_lut_cnt is 0.\n");
		return;
	}

	for (i = 0; i < panel->lge.cm_lut_cnt; i++) {
		if(!strncmp(mode, panel->lge.cm_lut_name_list[i], 3)) {
			if(update_color_table(panel, i) < 0) {
				pr_err("failed to update color mode table\n");
				return;
			}
		}
	}
}

static void update_rgb_table(struct dsi_panel *panel, int idx, int step)
{
	char *payload = NULL;
	u8 upper_value = 0x0;
	u8 lower_value = 0x0;
	u16 new_payload = 0x00;

	payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, idx + 1);
	if (!payload) {
		pr_err("dummy payload is null\n");
		return;
	}
	payload++;
	upper_value = (u8)(*payload);

	payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, idx);
	if (!payload) {
		pr_err("dummy payload is null\n");
		return;
	}
	payload++;
	lower_value = (u8)(*payload);

	new_payload = (u16)((upper_value << 8) | lower_value);
	if (step > 0)
		new_payload -= (0x10 * step);

	/* update upper */
	payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, idx + 1);
	if (!payload) {
		pr_err("upper value payload is null\n");
		return;
	}
	payload++;
	*payload = (u8)((new_payload & 0xFF00) >> 8);

	/* update lower */
	payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, idx);
	if (!payload) {
		pr_err("lower value payload is null\n");
		return;
	}
	payload++;
	*payload = (u8)(new_payload & 0x00FF);
}

static void custom_rgb_mapper(struct dsi_panel *panel, int cm_step, int rs, int gs, int bs)
{
	int i = 0;
	char *dst = NULL;
	char *src = NULL;
	int cmd_idx = 0;

	if (panel == NULL) {
		pr_err("null ptr\n");
		return;
	}

	cmd_idx = get_cm_lut_payload_cnt(panel, LGE_CM_LUT_RGB, cm_step);
	src = get_cm_lut_payload_addr(panel, LGE_CM_LUT_RGB, cm_step);
	if (src == NULL) {
		pr_err("get rgb set failed\n");
		return;
	}
	for (i = 0; i < cmd_idx; i++) {
		dst = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, CM_RED_INDEX + i);
		if (!dst) {
			pr_err("dummy dst is null\n");
			return;
		}
		*(++dst) = src[i];
	}

	update_rgb_table(panel, CM_RED_INDEX, rs);
	update_rgb_table(panel, CM_GREEN_INDEX, gs);
	update_rgb_table(panel, CM_BLUE_INDEX, bs);
}

static void screen_tune_mapper(struct dsi_panel *panel, int hue_step, int sat_step)
{
	int i = 0;
	char *dst = NULL;
	char *src = NULL;
	int cmd_idx = 0;

	if (panel == NULL) {
		pr_err("null ptr\n");
		return;
	}

	/* Saturation Control */
	cmd_idx = get_cm_lut_payload_cnt(panel, LGE_CM_LUT_SATURATION, sat_step);
	src = get_cm_lut_payload_addr(panel, LGE_CM_LUT_SATURATION, sat_step);
	if (src == NULL) {
		pr_err("get sat lut failed\n");
		return;
	}
	for (i = 0; i < cmd_idx; i++) {
		dst = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, CM_SATURATION_INDEX + i);
		*(++dst) = src[i];
	}

	/* Hue Control */
	cmd_idx = get_cm_lut_payload_cnt(panel, LGE_CM_LUT_HUE, hue_step);
	src = get_cm_lut_payload_addr(panel, LGE_CM_LUT_HUE, hue_step);
	if (src == NULL) {
		pr_err("get hue lut failed\n");
		return;
	}
	for (i = 0; i < cmd_idx; i++) {
		dst = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY, CM_HUE_INDEX + i);
		*(++dst) = src[i];
	}
}

static void lge_set_screen_mode_rm692C9(struct dsi_panel *panel, bool send_cmd)
{
	mutex_lock(&panel->panel_lock);

	pr_info("screen_mode %d\n", panel->lge.screen_mode);

	switch (panel->lge.screen_mode) {
	case screen_mode_oled_natural:
		pr_info("preset: %d, red: %d, green: %d, blue: %d\n",
				panel->lge.cm_preset_step, panel->lge.cm_red_step,
				panel->lge.cm_green_step, panel->lge.cm_blue_step);
		panel->lge.sharpness_status = 0x00;
		color_space_mapper(panel, "nat");
		custom_rgb_mapper(panel, panel->lge.cm_preset_step, 0, 0, 0);
		break;
	case screen_mode_oled_vivid:
		color_space_mapper(panel, "viv");
		panel->lge.sharpness_status = 0x00;
		break;
	case screen_mode_oled_cinema:
		color_space_mapper(panel, "cin");
		panel->lge.sharpness_status = 0x00;
		break;
	case screen_mode_oled_custom:
		color_space_mapper(panel, "nat");
		panel->lge.sharpness_status = 0x01;
		pr_info("saturation: %d, hue: %d, sharpness: %d\n",
				panel->lge.sc_sat_step, panel->lge.sc_hue_step, panel->lge.sc_sha_step);
		custom_rgb_mapper(panel,
			panel->lge.cm_preset_step, panel->lge.cm_red_step,
			panel->lge.cm_green_step, panel->lge.cm_blue_step);
		screen_tune_mapper(panel, panel->lge.sc_hue_step, panel->lge.sc_sat_step);
		break;
	default:
		panel->lge.sharpness_status = 0x00;
		color_space_mapper(panel, "nat");
		break;
	}
	mutex_unlock(&panel->panel_lock);

	lge_display_control_store_rm692C9(panel, send_cmd);
    return;
}

static void lge_set_screen_tune_rm692C9(struct dsi_panel *panel)
{
	pr_info("hue_idx=%d, sat_idx=%d, sha_idx=%d\n", panel->lge.sc_hue_step,
				panel->lge.sc_sat_step, panel->lge.sc_sha_step);

	mutex_lock(&panel->panel_lock);
	color_space_mapper(panel, "exp");
	/* sharpness will be controlled in control func. */
	screen_tune_mapper(panel, panel->lge.sc_hue_step, panel->lge.sc_sat_step);
	mutex_unlock(&panel->panel_lock);

	return;
}

static void lge_set_rgb_tune_rm692C9(struct dsi_panel *panel, bool send_cmd)
{
	pr_info("cm_step=%d, red_idx=%d, green_idx=%d, blue_idx=%d\n",
				panel->lge.cm_preset_step, panel->lge.cm_red_step,
				panel->lge.cm_green_step, panel->lge.cm_blue_step);

	mutex_lock(&panel->panel_lock);
	custom_rgb_mapper(panel, panel->lge.cm_preset_step, panel->lge.cm_red_step,
				panel->lge.cm_green_step, panel->lge.cm_blue_step);
	mutex_unlock(&panel->panel_lock);

	return;
}

static void sharpness_set_rm692C9(struct dsi_panel *panel, int input)
{
	mutex_lock(&panel->panel_lock);
	if (input)
		panel->lge.sharpness_status = 0x01;
	else
		panel->lge.sharpness_status = 0x00;
	mutex_unlock(&panel->panel_lock);

	lge_display_control_store_rm692C9(panel, true);
}

static void lge_set_video_enhancement_rm692C9(struct dsi_panel *panel, int input)
{
	int rc = 0;
	struct backlight_device *bd;

	if (!panel->bl_config.raw_bd) {
		pr_err("backlight device is NULL\n");
		return;
	}
	bd = panel->bl_config.raw_bd;

	if (input) {
		mutex_lock(&panel->panel_lock);
		rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_VIDEO_ENHANCEMENT_ON);
		mutex_unlock(&panel->panel_lock);
		if (rc)
			pr_err("failed to send VIDEO_ENHANCEMENT_ON cmd, rc=%d\n", rc);
	} else {
		lge_set_screen_mode_rm692C9(panel, true);
	}

	pr_info("send cmds to %s the video enhancer \n",
		(input == true) ? "enable" : "disable");

	mutex_lock(&bd->ops_lock);
	lge_backlight_device_update_status(panel->bl_config.raw_bd);
	mutex_unlock(&bd->ops_lock);
}

struct lge_ddic_ops rm692C9_ops = {
	.store_aod_area = NULL,
	.prepare_aod_cmds = NULL,
	.prepare_aod_area = NULL,
	.lge_check_vert_black_line = NULL,
	.lge_check_vert_white_line = NULL,
	.lge_check_vert_line_restore = NULL,
	.lge_bc_dim_set = NULL,
	.lge_set_therm_dim = NULL,
	.lge_get_brightness_dim = NULL,
	.lge_set_brightness_dim = NULL,
	.hdr_mode_set = hdr_mode_set_rm692C9,
	.lge_set_rgb_tune = lge_set_rgb_tune_rm692C9,
	.lge_display_control_store = lge_display_control_store_rm692C9,
	.lge_set_screen_tune = lge_set_screen_tune_rm692C9,
	.lge_set_screen_mode = lge_set_screen_mode_rm692C9,
	.sharpness_set = sharpness_set_rm692C9,
	.lge_set_true_view_mode = NULL,
	.lge_set_video_enhancement = lge_set_video_enhancement_rm692C9,
	.lge_set_fp_lhbm = NULL,
	.lge_set_fp_lhbm_br_lvl = NULL,
	/* drs not supported */
	.get_current_res = NULL,
	.get_support_res = NULL,
	/* bist not supported */
	.bist_ctrl = NULL,
	.release_bist = NULL,
	/* error detect not supported */
	.err_detect_work = NULL,
	.err_detect_irq_handler = NULL,
	.set_err_detect_mask = NULL,
	/* pps not used */
	.set_pps_cmds = NULL,
	.unset_pps_cmds = NULL,
	/* irc not supported */
	.set_irc_default_state = NULL,
	.set_irc_state = NULL,
	.get_irc_state = NULL,
};
