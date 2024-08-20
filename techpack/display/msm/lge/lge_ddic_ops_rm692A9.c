#define pr_fmt(fmt)	"[Display][rm692A9-ops:%s:%d] " fmt, __func__, __LINE__

#include "dsi_panel.h"
#include "dsi_display.h"
#include "sde_connector.h"
#include "lge_ddic_ops_helper.h"
#include "cm/lge_color_manager.h"
#include "brightness/lge_brightness_def.h"

#define DDIC_REG_DEBUG 0

#define ADDR_PTLAR 0x30
#define HBM_BC_DIMMING_DEFAULT 0xE0
#define CM_DEFAULT 0x00
#define SHARPNESS_DEFAULT 0x00
#define MOVE_WP_DEFAULT 0x08
#define CM_SATURATION_INDEX 0
#define CM_RED_INDEX 19
#define CM_GREEN_INDEX 25
#define CM_BLUE_INDEX 31
#define SATURATION_DEFAULT 0x0B

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

const struct drs_res_info rm692A9_res[1] = {
	{"fhd", 0, 1080, 2340},
};

static void dump_cm_dummy_reg_value(struct dsi_panel *panel)
{
	int i = 0;
	char *payload = NULL;
	int length = 0;

	if(1) {
		pr_err("display bringup\n");
		return;
	}

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

static void lge_display_control_store_rm692A9(struct dsi_panel *panel, bool send_cmd)
{
	char *cm_payload = NULL;
	char *sharpness_payload = NULL;
	char *sharpness_level_payload = NULL;
	char *fixed_wp_payload = NULL;
	char *src = NULL;

	if(!panel) {
		pr_err("panel not exist\n");
		return;
	}

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	sharpness_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1, 1);
	sharpness_level_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1, 4);
	fixed_wp_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_2, 2);
	cm_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_2, 1);

	if (!cm_payload || !sharpness_payload || !sharpness_level_payload || !fixed_wp_payload) {
		pr_err("LGE_DDIC_DSI_DISP_CTRL_COMMAND is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	/* CM_SEL & CE_SEL */
	/* To Do */
	cm_payload[1] &= CM_DEFAULT;
	if (panel->lge.color_manager_status)
		cm_payload[1] = 0x10;

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
	if (panel->lge.use_ace_ctrl)
//		lge_set_ace_rm692A9(panel, panel->lge.irc_current_state, panel->lge.hdr_mode, panel->lge.ve_hdr);

	fixed_wp_payload[1] |= MOVE_WP_DEFAULT;
	if (panel->lge.move_wp)
		fixed_wp_payload[1] = 0x00;

	pr_info("ctrl-command-1 [SH]0x%02x [SL]0x%02x [WP]0x%02x [CM]0x%02x\n",
			sharpness_payload[1], sharpness_level_payload[1],
			fixed_wp_payload[1], cm_payload[1]);

	if (send_cmd) {
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY);
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1);
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_2);
	}
	mutex_unlock(&panel->panel_lock);
	if(DDIC_REG_DEBUG)
		dump_cm_dummy_reg_value(panel);
	return;
}

static void lge_bc_dim_set_rm692A9(struct dsi_panel *panel, u8 bc_dim_en, u8 bc_dim_f_cnt)
{
	char *payload = NULL;
	u8 data = 0;

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	mutex_lock(&panel->panel_lock);
	payload = get_payload_addr(panel, LGE_DDIC_DSI_BC_DEFAULT_DIMMING, 1);
	if (!payload) {
		mutex_unlock(&panel->panel_lock);
		return;
	}

	data = (bc_dim_f_cnt - 1);
	payload++;
	*payload = ((data << 4) | data);
	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_BC_DEFAULT_DIMMING);

	panel->lge.bc_dim_en = bc_dim_en;

	mutex_unlock(&panel->panel_lock);

	lge_display_control_store_rm692A9(panel, true);

	mdelay(15);
	pr_info("EN : %d FRAMES: 0x%x\n", panel->lge.bc_dim_en, *payload);
}

static int lge_get_brightness_dim_rm692A9(struct dsi_panel *panel)
{
	u8 bc_dim_f_cnt;
	char *payload = NULL;

	if(1) {
		pr_err("display bringup\n");
		return 0;
	}

	payload = get_payload_addr(panel, LGE_DDIC_DSI_BC_DEFAULT_DIMMING, 1);

	if (payload) {
		payload++;
		bc_dim_f_cnt = (*payload + 1);
	} else {
		pr_err("bc_dim_f_cnt payload is not available\n");
		return -EINVAL;
	}
	return bc_dim_f_cnt;

}

