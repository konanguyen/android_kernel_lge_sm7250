#define pr_fmt(fmt)	"[Display][ft8756-ops:%s:%d] " fmt, __func__, __LINE__

#include "dsi_panel.h"
#include "lge_ddic_ops_helper.h"
#include "cm/lge_color_manager.h"
#include "brightness/lge_brightness_def.h"
#include "lge_dsi_panel.h"

extern int lge_ddic_dsi_panel_tx_cmd_set(struct dsi_panel *panel,
				enum lge_ddic_dsi_cmd_set_type type);
extern int lge_mdss_dsi_panel_cmd_read(struct dsi_panel *panel,
					u8 cmd, int cnt, char* ret_buf);
extern char* get_payload_addr(struct dsi_panel *panel, enum lge_ddic_dsi_cmd_set_type type, int position);
extern int get_payload_cnt(struct dsi_panel *panel, enum lge_ddic_dsi_cmd_set_type type, int position);
extern int lge_backlight_device_update_status(struct backlight_device *bd);

const struct drs_res_info ft8756_res[3] = {
	{"fhd", 0, 1080, 2400},
};

#define NUM_PRE_CTRL 5
#define OFFSET_PRE_CTRL 1
#define REG_PRE_CTRL 0x94
#define PAYLOAD_PRE 0

#define NUM_SAT_CTRL 5
#define OFFSET_SAT_CTRL 12
#define REG_SAT_CTRL1 0xD6
#define REG_SAT_CTRL2 0xD7
#define PAYLOAD_SAT 4
#define LINE_SAT_CTRL 11

#define NUM_HUE_CTRL 5
#define OFFSET_HUE_CTRL 12
#define REG_HUE_CTRL1 0xD6
#define REG_HUE_CTRL2 0xD7
#define PAYLOAD_HUE 4
#define LINE_HUE_CTRL 11

#define NUM_SHA_CTRL 5
#define OFFSET_SHA_CTRL 2
#define REG_SHA_CTRL 0x90
#define PAYLOAD_SHA 0

static char pre_ctrl_values[NUM_PRE_CTRL][OFFSET_PRE_CTRL] = {
	{0xA4},
	{0x91},
	{0x80},
	{0xDA},
	{0xFF}
};

static char sat_ctrl_values[NUM_SAT_CTRL][OFFSET_SAT_CTRL] = {
	{0x6D, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D, 0x6D},
	{0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76, 0x76},
	{0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F},
	{0x8E, 0x8E, 0x8E, 0x8E, 0x8E, 0x8E, 0x8E, 0x8E, 0x8E, 0x8E, 0x8E, 0x8E},
	{0x9D, 0x9D, 0x9D, 0x9D, 0x9D, 0x9D, 0x9D, 0x9D, 0x9D, 0x9D, 0x9D, 0x9D}
};

static char hue_ctrl_values[NUM_HUE_CTRL][OFFSET_HUE_CTRL] = {
	{0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73},
	{0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79},
	{0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F},
	{0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85},
	{0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A}
};

static char sha_ctrl_values[NUM_SHA_CTRL][OFFSET_SHA_CTRL] = {
	{0x80, 0x00},
	{0x80, 0x33},
	{0x80, 0x55},
	{0x80, 0x80},
	{0x80, 0xA0}
};

static void lge_set_preset_ft8756(struct dsi_panel *panel, bool send_cmd)
{
	int i;
	int pre_index = 0;

	char *pre_payload = NULL;

	pr_err("lge_set_preset_ft8756[S] send_cmd = %d\n", send_cmd);

	mutex_lock(&panel->panel_lock);

	pre_payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_PRESET, PAYLOAD_PRE);

	if (!pre_payload) {
		pr_err("LGE_DDIC_DSI_SET_PRESET is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	pre_index = panel->lge.cm_preset_step;

	// PRESET CTRL
	for (i = 0; i < OFFSET_PRE_CTRL; i++) {
		pre_payload[i+1] = pre_ctrl_values[pre_index][i];
	}

	for (i = 0; i < OFFSET_PRE_CTRL; i++) {
		pr_info("PRE Reg:0x%02x [%d:0x%02x]\n", pre_payload[0], i+1, pre_payload[i+1]);
	}

	if (send_cmd) {
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_PRESET);
	}

	mutex_unlock(&panel->panel_lock);
	return;
}