static void lge_set_brightness_dim_rm692A9(struct dsi_panel *panel, int input)
{
	u8 bc_dim_f_cnt;

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	if (input == BC_DIM_OFF) {
		pr_warn("rm692A9 do not off bc_dim feature\n");
		bc_dim_f_cnt = BC_DIM_FRAMES_NORMAL;
	} else {
		if (input < BC_DIM_MIN_FRAMES)
			bc_dim_f_cnt = BC_DIM_MIN_FRAMES;
		else if (input > BC_DIM_MAX_FRAMES)
			bc_dim_f_cnt = BC_DIM_MAX_FRAMES;
		else
			bc_dim_f_cnt = input;
	}

	lge_bc_dim_set_rm692A9(panel, BC_DIM_ON, bc_dim_f_cnt);
}

int hdr_mode_set_rm692A9(struct dsi_panel *panel, int input)
{
	bool hdr_mode = ((input > 0) ? true : false);
	bool need_color_mode_unset = false;

	if(1) {
		pr_err("display bringup\n");
		return 0;
	}

	mutex_lock(&panel->panel_lock);

	switch (panel->lge.screen_mode) {
	case screen_mode_auto:
	case screen_mode_cinema:
	case screen_mode_photos:
	case screen_mode_web:
		need_color_mode_unset = true;
		break;
	default:
		break;
	}

	if (hdr_mode && need_color_mode_unset) {
		panel->lge.color_manager_status = 0;
	} else {
		panel->lge.color_manager_status = 1;
	}
	mutex_unlock(&panel->panel_lock);

	pr_info("hdr=%s, cm=%s\n", (hdr_mode ? "set" : "unset"),
			((panel->lge.color_manager_status == 1) ? "enabled" : "disabled"));

	lge_display_control_store_rm692A9(panel, true);

	lge_backlight_device_update_status(panel->bl_config.raw_bd);
	return 0;
}

static int update_color_table(struct dsi_panel *panel, int idx)
{
	int i = 0;
	char *dst = NULL;
	char *src = NULL;
	int cmd_idx = 0;

	if(1) {
		pr_err("display bringup\n");
		return 0;
	}

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

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	if (!panel->lge.cm_lut_cnt) {
		pr_err("fail to update color mode table beacause cm_lut_cnt is 0.\n");
		return;
	}
	panel->lge.color_manager_status = 0x01;

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

	if(1) {
		pr_err("display bringup\n");
		return;
	}

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
	int sc_hue_step = 0;

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	if (panel == NULL) {
		pr_err("null ptr\n");
		return;
	}

	panel->lge.color_manager_status = 0x01;
	if (panel->lge.screen_mode == screen_mode_auto)
		sc_hue_step = 2;
	else
		sc_hue_step = panel->lge.sc_hue_step;

	cmd_idx = get_cm_lut_payload_cnt(panel, LGE_CM_LUT_RGB_HUE, cm_step + 5*sc_hue_step);
	src = get_cm_lut_payload_addr(panel, LGE_CM_LUT_RGB_HUE, cm_step + 5*sc_hue_step);
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

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	if (panel == NULL) {
		pr_err("null ptr\n");
		return;
	}

	panel->lge.color_manager_status = 0x01;

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
	custom_rgb_mapper(panel,
		panel->lge.cm_preset_step, panel->lge.cm_red_step,
		panel->lge.cm_green_step, panel->lge.cm_blue_step);
}

static void lge_set_screen_mode_rm692A9(struct dsi_panel *panel, bool send_cmd)
{

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	pr_info("screen_mode %d\n", panel->lge.screen_mode);

	switch (panel->lge.screen_mode) {
	case screen_mode_auto:
		pr_info("preset: %d, red: %d, green: %d, blue: %d\n",
				panel->lge.cm_preset_step, panel->lge.cm_red_step,
				panel->lge.cm_green_step, panel->lge.cm_blue_step);

		panel->lge.sharpness_status = 0x00;
		color_space_mapper(panel, "def");

		if (panel->lge.cm_preset_step == 2 &&
			!(panel->lge.cm_red_step | panel->lge.cm_green_step | panel->lge.cm_blue_step)) {
			panel->lge.move_wp = 0x00;
		} else {
			panel->lge.move_wp = 0x01;
		}

		custom_rgb_mapper(panel,
			panel->lge.cm_preset_step, panel->lge.cm_red_step,
			panel->lge.cm_green_step, panel->lge.cm_blue_step);
		break;
	case screen_mode_sports:
		color_space_mapper(panel, "spo");
		panel->lge.sharpness_status = 0x00;
		panel->lge.move_wp = 0x01;
		break;
	case screen_mode_game:
		color_space_mapper(panel, "gam");
		panel->lge.sharpness_status = 0x00;
		panel->lge.move_wp = 0x01;
		break;
	case screen_mode_expert:
		color_space_mapper(panel, "exp");
		panel->lge.sharpness_status = 0x01;
		panel->lge.dgc_status = 0x01;

		pr_info("saturation: %d, hue: %d, sharpness: %d\n",
				panel->lge.sc_sat_step, panel->lge.sc_hue_step, panel->lge.sc_sha_step);

		if (panel->lge.cm_preset_step == 2 &&
			!(panel->lge.cm_red_step | panel->lge.cm_green_step | panel->lge.cm_blue_step)) {
			panel->lge.move_wp = 0x00;
		} else {
		    panel->lge.move_wp = 0x01;
		}

		/* sharpness will be controlled in control func. */
		screen_tune_mapper(panel, panel->lge.sc_hue_step, panel->lge.sc_sat_step);
		break;
	case screen_mode_cinema:
		color_space_mapper(panel, "cin");
		panel->lge.sharpness_status = 0x00;
		panel->lge.move_wp = 0x01;
		break;
	case screen_mode_photos:
		color_space_mapper(panel, "pho");
		panel->lge.sharpness_status = 0x00;
		panel->lge.move_wp = 0x01;
		break;
	case screen_mode_web:
		color_space_mapper(panel, "web");
		panel->lge.sharpness_status = 0x00;
		panel->lge.move_wp = 0x01;
		break;
	default:
		panel->lge.sharpness_status = 0x00;
		panel->lge.move_wp = 0x00;
		color_space_mapper(panel, "def");
		break;
	}
	mutex_unlock(&panel->panel_lock);

	lge_display_control_store_rm692A9(panel, send_cmd);
    return;
}