static void lge_display_control_store_ft8756(struct dsi_panel *panel, bool send_cmd)
{
	char *cm_payload = NULL;
	char *pre_payload = NULL;

	if(!panel) {
		pr_err("panel not exist\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	cm_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1, 0);
	pre_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1, 1);

	if (!cm_payload) {
		pr_err("LGE_DDIC_DSI_DISP_CTRL_COMMAND cm_payload is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	if (!pre_payload) {
		pr_err("LGE_DDIC_DSI_DISP_CTRL_COMMAND pre_payload is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	cm_payload[1] &= 0x00;
	pre_payload[1] &= 0x00;

	/* CM_EN */
	cm_payload[1] |= panel->lge.color_manager_status << 7;
	/* PRE_EN */
	pre_payload[1] |= panel->lge.dgc_status << 7;

	pr_info("ctrl-command-1(CM): 0x%02x", cm_payload[1]);
	pr_info("ctrl-command-1(PRESET): 0x%02x", pre_payload[1]);

	if (send_cmd) {
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1);
	}

	mutex_unlock(&panel->panel_lock);
	return;
}

static void lge_set_screen_tune_ft8756(struct dsi_panel *panel)
{
	int i, i_pos;
	int sat_index = 0;
	int hue_index = 0;
	int sha_index = 0;

	char *sat_payload = NULL;
	char *hue_payload = NULL;
	char *sha_payload = NULL;

	mutex_lock(&panel->panel_lock);

	// SATURATION TUNE
	for (i_pos = PAYLOAD_SAT; i_pos < LINE_SAT_CTRL; i_pos += 2) {
		sat_payload = NULL;

		sat_payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_SATURATION, i_pos);

		if (!sat_payload) {
			pr_err("LGE_DDIC_DSI_SET_SATURATION is NULL\n");
			mutex_unlock(&panel->panel_lock);
			return;
		}

		sat_index = panel->lge.sc_sat_step;

		// SATURATION CTRL
		for (i = 0; i < OFFSET_SAT_CTRL; i++) {
			sat_payload[i+1] = sat_ctrl_values[sat_index][i];
		}

		for (i = 0; i < OFFSET_SAT_CTRL; i++) {
			pr_debug("SAT(pos %d) Reg:0x%02x [%d:0x%02x]\n", i_pos, sat_payload[0], i+1, sat_payload[i+1]);
		}
	}

	// HUE TUNE
	for (i_pos = PAYLOAD_HUE; i_pos < LINE_HUE_CTRL; i_pos += 2) {
		hue_payload = NULL;

		hue_payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_HUE, i_pos);

		if (!hue_payload) {
			pr_err("LGE_DDIC_DSI_SET_HUE is NULL\n");
			mutex_unlock(&panel->panel_lock);
			return;
		}

		hue_index = panel->lge.sc_hue_step;

		// HUE CTRL
		for (i = 0; i < OFFSET_HUE_CTRL; i++) {
			hue_payload[i+1] = hue_ctrl_values[hue_index][i];
		}

		for (i = 0; i < OFFSET_HUE_CTRL; i++) {
			pr_debug("HUE(pos %d) Reg:0x%02x [%d:0x%02x]\n", i_pos, hue_payload[0], i+1, hue_payload[i+1]);
		}
	}

	// SHARPNESS TUNE
	sha_payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_SHARPNESS, PAYLOAD_SHA);

	if (!sha_payload) {
		pr_err("LGE_DDIC_DSI_SET_SHARPNESS is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	sha_index = panel->lge.sc_sha_step;

	// SHARPNESS CTRL
	for (i = 0; i < OFFSET_SHA_CTRL; i++) {
		sha_payload[i+1] = sha_ctrl_values[sha_index][i];
	}

	for (i = 0; i < OFFSET_SHA_CTRL; i++) {
		pr_debug("SHA Reg:0x%02x [%d:0x%02x]\n", sha_payload[0], i+1, sha_payload[i+1]);
	}

	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SATURATION);
	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_HUE);
	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SHARPNESS);

	mutex_unlock(&panel->panel_lock);
	return;
}