static void lge_set_screen_tune_rm692A9(struct dsi_panel *panel)
{
	pr_info("hue_idx=%d, sat_idx=%d, sha_idx=%d\n", panel->lge.sc_hue_step,
				panel->lge.sc_sat_step, panel->lge.sc_sha_step);

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	mutex_lock(&panel->panel_lock);
	color_space_mapper(panel, "exp");
	/* sharpness will be controlled in control func. */
	screen_tune_mapper(panel, panel->lge.sc_hue_step, panel->lge.sc_sat_step);
	mutex_unlock(&panel->panel_lock);

	return;
}

static void lge_set_custom_rgb_rm692A9(struct dsi_panel *panel, bool send_cmd)
{
	pr_info("cm_step=%d, red_idx=%d, green_idx=%d, blue_idx=%d\n",
				panel->lge.cm_preset_step, panel->lge.cm_red_step,
				panel->lge.cm_green_step, panel->lge.cm_blue_step);

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	custom_rgb_mapper(panel, panel->lge.cm_preset_step, panel->lge.cm_red_step,
				panel->lge.cm_green_step, panel->lge.cm_blue_step);
	if (panel->lge.cm_preset_step == 2 &&
	  !(panel->lge.cm_red_step | panel->lge.cm_green_step | panel->lge.cm_blue_step)) {
		panel->lge.move_wp = 0x00;
	} else {
		panel->lge.move_wp = 0x01;
	}
	mutex_unlock(&panel->panel_lock);

	return;
}

static void sharpness_set_rm692A9(struct dsi_panel *panel, int input)
{

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	mutex_lock(&panel->panel_lock);
	if (input)
		panel->lge.sharpness_status = 0x01;
	else
		panel->lge.sharpness_status = 0x00;
	mutex_unlock(&panel->panel_lock);

	lge_display_control_store_rm692A9(panel, true);
}

static void lge_set_video_enhancement_rm692A9(struct dsi_panel *panel, int input)
{
	if(panel == NULL) {
		pr_err("null ptr\n");
		return;
	}

	if(1) {
		pr_err("display bringup\n");
		return;
	}

	mutex_lock(&panel->panel_lock);
	panel->lge.ve_hdr = !!input;
	if (panel->lge.use_ace_ctrl)
//		lge_set_ace_rm692A9(panel, panel->lge.irc_current_state, panel->lge.hdr_mode, panel->lge.ve_hdr);
	mutex_unlock(&panel->panel_lock);
	lge_backlight_device_update_status(panel->bl_config.raw_bd);
}


struct lge_ddic_ops rm692A9_ops = {
	.store_aod_area = NULL,
	.prepare_aod_cmds = NULL,
	.prepare_aod_area = NULL,
	.lge_check_vert_black_line = NULL,
	.lge_check_vert_white_line = NULL,
	.lge_check_vert_line_restore = NULL,
	.lge_bc_dim_set = lge_bc_dim_set_rm692A9,
	.lge_set_therm_dim = NULL,
	.lge_get_brightness_dim = lge_get_brightness_dim_rm692A9,
	.lge_set_brightness_dim = lge_set_brightness_dim_rm692A9,
	/* To Do : Image Quality */
	.hdr_mode_set = hdr_mode_set_rm692A9,
	.lge_set_custom_rgb = lge_set_custom_rgb_rm692A9,
	.lge_display_control_store = lge_display_control_store_rm692A9,
	.lge_set_screen_tune = lge_set_screen_tune_rm692A9,
	.lge_set_screen_mode = lge_set_screen_mode_rm692A9,
	.sharpness_set = sharpness_set_rm692A9,
	.lge_set_true_view_mode = NULL,
	.lge_set_video_enhancement = lge_set_video_enhancement_rm692A9,
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