static void lge_set_screen_mode_ft8756(struct dsi_panel *panel, bool send_cmd)
{
	mutex_lock(&panel->panel_lock);

	pr_info("screen_mode %d\n", panel->lge.screen_mode);

	switch (panel->lge.screen_mode) {
	case screen_mode_oled_natural:
		panel->lge.color_manager_status= 0x01;
		pr_info("natural mode\n");
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_CM_NATURAL);
		mutex_unlock(&panel->panel_lock);
		lge_set_preset_ft8756(panel, send_cmd);
		mutex_lock(&panel->panel_lock);
		break;
	case screen_mode_oled_vivid:
		panel->lge.color_manager_status= 0x01;
		pr_info("vivid mode\n");
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_CM_VIVID);
		break;
	case screen_mode_oled_custom:
		pr_info("preset: %d, red: %d, green: %d, blue: %d\n",
				panel->lge.cm_preset_step, panel->lge.cm_red_step,
				panel->lge.cm_green_step, panel->lge.cm_blue_step);
		pr_info("saturation: %d, hue: %d, sharpness: %d\n",
				panel->lge.sc_sat_step, panel->lge.sc_hue_step, panel->lge.sc_sha_step);

		panel->lge.color_manager_status= 0x01;

		//lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SATURATION_DEFAULT);
		//lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_HUE_DEFAULT);
		//lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SHARPNESS_DEFAULT);

		mutex_unlock(&panel->panel_lock);
		lge_set_screen_tune_ft8756(panel);
		lge_set_preset_ft8756(panel, send_cmd);
		mutex_lock(&panel->panel_lock);
		break;
	default:
		panel->lge.color_manager_status= 0x00;
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SATURATION_DEFAULT);
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_HUE_DEFAULT);
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SHARPNESS_DEFAULT);
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_PRESET_DEFAULT);
		break;
	}

	mutex_unlock(&panel->panel_lock);
	//lge_display_control_store_ft8756(panel, send_cmd);

	return;
}

static void lge_set_video_enhancement_ft8756(struct dsi_panel *panel, int input)
{
	int rc = 0;

	mutex_lock(&panel->panel_lock);

	if (input) {
		rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_VIDEO_ENHANCEMENT_ON);
		if (rc)
			pr_err("failed to send VIDEO_ENHANCEMENT_ON cmd, rc=%d\n", rc);
	}
	else {
		rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_VIDEO_ENHANCEMENT_OFF);
		if (rc)
			pr_err("failed to send VIDEO_ENHANCEMENT_OFF cmd, rc=%d\n", rc);
		mutex_unlock(&panel->panel_lock);
		lge_set_screen_mode_ft8756(panel,true);
		mutex_lock(&panel->panel_lock);
	}
	mutex_unlock(&panel->panel_lock);

	pr_info("send cmds to %s the video enhancer \n",
		(input == true) ? "enable" : "disable");
}

static int lge_hdr_mode_set_ft8756(struct dsi_panel *panel, int input)
{
	bool hdr_mode = ((input > 0) ? true : false);

	mutex_lock(&panel->panel_lock);
	if (hdr_mode) {
		panel->lge.color_manager_status = 0x00;
		panel->lge.dgc_status = 0x00;
	} else {
		panel->lge.color_manager_status = 0x01;
		panel->lge.dgc_status = 0x01;
	}
	mutex_unlock(&panel->panel_lock);
	pr_info("hdr=%s, cm=%s preset=%s\n", (hdr_mode ? "set" : "unset"),
			((panel->lge.color_manager_status == 1) ? "enabled" : "disabled"),
			((panel->lge.dgc_status == 1) ? "enabled" : "disabled"));

	if (hdr_mode) {
		lge_display_control_store_ft8756(panel, true);
	} else {
		lge_set_screen_mode_ft8756(panel, true);
	}
	return 0;
}

struct lge_ddic_ops ft8756_ops = {
	/* aod */
	.store_aod_area = NULL,
	.prepare_aod_cmds = NULL,
	.prepare_aod_area = NULL,
	.lge_check_vert_black_line = NULL,
	.lge_check_vert_white_line = NULL,
	.lge_check_vert_line_restore = NULL,
	/* brightness */
	.lge_bc_dim_set = NULL,
	.lge_set_therm_dim = NULL,
	.lge_get_brightness_dim = NULL,
	.lge_set_brightness_dim = NULL,
	/* image quality */
	.hdr_mode_set = lge_hdr_mode_set_ft8756,
	.lge_set_custom_rgb = lge_set_preset_ft8756,
	.lge_display_control_store = lge_display_control_store_ft8756,
	.lge_set_screen_tune = lge_set_screen_tune_ft8756,
	.lge_set_screen_mode = lge_set_screen_mode_ft8756,
	.sharpness_set = NULL,
	.lge_set_true_view_mode = NULL,
	.lge_set_video_enhancement = lge_set_video_enhancement_ft8756,
	.lge_vr_lp_mode_set = NULL,
	.lge_set_tc_perf = NULL,
	/* drs */
	.get_current_res = NULL,
	.get_support_res = NULL,
	/* bist */
	.bist_ctrl = NULL,
	.release_bist = NULL,
	/* error detect */
	.err_detect_work = NULL,
	.err_detect_irq_handler = NULL,
	.set_err_detect_mask = NULL,
	/* pps */
	.set_pps_cmds = NULL,
	.unset_pps_cmds = NULL,
	/* irc */
	.set_irc_default_state = NULL,
	.set_irc_state = NULL,
	.get_irc_state = NULL,
	/* lhbm */
	.lge_set_fp_lhbm = NULL,
};
