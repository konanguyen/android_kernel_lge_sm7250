#define pr_fmt(fmt)	"[Display][lge-dsi-panel:%s:%d] " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include <linux/lge_panel_notify.h>
#include <soc/qcom/lge/board_lge.h>

#include "msm_drv.h"
#include "dsi_panel.h"
#include "dsi_ctrl_hw.h"
#include "dsi_display.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include <drm/drm_mode.h>
#include "dsi_drm.h"
#include "drs/lge_drs_mngr.h"
#include "lge_ddic_ops_helper.h"
#include "brightness/lge_brightness_def.h"
#include "cm/lge_color_manager.h"
#include "factory/lge_factory.h"
#include "err_detect/lge_err_detect.h"
#if defined(CONFIG_MFD_DW8768)
#include <linux/mfd/dw8768.h>
#endif
#if defined(CONFIG_DSV_SM5109)
#include <linux/mfd/sm5109.h>
#endif

#ifdef CONFIG_MACH_LITO_WINGLM
#include <soc/qcom/lge/lge_handle_panic.h>
#endif

extern bool lge_get_factory_boot(void);
extern int lge_panel_notifier_call_chain(unsigned long val, int display_id, int state);
extern int mdss_dsi_parse_color_manager_modes(struct device_node *np,
			struct lge_dsi_color_manager_mode_entry color_manager_table[NUM_COLOR_MODES],
			u32 *color_manager_modes_num,
			const char *name);
extern void lge_bc_dim_work_init(struct dsi_panel *panel);
int dsi_panel_reset(struct dsi_panel *panel);

extern int lge_get_mfts_mode(void);
extern int dsi_panel_set_pinctrl_state(struct dsi_panel *panel, bool enable);
extern int dsi_panel_tx_cmd_set(struct dsi_panel *panel, enum dsi_cmd_set_type type);
extern int dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt);
extern int dsi_panel_create_cmd_packets(const char *data,
					u32 length,
					u32 count,
					struct dsi_cmd_desc *cmd);
extern int dsi_panel_alloc_cmd_packets(struct dsi_panel_cmd_set *cmd,
					u32 packet_count);

extern void lge_dsi_panel_blmap_free(struct dsi_panel *panel);
extern int lge_dsi_panel_parse_blmap(struct dsi_panel *panel, struct device_node *of_node);
extern int lge_dsi_panel_parse_brightness(struct dsi_panel *panel,	struct device_node *of_node);
extern void lge_panel_drs_create_sysfs(struct dsi_panel *panel, struct device *panel_sysfs_dev);
extern void lge_panel_reg_create_sysfs(struct dsi_panel *panel, struct device *panel_sysfs_dev);
extern void lge_ddic_ops_init(struct dsi_panel *panel);
extern void lge_ddic_feature_init(struct dsi_panel *panel);
extern int lge_ddic_dsi_panel_parse_cmd_sets(struct dsi_panel *panel, struct device_node *of_node);
extern int lge_update_backlight_ex(struct dsi_panel *panel);
extern void lge_ambient_create_sysfs(struct dsi_panel *panel, struct device *panel_sysfs_dev);
extern int lge_drs_mngr_init(struct dsi_panel *panel);
extern int lge_mdss_dsi_panel_cmd_read(struct dsi_panel *panel,
					u8 cmd, int cnt, char* ret_buf);
extern int lge_mdss_dsi_panel_cmds_backup(struct dsi_panel *panel, char *owner,
				enum dsi_cmd_set_type type, u8 reg, int nth_cmds);
static int lge_dsi_panel_pin_seq(struct lge_panel_pin_seq *seq);
extern int lge_ddic_dsi_panel_tx_cmd_set(struct dsi_panel *panel,
				enum lge_ddic_dsi_cmd_set_type type);
extern int lge_brightness_create_sysfs(struct dsi_panel *panel, struct device *panel_sysfs_dev);
extern int lge_ambient_set_interface_data(struct dsi_panel *panel);
extern int lge_panel_factory_create_sysfs(struct dsi_panel *panel, struct device *panel_sysfs_dev);
extern int lge_ddic_dsi_panel_parse_cm_lut_cmd_sets(struct dsi_panel *panel,
				struct device_node *of_node);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT)
/*---------------------------------------------------------------------------*/
/* LCD off & dimming                                                         */
/*---------------------------------------------------------------------------*/
static bool fb_blank_called;

bool is_blank_called(void)
{
	return fb_blank_called;
}

int lge_get_bootreason_with_lcd_dimming(void)
{
	int ret = 0;

	if (lge_get_bootreason() == 0x23)
		ret = 1;
	else if (lge_get_bootreason() == 0x24)
		ret = 2;
	else if (lge_get_bootreason() == 0x25)
		ret = 3;
	return ret;
}

bool is_factory_cable(void)
{
	cable_boot_type cable_info = lge_get_boot_cable();

	if (cable_info == LT_CABLE_56K ||
		cable_info == LT_CABLE_130K ||
		cable_info == LT_CABLE_910K)
		return true;
	return false;
}

static void lge_set_blank_called(void)
{
	fb_blank_called = true;
}
#endif

static int lge_dsi_panel_mode_set(struct dsi_panel *panel)
{
	bool reg_backup_cond = false;
	int tc_perf_status = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->lge.ecc_status && panel->lge.ddic_ops && panel->lge.ddic_ops->lge_set_ecc_status)
		panel->lge.ddic_ops->lge_set_ecc_status(panel, panel->lge.ecc_status);

	reg_backup_cond = !(panel->lge.use_ddic_reg_backup^panel->lge.ddic_reg_backup_complete);

	pr_info("backup=%d\n", reg_backup_cond);
	if (reg_backup_cond && panel->lge.use_color_manager) {
		if (panel->lge.ddic_ops &&
				panel->lge.ddic_ops->lge_set_screen_mode)
			panel->lge.ddic_ops->lge_set_screen_mode(panel, true);

		if (panel->lge.ddic_ops &&
				panel->lge.ddic_ops->lge_bc_dim_set)
			panel->lge.ddic_ops->lge_bc_dim_set(panel, BC_DIM_ON, BC_DIM_FRAMES_NORMAL);
		panel->lge.is_sent_bc_dim_set = true;
	} else {
		pr_warn("skip ddic mode set on booting or not supported!\n");
	}

	if (panel->lge.use_tc_perf) {
		tc_perf_status = panel->lge.tc_perf;
		if (tc_perf_status && panel->lge.ddic_ops &&
				panel->lge.ddic_ops->lge_set_tc_perf) {
			panel->lge.ddic_ops->lge_set_tc_perf(panel, tc_perf_status);
		}
	}

	return 0;
}

static inline bool cmd_set_exists(struct dsi_panel *panel, enum lge_ddic_dsi_cmd_set_type type)
{
	if (!panel || !panel->cur_mode)
		return false;
	return (panel->lge.lge_cmd_sets[type].count != 0);
}

char* get_ddic_name(void *disp)
{
	struct dsi_panel *panel = NULL;
	struct dsi_display *display = disp;

	if (!display) {
		return NULL;
	}

	panel = display->panel;

	if (!panel) {
		return NULL;
	}

	return panel->lge.ddic_name;
}

bool is_ddic_name_matched(struct dsi_panel *panel, char *ddic_name)
{
	if (panel == NULL || ddic_name == NULL) {
		pr_err("input parameter is NULL\n");
		return false;
	}

	if(!strcmp(panel->lge.ddic_name, ddic_name)) {
		pr_err("ddic is matched : input ddic_name = %s, lge_ddic = %s\n", ddic_name, panel->lge.ddic_name);
		return true;
	}
	pr_err("ddic is not matched : input ddic_name = %s, lge_ddic = %s\n", ddic_name, panel->lge.ddic_name);

	return false;
}

bool is_ddic_name(uint32_t display_idx, char *ddic_name)
{
	struct dsi_display *display_prim = primary_display;
	struct dsi_display *display_sec = secondary_display;
	struct dsi_panel *panel;

	if (ddic_name == NULL) {
		pr_err("input parameter is NULL\n");
		return false;
	}

	if (display_idx == 0 && display_prim) {
		panel = display_prim->panel;

		if (panel && !strcmp(panel->lge.ddic_name, ddic_name)) {
			pr_info("primary ddic is matched : input ddic_name = %s, lge_ddic = %s\n", ddic_name, panel->lge.ddic_name);
			return true;
		}
		pr_err("primary ddic is not matched\n");
	} else if (display_idx == 1 && display_sec) {
		panel = display_sec->panel;

		if (panel && !strcmp(panel->lge.ddic_name, ddic_name)) {
			pr_info("secondary ddic is matched : input ddic_name = %s, lge_ddic = %s\n", ddic_name, panel->lge.ddic_name);
			return true;
		}
		pr_err("secondary ddic is not matched\n");
	}

	return false;
}
EXPORT_SYMBOL(is_ddic_name);

int dsi_panel_full_power_seq(struct dsi_panel *panel)
{
	int ret = 0;

	if (!panel->lge.is_incell || lge_get_mfts_mode() || panel->lge.panel_dead)
		ret = 1;

	if (panel->lge.mfts_auto_touch)
		ret = 0;

	return ret;
}

static const char *LPNAME[] = { "NOLP", "LP1", "LP2", "OFF", "MAX"};
static bool dsi_panel_need_mask(struct dsi_panel *panel)
{
	if (panel->lge.partial_area_vertical_changed)
		return true;

	if (panel->lge.partial_area_height_changed &&
			panel->lge.update_pps_in_lp)
		return true;

	return false;
}

static void set_aod_area(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->prepare_aod_area) {
		panel->lge.ddic_ops->prepare_aod_area(panel,
				panel->lge.lge_cmd_sets[LGE_DDIC_DSI_AOD_AREA].cmds,
				panel->lge.lge_cmd_sets[LGE_DDIC_DSI_AOD_AREA].count);
	}

	rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_AOD_AREA);
	if (rc) {
		pr_err("[%s] failed to send cmd, rc=%d\n",
		       panel->name, rc);
	}

	return;
}

static void set_lp(struct dsi_panel *panel, enum lge_panel_lp_state lp_state, enum lge_ddic_dsi_cmd_set_type cmd_set_type)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	if (panel->lge.panel_state == lp_state) {
		pr_err("already %s state\n", LPNAME[lp_state]);
		return;
	}

	if (!cmd_set_exists(panel, cmd_set_type)) {
		pr_err("No %s cmd\n", LPNAME[lp_state]);
		return;
	}

	rc = lge_ddic_dsi_panel_tx_cmd_set(panel, cmd_set_type);
	if (rc) {
		pr_err("[%s] failed to send %d cmd, rc=%d\n",
		       panel->name, cmd_set_type, rc);
	} else {
		panel->lge.panel_state = lp_state;
		pr_info("sent %s cmd\n", LPNAME[lp_state]);
	}

	return;
}

static int dsi_panel_get_current_power_mode(struct dsi_panel *panel)
{
	struct dsi_display *display = NULL;
	struct sde_connector *conn = NULL;
	int mode;

	if (!panel) {
		pr_err("invalid panel param\n");
		return -EINVAL;
	}

	display = container_of(panel->host, struct dsi_display, host);
	if (!display) {
		pr_err("invalid display param\n");
		return -EINVAL;
	}

	conn = to_sde_connector(display->drm_conn);
	if (!conn)
		return -EINVAL;

	switch (conn->dpms_mode) {
	case DRM_MODE_DPMS_ON:
		mode = conn->lp_mode;
		break;
	case DRM_MODE_DPMS_STANDBY:
		mode = SDE_MODE_DPMS_STANDBY;
		break;
	case DRM_MODE_DPMS_SUSPEND:
		mode = SDE_MODE_DPMS_SUSPEND;
		break;
	case DRM_MODE_DPMS_OFF:
		mode = SDE_MODE_DPMS_OFF;
		break;
	default:
		mode = conn->lp_mode;
		pr_err("unrecognized mode=%d\n", mode);
		break;
	}

	return mode;
}

static int dsi_panel_get_last_power_mode(struct dsi_panel *panel)
{
	struct dsi_display *display = NULL;
	struct sde_connector *conn = NULL;

	if (!panel) {
		pr_err("invalid panel param\n");
		return -EINVAL;
	}

	display = container_of(panel->host, struct dsi_display, host);
	if (!display) {
		pr_err("invalid display param\n");
		return -EINVAL;
	}

	conn = to_sde_connector(display->drm_conn);
	if (!conn)
		return -EINVAL;

	return conn->last_panel_power_mode;
}

static inline bool is_power_off(int pwr_mode)
{
	return (pwr_mode == SDE_MODE_DPMS_OFF);
}

static inline bool is_power_on_interactive(int pwr_mode)
{
	return (pwr_mode == SDE_MODE_DPMS_ON);
}

static inline bool is_power_on(int pwr_mode)
{
	return !is_power_off(pwr_mode);
}

static inline bool is_power_on_lp(int pwr_mode)
{
	return !is_power_off(pwr_mode) &&
		!is_power_on_interactive(pwr_mode);
}

static inline bool is_power_on_ulp(int pwr_mode)
{
	return (pwr_mode == SDE_MODE_DPMS_LP2);
}

bool lge_dsi_panel_is_power_off(struct dsi_panel *panel)
{
	int last_panel_power_mode = dsi_panel_get_current_power_mode(panel);
	return is_power_off(last_panel_power_mode);
}

bool lge_dsi_panel_is_power_on_interactive(struct dsi_panel *panel)
{
	int last_panel_power_mode = dsi_panel_get_current_power_mode(panel);
	return is_power_on_interactive(last_panel_power_mode);
}

bool lge_dsi_panel_is_power_on(struct dsi_panel *panel)
{
	int last_panel_power_mode = dsi_panel_get_current_power_mode(panel);
	return is_power_on(last_panel_power_mode);
}

bool lge_dsi_panel_is_power_on_lp(struct dsi_panel *panel)
{
	int last_panel_power_mode = dsi_panel_get_current_power_mode(panel);
	return is_power_on_lp(last_panel_power_mode);
}

bool lge_dsi_panel_is_power_on_ulp(struct dsi_panel *panel)
{
	int last_panel_power_mode = dsi_panel_get_current_power_mode(panel);
	return is_power_on_ulp(last_panel_power_mode);
}

bool is_need_lhbm_recovery(struct dsi_panel *panel,
				enum lge_ddic_dsi_cmd_set_type cmd_type)
{
	int ret = false;

	if ((panel->lge.use_fp_lhbm) && (panel->lge.ddic_ops)
		&& (panel->lge.ddic_ops->lge_set_fp_lhbm)
		&& ((cmd_type == LGE_DDIC_DSI_SET_LP2)
			|| (cmd_type == LGE_DDIC_DSI_SET_LP1)
			|| (cmd_type == LGE_DDIC_DSI_SET_NOLP))
		&& ((panel->lge.old_fp_lhbm_mode == LGE_FP_LHBM_SM_ON)
			|| (panel->lge.old_fp_lhbm_mode == LGE_FP_LHBM_ON)
			|| (panel->lge.old_fp_lhbm_mode == LGE_FP_LHBM_READY)
			|| (panel->lge.lhbm_ready_enable == true)))
		ret = true;

	return ret;
}


static enum lge_ddic_dsi_cmd_set_type dsi_panel_select_cmd_type(struct dsi_panel *panel)
{
	int rc;
	int last_panel_power_mode;
	enum lge_ddic_dsi_cmd_set_type type = LGE_DDIC_DSI_CMD_SET_MAX;

	last_panel_power_mode = dsi_panel_get_last_power_mode(panel);
	if (last_panel_power_mode < 0) {
		pr_err("fail to get last_panel_pwr_mode\n");
		goto exit;
	}
	mutex_lock(&panel->lge.pa_changed_lock);
	if (!is_power_on_lp(last_panel_power_mode)) {
		if (is_power_off(last_panel_power_mode) &&
				panel->lge.use_cmd_wait_pa_changed &&
				((panel->lge.aod_area.w == 0) ||
				(panel->lge.aod_area.h == 0))) {
			init_completion(&panel->lge.pa_changed_done);
			panel->lge.wait_pa_changed = true;
			mutex_unlock(&panel->lge.pa_changed_lock);
			rc = wait_for_completion_timeout(&panel->lge.pa_changed_done, 2000);
			if (rc <= 0) {
				pr_warn("aod image will be displayed by default setting\n");
			}
			mutex_lock(&panel->lge.pa_changed_lock);
		}
		type = LGE_DDIC_DSI_SET_LP2;
	} else if (panel->lge.partial_area_vertical_changed ||
			panel->lge.partial_area_height_changed) {
		type = LGE_DDIC_DSI_AOD_AREA;
	} else {
		pr_debug("skip command\n");
	}
	mutex_unlock(&panel->lge.pa_changed_lock);

exit:
	pr_info("select_cmd=%d\n", type);
	return type;
}

static int dsi_panel_send_lp_cmds(struct dsi_panel *panel,
				enum lge_ddic_dsi_cmd_set_type cmd_type)
{
	int rc = 0;
	bool need_mask = true;
	bool need_prepare = true;
	char *bist_name;
	enum lge_panel_lp_state panel_state = LGE_PANEL_STATE_MAX;

	if (!panel)
		return -EINVAL;

	if (is_need_lhbm_recovery(panel, cmd_type)) {
		panel->lge.fp_lhbm_mode = LGE_FP_LHBM_EXIT;
		panel->lge.ddic_ops->lge_set_fp_lhbm(panel, panel->lge.fp_lhbm_mode);
	}

	switch (cmd_type) {
	case LGE_DDIC_DSI_SET_LP1:
		bist_name = "lp1";
		panel_state = LGE_PANEL_LP1;
		break;
	case LGE_DDIC_DSI_SET_LP2:
		bist_name = "lp2";
		panel_state = LGE_PANEL_LP2;
		break;
	case LGE_DDIC_DSI_SET_NOLP:
		bist_name = "nolp";
		panel_state = LGE_PANEL_NOLP;
		need_prepare = false;
		break;
	case LGE_DDIC_DSI_AOD_AREA:
		bist_name ="aod_area";
		break;
	default:
		bist_name = "none";
		break;
	};

	mutex_lock(&panel->lge.pa_changed_lock);

	if (cmd_type == LGE_DDIC_DSI_CMD_SET_MAX)
		goto exit;

	need_mask = dsi_panel_need_mask(panel);

	/* 1. masking */
	if (is_bist_supported(panel, bist_name) && need_mask &&
			(panel->lge.ddic_ops && panel->lge.ddic_ops->bist_ctrl)) {
		mutex_lock(&panel->lge.bist_lock);
		if (panel->lge.ddic_ops->bist_ctrl(panel, true) < 0)
			pr_err("fail to control BIST\n");
		mutex_unlock(&panel->lge.bist_lock);
	}

	/* 2. send lp command */
	mutex_lock(&panel->panel_lock);

	/* UNLOCK REGISTER */
	if (panel->lge.use_ddic_reg_lock)
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_REGISTER_UNLOCK);

	if (panel->cur_mode && need_prepare) {
		set_aod_area(panel);

		if (cmd_type != LGE_DDIC_DSI_AOD_AREA) {
			if (panel->lge.ddic_ops && panel->lge.ddic_ops->prepare_aod_cmds) {
				panel->lge.ddic_ops->prepare_aod_cmds(panel,
						panel->lge.lge_cmd_sets[cmd_type].cmds,
						panel->lge.lge_cmd_sets[cmd_type].count);
			}
		}
	}

	if(cmd_type != LGE_DDIC_DSI_AOD_AREA)
		set_lp(panel, panel_state, cmd_type);

	/* LOCK REGISTER */
	if (panel->lge.use_ddic_reg_lock)
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_REGISTER_LOCK);

	mutex_unlock(&panel->panel_lock);
	/* 3. update pps */
	if (panel->lge.update_pps_in_lp) {
		if (panel->lge.ddic_ops && panel->lge.ddic_ops->set_pps_cmds) {
			rc = panel->lge.ddic_ops->set_pps_cmds(panel, cmd_type);
			if (rc) {
				pr_warn("WARNING: fail to update pps info\n");
			}
		}

		rc = dsi_panel_update_pps(panel);
		if (rc)
			pr_err("fail to send pps\n");

		if (panel->lge.ddic_ops && panel->lge.ddic_ops->unset_pps_cmds) {
			rc = panel->lge.ddic_ops->unset_pps_cmds(panel, cmd_type);
			if (rc) {
				pr_warn("WARNING: fail to unset pps info\n");
			}
		}
	}

	/* 4. un-masking */
	if ((is_bist_supported(panel, bist_name)) && need_mask &&
			(panel->lge.ddic_ops && panel->lge.ddic_ops->bist_ctrl)) {
		mutex_lock(&panel->lge.bist_lock);
		if (panel->lge.ddic_ops->bist_ctrl(panel, false) < 0)
			pr_err("fail to control BIST\n");
		mutex_unlock(&panel->lge.bist_lock);
	}

	panel->lge.partial_area_vertical_changed = false;
	panel->lge.partial_area_height_changed = false;

	if (panel->lge.forced_lhbm == true && panel->lge.ddic_ops && panel->lge.ddic_ops->lge_set_fp_lhbm)
		panel->lge.ddic_ops->lge_set_fp_lhbm(panel, panel->lge.fp_lhbm_mode);

exit:
	mutex_unlock(&panel->lge.pa_changed_lock);

	return rc;
}

static int dsi_panel_update_lp_state(struct dsi_panel *panel, enum lge_panel_lp_state new)
{
	if (!panel)
		return -EINVAL;

	panel->lge.lp_state = new;
	return 0;
}

/* @Override */
int dsi_panel_set_lp2(struct dsi_panel *panel)
{
	int rc;
	enum lge_ddic_dsi_cmd_set_type cmd_type;

	if (!panel)
		return -EINVAL;

	cmd_type = dsi_panel_select_cmd_type(panel);

	rc = dsi_panel_send_lp_cmds(panel, cmd_type);
	if (rc < 0) {
		pr_err("fail to send lp command\n");
	}

	if ((cmd_type == LGE_DDIC_DSI_CMD_SET_MAX) || (cmd_type == LGE_DDIC_DSI_AOD_AREA))
		panel->lge.panel_state = LGE_PANEL_LP2;
	rc = dsi_panel_update_lp_state(panel, LGE_PANEL_LP2);
	if (rc < 0) {
		pr_err("fail to update lp state\n");
	} else {
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_BLANK,
				panel->lge.display_id, LGE_PANEL_STATE_LP2); /* U2_UNBLANK; DOZE */
	}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT)
	lge_set_blank_called();
#endif
	return rc;
}

/* @Override */
int dsi_panel_set_lp1(struct dsi_panel *panel)
{
	int rc;
	enum lge_ddic_dsi_cmd_set_type cmd_type;

	if (!panel)
		return -EINVAL;

	cmd_type = dsi_panel_select_cmd_type(panel);

	rc = dsi_panel_send_lp_cmds(panel, cmd_type);
	if (rc < 0) {
		pr_err("fail to send lp command\n");
	}

	if ((cmd_type == LGE_DDIC_DSI_CMD_SET_MAX) || (cmd_type == LGE_DDIC_DSI_AOD_AREA))
		panel->lge.panel_state = LGE_PANEL_LP1;
	rc = dsi_panel_update_lp_state(panel, LGE_PANEL_LP1);
	if (rc < 0) {
		pr_err("update lp state\n");
	} else {
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_BLANK,
				panel->lge.display_id, LGE_PANEL_STATE_LP1); /* U2_BLANK; DOZE_SUSPEND */
	}
#if IS_ENABLED(CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT)
	lge_set_blank_called();
#endif
	return rc;
}

/* @Override */
int dsi_panel_set_nolp(struct dsi_panel *panel)
{
	int rc;
	struct dsi_display *display = NULL;
	struct sde_connector *conn = NULL;
	int lp_mode, last_panel_power_mode;

	if (!panel)
		return -EINVAL;

	display = container_of(panel->host, struct dsi_display, host);
	conn = to_sde_connector(display->drm_conn);
	lp_mode = conn->lp_mode;
	last_panel_power_mode = conn->last_panel_power_mode;

	if (lp_mode == SDE_MODE_DPMS_ON && last_panel_power_mode == SDE_MODE_DPMS_OFF) {
		panel->lge.panel_state = LGE_PANEL_NOLP;
		rc = dsi_panel_update_lp_state(panel, LGE_PANEL_NOLP);
		pr_info("going to on from off\n");
		goto mode_set;
	} else if (lp_mode == SDE_MODE_DPMS_OFF) {
		panel->lge.panel_state = LGE_PANEL_OFF;
		rc = dsi_panel_update_lp_state(panel, LGE_PANEL_OFF);
		pr_info("going to off\n");
		return 0;
	}

	rc = dsi_panel_send_lp_cmds(panel, LGE_DDIC_DSI_SET_NOLP);
	if (rc < 0) {
		pr_err("fail to send lp command\n");
	}

	rc = dsi_panel_update_lp_state(panel, LGE_PANEL_NOLP);
	if (rc < 0) {
		pr_err("fail to update lp state\n");
	}

	lge_panel_notifier_call_chain(LGE_PANEL_EVENT_BLANK, panel->lge.display_id, LGE_PANEL_STATE_UNBLANK); // U3, UNBLANK

mode_set:
	lge_dsi_panel_mode_set(panel);

	return rc;
}

#if defined(CONFIG_DSV_SM5109)
static int gpio_ldo_enable(struct dsi_panel *panel, bool enable)
{
	int rc;

	if (enable) {
		usleep_range(2000, 2000);
		if (gpio_is_valid(panel->lge.touch_reset_gpio)) {
			rc = gpio_direction_output(panel->lge.touch_reset_gpio, 1);
			if (rc) {
				DSI_ERR("unable to set dir for touch_reset_gpio gpio rc=%d\n", rc);
				goto exit;
			}
		}
		usleep_range(3000, 3000);
		if (gpio_is_valid(panel->lge.dsv_vpos_gpio_sm5109)) {
			rc = gpio_direction_output(panel->lge.dsv_vpos_gpio_sm5109, 1);
			if (rc) {
				DSI_ERR("unable to set dir for dsv_vpos_gpio_sm5109 gpio rc=%d\n", rc);
				goto exit;
			}
		}
		usleep_range(3000, 3000);
		if (gpio_is_valid(panel->lge.dsv_vneg_gpio_sm5109)) {
			rc = gpio_direction_output(panel->lge.dsv_vneg_gpio_sm5109, 1);
			if (rc) {
				DSI_ERR("unable to set dir for dsv_vneg_gpio_sm5109 gpio rc=%d\n", rc);
				goto exit;
			}
		}
		usleep_range(3000, 3000);
		sm5109_set_output_voltage(0x0F); /*+5.5V, -5.5V*/
		sm5109_register_set(SM5109_DISCHARGE_STATUS_CONTROL_REG, 0x00);
	} else {
		if (gpio_is_valid(panel->lge.touch_reset_gpio)) {
			rc = gpio_direction_output(panel->lge.touch_reset_gpio, 0);
			if (rc) {
				DSI_ERR("unable to set dir for touch_reset_gpio gpio rc=%d\n", rc);
				goto exit;
			}
		}
		usleep_range(3000, 3000);
		if (gpio_is_valid(panel->lge.dsv_vpos_gpio_sm5109)) {
			rc = gpio_direction_output(panel->lge.dsv_vpos_gpio_sm5109, 0);
			if (rc) {
				DSI_ERR("unable to set dir for dsv_vpos_gpio_sm5109 gpio rc=%d\n", rc);
				goto exit;
			}
		}

		if (gpio_is_valid(panel->lge.dsv_vneg_gpio_sm5109)) {
			rc = gpio_direction_output(panel->lge.dsv_vneg_gpio_sm5109, 0);
			if (rc) {
				DSI_ERR("unable to set dir for dsv_vneg_gpio_sm5109 gpio rc=%d\n", rc);
				goto exit;
			}
		}
		usleep_range(3000, 3000);
	}

	exit:
		return rc;
}
#endif

#ifdef CONFIG_MACH_LITO_WINGLM
typedef enum {
    ENABLED_DISPLAY_PMIC_LDO_NONE = 0,
    ENABLED_PRIM_DISPLAY_PIMC_LDO = 1, // BIT(0)
    ENABLED_SEC_DISPLAY_PMIC_LDO  = 2, // BIT(1)
    ENABLED_DISPLAY_PMIC_LDO_MAX  = 4, // BIT(3)
} lge_hw_display_ldo_enabled;
#endif

/* @Override */
int dsi_panel_power_on(struct dsi_panel *panel)
{
	int rc = 0;

	pr_info("[%s] ++\n", panel->name);
	if (dsi_panel_full_power_seq(panel)) {
		if (panel->lge.use_labibb) {
#ifdef CONFIG_MACH_LITO_WINGLM
			if (panel->lge.display_id == DSI_SECONDARY) {
				//set smem display_ldo_enabled
				lge_set_display_ldo_enabled(ENABLED_SEC_DISPLAY_PMIC_LDO);
			}
#endif
			rc = dsi_pwr_enable_regulator(&panel->power_info, true);
			if (rc) {
				pr_err("[%s] failed to enable LABIBB, rc=%d\n", panel->name, rc);
				goto exit;
			}
			pr_info("[%s] Turn on labibb\n", panel->name);
#ifdef CONFIG_MACH_LITO_WINGLM
			if (panel->lge.display_id == DSI_SECONDARY) {
				//unset smem display_ldo_enabled
				lge_set_display_ldo_enabled(ENABLED_DISPLAY_PMIC_LDO_NONE);
			}
#endif
		}

#if defined(CONFIG_DSV_SM5109)
		rc = gpio_ldo_enable(panel, true);
		if(rc) {
			DSI_ERR("[%s] failed to gpio_ldo_enable true, rc=%d\n", panel->name, rc);
		}
#endif
		if (panel->lge.pins) {
			rc = lge_dsi_panel_pin_seq(panel->lge.panel_on_seq);
			if (rc) {
				pr_err("[%s] failed to set lge panel pin, rc=%d\n", panel->name, rc);
				goto error_disable_vregs;
			}
			pr_info("[%s] Turn on vddi/vpnl\n", panel->name);
		}
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_POWER, panel->lge.display_id, LGE_PANEL_POWER_VDDIO_ON); // PANEL VDDIO ON
	} else {
		if (panel->lge.use_panel_reset_low_before_lp11) {
			pr_info("[%s] reset low before LP11\n", panel->name);
			if (gpio_is_valid(panel->reset_config.reset_gpio)) {
				lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RESET, panel->lge.display_id, LGE_PANEL_RESET_LOW); // PANEL RESET LOW
				gpio_set_value(panel->reset_config.reset_gpio, 0);
				usleep_range(5000, 5000);
			}
		} else {
			pr_info("Do not control display powers for Incell display\n");
		}
	}

	goto exit;

error_disable_vregs:
	(void)dsi_pwr_enable_regulator(&panel->power_info, false);

exit:
	pr_info("[%s] --\n", panel->name);
	return rc;
}

static int lge_dsi_panel_pin_seq(struct lge_panel_pin_seq *seq)
{
	int rc = 0;

	if (!seq) {
		return -EINVAL;
	}

	while (gpio_is_valid(seq->gpio)) {
		if (seq->level) {
			rc = gpio_direction_output(seq->gpio, 1);
			if (rc) {
				pr_err("unable to set dir for gpio %d, rc=%d\n", seq->gpio, rc);
				break;
			} else {
				pr_info("gpio %d -> %d\n", seq->gpio, 1);
			}
		} else {
			gpio_set_value(seq->gpio, 0);
			pr_info("gpio %d -> %d\n", seq->gpio, 0);
		}
		usleep_range(seq->sleep_ms*1000, seq->sleep_ms*1000);
		seq++;
	}

	return rc;
}

/* @Override */
int dsi_panel_power_off(struct dsi_panel *panel)
{
	int rc = 0;

	pr_info("[%s] ++\n", panel->name);
	if (gpio_is_valid(panel->reset_config.disp_en_gpio))
		gpio_set_value(panel->reset_config.disp_en_gpio, 0);

	if (gpio_is_valid(panel->reset_config.lcd_mode_sel_gpio))
		gpio_set_value(panel->reset_config.lcd_mode_sel_gpio, 0);

	if (dsi_panel_full_power_seq(panel)) {
		if(panel->lge.pins) {
			pr_info("[%s] Turn off vddi/vpnl\n", panel->name);
			lge_dsi_panel_pin_seq(panel->lge.panel_off_seq); // load
		}

#if defined(CONFIG_DSV_SM5109)
		rc = gpio_ldo_enable(panel, false);
		if(rc) {
			DSI_ERR("[%s] failed to gpio_ldo_enable false, rc=%d\n", panel->name, rc);
		}
#endif
		if (panel->lge.use_labibb) {
			rc = dsi_pwr_enable_regulator(&panel->power_info, false);
			if (rc)
				pr_err("[%s] failed to disable LABIBB, rc=%d\n", panel->name, rc);
			else {
				pr_info("[%s] Turn off labibb\n", panel->name);
				lge_panel_notifier_call_chain(LGE_PANEL_EVENT_POWER, panel->lge.display_id, LGE_PANEL_POWER_VDDIO_OFF); // PANEL VDDIO LOW
			}
		}
	} else {
		pr_info("Do not control LCD powers for LPWG mode");
	}

	if (panel->lge.use_ddic_reg_backup && panel->lge.ddic_reg_backup_complete &&
			panel->lge.is_sent_bc_dim_set)
		panel->lge.is_sent_bc_dim_set = false;
	else if (!panel->lge.use_ddic_reg_backup && panel->lge.is_sent_bc_dim_set)
		panel->lge.is_sent_bc_dim_set = false;

	if (panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl) {
		if (panel->lge.ddic_ops && panel->lge.ddic_ops->bist_ctrl)
			panel->lge.ddic_ops->set_irc_default_state(panel);
		panel->lge.irc_pending = false;
	}

	lge_panel_notifier_call_chain(LGE_PANEL_EVENT_BLANK, panel->lge.display_id, LGE_PANEL_STATE_BLANK); // U0, BLANK
#if IS_ENABLED(CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT)
	lge_set_blank_called();
#endif
	pr_info("[%s] --\n", panel->name);
	return rc;
}

static int dsi_panel_reset_off(struct dsi_panel *panel)
{
	int rc = 0;

	if (lge_get_factory_boot() || lge_get_mfts_mode()) {
		if (panel->lge.use_panel_err_detect && panel->lge.err_detect_irq_enabled)
			lge_panel_err_detect_irq_control(panel, false);
	}
	usleep_range(5000, 5000);

	if (dsi_panel_full_power_seq(panel)) {
		if (panel->lge.use_ext_dsv) {
			if (panel->lge.is_incell && panel->lge.reset_after_ddvd) {
				if(panel->lge.ddic_ops->lge_panel_dsv_ctrl)
					panel->lge.ddic_ops->lge_panel_dsv_ctrl(panel, false);
			}
		}

		if (gpio_is_valid(panel->reset_config.reset_gpio)) {
			pr_info("Set Reset GPIO to Low\n");
			lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RESET, panel->lge.display_id, LGE_PANEL_RESET_LOW); // PANEL RESET LOW
			gpio_set_value(panel->reset_config.reset_gpio, 0);
		}
		usleep_range(5000, 5000);

		rc = dsi_panel_set_pinctrl_state(panel, false);
		if (rc) {
			pr_err("[%s] failed set pinctrl state, rc=%d\n", panel->name, rc);
		}
		usleep_range(5000, 5000);

		if (panel->lge.use_ext_dsv) {
			if (panel->lge.is_incell && !panel->lge.reset_after_ddvd) {
				if(panel->lge.ddic_ops->lge_panel_dsv_ctrl)
					panel->lge.ddic_ops->lge_panel_dsv_ctrl(panel, false);
			}
		}
	}

	return rc;
}

/* @Override */
int dsi_panel_reset(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_panel_reset_config *r_config = &panel->reset_config;
	int i;

	if (gpio_is_valid(panel->reset_config.disp_en_gpio)) {
		rc = gpio_direction_output(panel->reset_config.disp_en_gpio, 1);
		if (rc) {
			pr_err("unable to set dir for disp gpio rc=%d\n", rc);
			goto exit;
		}
	}

	if (panel->lge.use_ext_dsv) {
		if (panel->lge.is_incell && panel->lge.reset_after_ddvd) {
			if(panel->lge.ddic_ops->lge_panel_dsv_ctrl)
				panel->lge.ddic_ops->lge_panel_dsv_ctrl(panel, true);
		}
	}

	if (r_config->count) {
		rc = gpio_direction_output(r_config->reset_gpio,
			r_config->sequence[0].level);
		if (rc) {
			pr_err("unable to set dir for rst gpio rc=%d\n", rc);
			goto exit;
		}
	}

	pr_info("Set Reset GPIO to HIGH\n");
	lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RESET, panel->lge.display_id, LGE_PANEL_RESET_HIGH); // PANEL RESET HIGH
	for (i = 0; i < r_config->count; i++) {
		gpio_set_value(r_config->reset_gpio,
			       r_config->sequence[i].level);

		if (r_config->sequence[i].sleep_ms)
			usleep_range(r_config->sequence[i].sleep_ms * 1000,
				     r_config->sequence[i].sleep_ms * 1000);
	}

	usleep_range(5000, 5000);

	if (panel->lge.use_ext_dsv) {
		if (panel->lge.is_incell && !panel->lge.reset_after_ddvd) {
			if(panel->lge.ddic_ops->lge_panel_dsv_ctrl)
				panel->lge.ddic_ops->lge_panel_dsv_ctrl(panel, true);
		}
	}

	if (gpio_is_valid(panel->bl_config.en_gpio)) {
		rc = gpio_direction_output(panel->bl_config.en_gpio, 1);
		if (rc) {
			pr_err("unable to set dir for bklt gpio rc=%d\n", rc);
			goto error_disable_reset_pin;
		}
	}

	if (gpio_is_valid(panel->reset_config.lcd_mode_sel_gpio)) {
		bool out = true;

		if ((panel->reset_config.mode_sel_state == MODE_SEL_DUAL_PORT)
				|| (panel->reset_config.mode_sel_state
					== MODE_GPIO_LOW))
			out = false;
		else if ((panel->reset_config.mode_sel_state
				== MODE_SEL_SINGLE_PORT) ||
				(panel->reset_config.mode_sel_state
				 == MODE_GPIO_HIGH))
			out = true;

		rc = gpio_direction_output(
			panel->reset_config.lcd_mode_sel_gpio, out);
		if (rc) {
			pr_err("unable to set dir for mode gpio rc=%d\n", rc);
			goto error_disable_gpio_and_regulator;
		}
	}

	goto exit;

error_disable_gpio_and_regulator:
	if (gpio_is_valid(panel->reset_config.disp_en_gpio))
		gpio_set_value(panel->reset_config.disp_en_gpio, 0);

	if (gpio_is_valid(panel->bl_config.en_gpio))
		gpio_set_value(panel->bl_config.en_gpio, 0);

error_disable_reset_pin:
	if (gpio_is_valid(panel->reset_config.reset_gpio)) {
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RESET, panel->lge.display_id, LGE_PANEL_RESET_LOW); // PANEL RESET LOW
		gpio_set_value(panel->reset_config.reset_gpio, 0);
	}
exit:
	return rc;
}

/* @Override */
int dsi_panel_pre_prepare(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_power_on(panel);
	if (rc) {
		pr_err("[%s] panel power on failed, rc=%d\n", panel->name, rc);
		goto error;
	}

	/* If LP11_INIT is set, panel will be powered up during prepare() */
	if (panel->lp11_init)
		goto error;

	rc = dsi_panel_reset(panel);
	if (rc) {
		pr_err("[%s] panel reset failed, rc=%d\n", panel->name, rc);
		goto error;
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

/* @Override */
int dsi_panel_prepare(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (panel->lp11_init) {
		rc = dsi_panel_reset(panel);
		if (rc) {
			pr_err("[%s] panel reset failed, rc=%d\n",
			       panel->name, rc);
			goto error;
		}
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_PRE_ON);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_SET_PRE_ON cmds, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

/* @Override */
int dsi_panel_unprepare(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (panel->lge.use_ddic_reg_backup) {
		if (atomic_read(&panel->lge.backup_state) > 0) {
			pr_warn("WARNING: backup work pending\n");
			flush_work(&panel->lge.backup_work);
			atomic_set(&panel->lge.backup_state, 0);
		}
	}

	if (panel->lge.use_drs_mngr
			&& (lge_drs_mngr_get_state(panel) > DRS_IDLE)) {
		rc = lge_drs_mngr_finish(panel);
		if (rc) {
			pr_warn("WARNING: drs is not finished correctly\n");
		} else {
			rc = wait_for_completion_timeout(&panel->lge.drs_mngr.drs_work_done,
							DRS_TIMEOUT);
			if (rc <= 0)
				pr_err("drs timeout\n");

			pr_info("drs state (%d)\n", lge_drs_mngr_get_state(panel));
		}
	}

	panel->lge.allow_bl_update_ex = false;

	mutex_lock(&panel->panel_lock);
	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_POST_OFF);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_SET_POST_OFF cmds, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

	if (panel->lp11_init) {
		rc = dsi_panel_reset_off(panel);
		if (rc) {
			pr_err("[%s] panel reset_off failed, rc=%d\n", panel->name, rc);
			goto error;
		}
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

/* @Override */
int dsi_panel_post_unprepare(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	if (!panel->lp11_init) {
		rc = dsi_panel_reset_off(panel);
		if (rc) {
			pr_err("[%s] panel reset_off failed, rc=%d\n", panel->name, rc);
			goto error;
		}
	}

	rc = dsi_panel_power_off(panel);
	if (rc) {
		pr_err("[%s] panel power_Off failed, rc=%d\n", panel->name, rc);
		goto error;
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

/* @Override */
int dsi_panel_post_enable(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_POST_ON);
	if (rc) {
		pr_err("[%s] failed to send DSI_CMD_SET_POST_ON cmds, rc=%d\n",
		       panel->name, rc);
		goto error;
	}

	if (panel->lge.panel_dead) {
		panel->lge.panel_dead = false;
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RECOVERY, panel->lge.display_id, LGE_PANEL_RECOVERY_ALIVE);
		pr_err("[%s] recovery done. bl_lvl_recovery_unset = %d, bl_lvl_unset = %d\n", panel->name, panel->lge.bl_lvl_recovery_unset, panel->lge.bl_lvl_unset);
		if (panel->lge.bl_lvl_recovery_unset == -1) {
			panel->bl_config.bl_level = panel->lge.bl_lvl_unset;
			dsi_panel_set_backlight(panel, panel->lge.bl_lvl_unset);
		} else {
			panel->bl_config.bl_level = panel->lge.bl_lvl_recovery_unset;
			dsi_panel_set_backlight(panel, panel->lge.bl_lvl_recovery_unset);
		}
		panel->lge.bl_lvl_recovery_unset = -1;
		panel->lge.bl_lvl_unset = -1;
		panel->lge.allow_bl_update = true;
	}

	if (lge_get_factory_boot() || lge_get_mfts_mode()) {
		if (panel->lge.use_panel_err_detect && !panel->lge.err_detect_irq_enabled) {
			if (panel->lge.ddic_ops && panel->lge.ddic_ops->set_err_detect_mask)
				panel->lge.ddic_ops->set_err_detect_mask(panel);
			lge_panel_err_detect_irq_control(panel, true);
		}
	}

error:
	mutex_unlock(&panel->panel_lock);

	pr_info("lp_state=%d, dsi_mode_flags=0x%X\n",
			panel->lge.lp_state, panel->cur_mode->dsi_mode_flags);
	if (panel->lge.lp_state == LGE_PANEL_NOLP &&
			!(panel->cur_mode->dsi_mode_flags & DSI_MODE_FLAG_DMS)) {
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_BLANK, panel->lge.display_id, LGE_PANEL_STATE_UNBLANK); // U3, UNBLANK
	}

	return rc;
}

static void lge_mdss_panel_dead_notify(struct dsi_display *display)
{
	struct sde_connector *conn = NULL;
	struct drm_event event;

	if (!display) {
		pr_err("display is null.\n");
		return;
	}

	if (!display->panel) {
		pr_err("panel is null.\n");
		return;
	}

	conn = to_sde_connector(display->drm_conn);
	if (!conn) {
		pr_err("display->drm_conn is null\n");
		return;
	}

	mutex_lock(&display->panel->panel_lock);
	if (display->panel->lge.panel_dead) {
		pr_err("Already in recovery state\n");
		mutex_unlock(&display->panel->panel_lock);
		return;
	}

	pr_info("******** %s display : ESD detected!!!!LCD recovery function called!!!! ********\n", display->display_type);
	display->panel->lge.panel_dead = true;

	if (display->panel->lge.bl_lvl_unset == -1 && display->panel->lge.allow_bl_update == false)
		display->panel->lge.bl_lvl_recovery_unset = -1;
	else if (display->panel->lge.bl_lvl_unset != -1 && display->panel->lge.allow_bl_update == false)
		display->panel->lge.bl_lvl_recovery_unset = display->panel->lge.bl_lvl_unset;
	else
		display->panel->lge.bl_lvl_recovery_unset = display->panel->bl_config.bl_level;

	mutex_unlock(&display->panel->panel_lock);

	if (lge_get_factory_boot() || lge_get_mfts_mode()) {
		if (display->panel->lge.use_panel_err_detect && display->panel->lge.err_detect_irq_enabled)
			lge_panel_err_detect_irq_control(display->panel, false);
	}
	lge_panel_notifier_call_chain(LGE_PANEL_EVENT_RECOVERY, display->panel->lge.display_id, LGE_PANEL_RECOVERY_DEAD);

	event.type = DRM_EVENT_PANEL_DEAD;
	event.length = sizeof(u32);
	msm_mode_object_event_notify(&conn->base.base,
		conn->base.dev, &event, (u8 *)&display->panel->lge.panel_dead);
	sde_encoder_display_failure_notification(conn->encoder, false);
}

void lge_mdss_panel_dead_work(struct work_struct *work)
{
	struct lge_dsi_panel *lge_panel = NULL;
	struct dsi_panel *panel = NULL;
	struct dsi_display *display = NULL;
	struct sde_connector *conn = NULL;

	lge_panel = container_of(to_delayed_work(work), struct lge_dsi_panel, panel_dead_work);
	if (!lge_panel) {
		pr_err("fail to get lge_dsi_panel object\n");
		return;
	}

	panel = container_of(lge_panel, struct dsi_panel, lge);
	if (!panel) {
		pr_err("fail to get dsi_panel object\n");
		return;
	}

	display = container_of(panel->host, struct dsi_display, host);
	if (!display) {
		pr_err("display is null.\n");
		return;
	}

	conn = to_sde_connector(display->drm_conn);
	if (!conn) {
		pr_err("display->drm_conn is null\n");
		return;
	}

	mutex_lock(&conn->lock);
	if (dsi_panel_get_current_power_mode(display->panel) != SDE_MODE_DPMS_ON) {
		mutex_unlock(&conn->lock);
		pr_info("lp_state is not nolp(U3)\n");
		mutex_lock(&display->panel->panel_lock);
		if (display->panel->lge.panel_dead_pending) {
			mutex_unlock(&display->panel->panel_lock);
			pr_info("re-trigger panel_dead after 5 secs\n");
			schedule_delayed_work(&display->panel->lge.panel_dead_work,
							msecs_to_jiffies(STATUS_CHECK_INTERVAL_MS));
			return;
		}
		mutex_unlock(&display->panel->panel_lock);
	}
	mutex_unlock(&conn->lock);

	mutex_lock(&display->panel->panel_lock);
	display->panel->lge.panel_dead_pending = false;
	mutex_unlock(&display->panel->panel_lock);

	lge_mdss_panel_dead_notify(display);
}

void lge_mdss_report_panel_dead()
{
	struct dsi_display *display_prim = NULL;
	struct dsi_display *display_sec = NULL;
	struct sde_connector *conn_prim = NULL;
	struct sde_connector *conn_sec = NULL;

	display_prim = primary_display;
	display_sec = secondary_display;

	if (!display_prim && !display_sec) {
		pr_err("There is no display.\n");
		return;
	}

	if (!display_prim->panel && !display_sec->panel) {
		pr_err("There is no panel.\n");
		return;
	}

	conn_prim = to_sde_connector(display_prim->drm_conn);
	if(display_sec && display_sec->panel)
		conn_sec = to_sde_connector(display_sec->drm_conn);

	if (!conn_prim && !conn_sec) {
		pr_err("There is no sde_connector\n");
		return;
	}

	if (display_prim && display_prim->panel && conn_prim) {
		mutex_lock(&conn_prim->lock);
		if (dsi_panel_get_current_power_mode(display_prim->panel) != SDE_MODE_DPMS_ON) {
			pr_info("primary : lp_state is not nolp(U3)\n");
			if (!display_prim->panel->lge.panel_dead_pending) {
				pr_info("primary : re-trigger panel_dead after 5 secs\n");
				display_prim->panel->lge.panel_dead_pending = true;
				mutex_unlock(&conn_prim->lock);
				schedule_delayed_work(&display_prim->panel->lge.panel_dead_work,
											msecs_to_jiffies(STATUS_CHECK_INTERVAL_MS));
			} else {
				pr_info("primary : already re-triggered panel_dead\n");
				mutex_unlock(&conn_prim->lock);
			}
			return;
		} else if (display_prim->panel->lge.panel_dead_pending) {
			pr_err("primary : already panel dead work scheduled\n");
			mutex_unlock(&conn_prim->lock);
			return;
		}
		mutex_unlock(&conn_prim->lock);
	}

	if (display_sec && display_sec->panel && conn_sec) {
		mutex_lock(&conn_sec->lock);
		if (dsi_panel_get_current_power_mode(display_sec->panel) != SDE_MODE_DPMS_ON) {
			pr_info("secondary : lp_state is not nolp(U3)\n");
			if (!display_sec->panel->lge.panel_dead_pending) {
				pr_info("secondary : re-trigger panel_dead after 5 secs\n");
				display_sec->panel->lge.panel_dead_pending = true;
				mutex_unlock(&conn_sec->lock);
				schedule_delayed_work(&display_sec->panel->lge.panel_dead_work,
											msecs_to_jiffies(STATUS_CHECK_INTERVAL_MS));
			} else {
				pr_info("secodary : already re-triggered panel_dead\n");
				mutex_unlock(&conn_sec->lock);
			}
			return;
		} else if (display_sec->panel->lge.panel_dead_pending) {
			pr_err("secondary : already panel dead work scheduled\n");
			mutex_unlock(&conn_sec->lock);
			return;
		}
		mutex_unlock(&conn_sec->lock);
	}
	lge_mdss_panel_dead_notify(display_prim);
}
EXPORT_SYMBOL(lge_mdss_report_panel_dead);

static void lge_mdss_report_panel_dead_individually(struct dsi_panel *panel)
{
	struct dsi_display *display = NULL;
	struct sde_connector *conn = NULL;
	struct drm_event event;
	struct device *dev;

	if (!panel) {
		pr_err("invalid panel param\n");
		return;
	}

	display = container_of(panel->host, struct dsi_display, host);
	if (!display) {
		pr_err("invalid display param\n");
		return;
	}

	conn = to_sde_connector(display->drm_conn);
	if (!conn)
		return;

	panel->lge.force_panel_dead = true;

	mutex_lock(&conn->lock);
	dev = conn->base.dev->dev;

	if (!conn->ops.check_status || dev->power.is_suspended ||
					(conn->dpms_mode != DRM_MODE_DPMS_ON)) {
		pr_err("dpms mode: %d\n", conn->dpms_mode);
		mutex_unlock(&conn->lock);
		return;
	}

	conn->ops.check_status(&conn->base, display, true);
	mutex_unlock(&conn->lock);

	event.type = DRM_EVENT_PANEL_DEAD;
	event.length = sizeof(u32);
	msm_mode_object_event_notify(&conn->base.base,
		conn->base.dev, &event, (u8 *)&panel->lge.panel_dead);
	sde_encoder_display_failure_notification(conn->encoder, false);

	panel->lge.force_panel_dead = false;
}

static ssize_t lge_mdss_force_report_panel_dead(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	struct dsi_panel *panel;

	panel = dev_get_drvdata(dev);
	lge_mdss_report_panel_dead_individually(panel);
	return 1;
}
static DEVICE_ATTR(report_panel_dead, S_IRUGO,
										lge_mdss_force_report_panel_dead, NULL);

static struct attribute *test_attrs[] = {
	&dev_attr_report_panel_dead.attr,
	NULL,
};

static const struct attribute_group test_attr_group = {
	.name	= "test",
	.attrs	= test_attrs,
};

static int lge_dsi_panel_gpio_request(struct dsi_panel *panel)
{
	int rc = 0, i, j;
	char name[10] = {0,};

	for (i = 0; i < panel->lge.pins_num; i++) {
		if (gpio_is_valid(panel->lge.pins[i])) {
			snprintf(name, sizeof(name), "panel_pin_%d_gpio", i);
			rc = gpio_request(panel->lge.pins[i], name);
			if (rc) {
				pr_err("request for %s failed, rc=%d\n", name, rc);
				break;
			}
		}
	}

	if (i < panel->lge.pins_num) {
		for (j = i; j >= 0; j--) {
			if (gpio_is_valid(panel->lge.pins[j]))
				gpio_free(panel->lge.pins[j]);
		}
	}

	return rc;
}

static int lge_dsi_panel_gpio_release(struct dsi_panel *panel)
{
	int rc = 0, i;
	for (i = 0; i < panel->lge.pins_num; i++) {
		if (gpio_is_valid(panel->lge.pins[i]))
			gpio_free(panel->lge.pins[i]);
	}
#if defined(CONFIG_DSV_SM5109)
	if (gpio_is_valid(panel->lge.dsv_vpos_gpio_sm5109))
		gpio_free(panel->lge.dsv_vpos_gpio_sm5109);
	if (gpio_is_valid(panel->lge.dsv_vneg_gpio_sm5109))
		gpio_free(panel->lge.dsv_vneg_gpio_sm5109);
	if (gpio_is_valid(panel->lge.touch_reset_gpio))
		gpio_free(panel->lge.touch_reset_gpio);
#endif
	return rc;
}

static void lge_dsi_panel_create_sysfs(struct dsi_panel *panel)
{
	static struct class *class_panel = NULL;
	static struct device *dev_panel_prim = NULL;
	static struct device *dev_panel_sec = NULL;

	if(!class_panel){
		class_panel = class_create(THIS_MODULE, "panel");
		if (IS_ERR(class_panel)) {
			pr_err("Failed to create panel class\n");
			return;
		}
	}

	if (panel->lge.display_id == DSI_PRIMARY) {
		if(!dev_panel_prim) {
			dev_panel_prim = device_create(class_panel, NULL, 0, panel, "panel-0");
			if (IS_ERR(class_panel)) {
				pr_err("Failed to create panel-0 device\n");
				return;
			}

			lge_color_manager_create_sysfs(panel, dev_panel_prim);
			lge_brightness_create_sysfs(panel, dev_panel_prim);
			lge_ambient_create_sysfs(panel, dev_panel_prim);
			lge_panel_drs_create_sysfs(panel, dev_panel_prim);
			lge_panel_reg_create_sysfs(panel, dev_panel_prim);
			lge_panel_factory_create_sysfs(panel, dev_panel_prim);
			lge_panel_err_detect_create_sysfs(panel, dev_panel_prim);

			if (sysfs_create_group(&dev_panel_prim->kobj, &test_attr_group) < 0)
				pr_err("create test group fail!");
		}
	} else if (panel->lge.display_id == DSI_SECONDARY) {
		if(!dev_panel_sec) {
			dev_panel_sec = device_create(class_panel, NULL, 0, panel, "panel-1");
			if (IS_ERR(class_panel)) {
				pr_err("Failed to create panel-1 device\n");
				return;
			}

			lge_color_manager_create_sysfs(panel, dev_panel_sec);
			lge_brightness_create_sysfs(panel, dev_panel_sec);
			lge_ambient_create_sysfs(panel, dev_panel_sec);
			lge_panel_drs_create_sysfs(panel, dev_panel_sec);
			lge_panel_reg_create_sysfs(panel, dev_panel_sec);
			lge_panel_factory_create_sysfs(panel, dev_panel_sec);
			lge_panel_err_detect_create_sysfs(panel, dev_panel_sec);

			if (sysfs_create_group(&dev_panel_sec->kobj, &test_attr_group) < 0)
				pr_err("create test group fail!");
		}
	}
}

int lge_dsi_panel_drv_init(struct dsi_panel *panel)
{
	int rc = 0;

	rc = lge_dsi_panel_gpio_request(panel);
	if (rc)
		pr_err("failed to request panel pins, rc=%d\n", rc);

	lge_dsi_panel_create_sysfs(panel);

	return rc;
}

int lge_dsi_panel_drv_post_init(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_display *display = container_of(panel->host, struct dsi_display, host);

	pr_info("bl control variable init\n");
	if (display->is_cont_splash_enabled) {
		panel->lge.allow_bl_update = true;
		panel->lge.bl_lvl_unset = -1;
		panel->lge.allow_bl_update_ex = false;
		panel->lge.bl_ex_lvl_unset = -1;
		panel->lge.bl_lvl_recovery_unset = -1;
	} else {
		panel->lge.allow_bl_update = false;
		panel->lge.bl_lvl_unset = -1;
		panel->lge.allow_bl_update_ex = false;
		panel->lge.bl_ex_lvl_unset = -1;
		panel->lge.bl_lvl_recovery_unset = -1;
	}
	return rc;
}

int lge_dsi_panel_drv_deinit(struct dsi_panel *panel)
{
	int rc = 0;

	if (panel->lge.use_panel_err_detect)
		lge_panel_err_detect_remove(panel);
	rc = lge_dsi_panel_gpio_release(panel);
	if (rc)
		pr_err("failed to release panel pins, rc=%d\n", rc);

	return rc;
}

static int lge_dsi_panel_parse_pin_seq(struct dsi_panel *panel, struct device_node *of_node, const char* prop_name, struct lge_panel_pin_seq **seq_out)
{
	int rc = 0, i;
	u32 length = 0, count = 0;
	const u32 *prop;
	u32 *arr;
	struct lge_panel_pin_seq *seq;

	prop = of_get_property(of_node, prop_name, &length);
	if (!prop) {
		pr_err("%s not found\n", prop_name);
		rc = -EINVAL;
		goto error_no_free_arr;
	}

	arr = kzalloc(length, GFP_KERNEL);
	if (!arr) {
		rc = -ENOMEM;
		goto error_no_free_arr;
	}

	length /= sizeof(u32);
	rc = of_property_read_u32_array(of_node, prop_name, arr, length);
	if (rc) {
		pr_err("failed to read u32 array of %s, rc=%d\n", prop_name, rc);
		goto error_free_arr;
	}

	count = length/3;
	seq = kzalloc(sizeof(struct lge_panel_pin_seq)*(count+1), GFP_KERNEL);
	if (!seq) {
		rc = -ENOMEM;
		goto error_free_arr;
	}

	*seq_out = seq;
	for (i = 0; i < length; i += 3) {
		if (arr[i] >= panel->lge.pins_num || arr[i] < 0) {
			pr_err("failed to parse %s, pins_num=%d, arr[%d]=%d\n", prop_name, panel->lge.pins_num, i, arr[i]);
			rc = -EINVAL;
			break;
		}
		seq->gpio = panel->lge.pins[arr[i]];
		seq->level = arr[i+1];
		seq->sleep_ms = arr[i+2];
		seq++;
	}
	seq->gpio = -1;

	if (rc) {
		kfree(*seq_out);
		*seq_out = NULL;
	}

error_free_arr:
	kfree(arr);
error_no_free_arr:
	return rc;
}

#define LGE_PROPNAME_PANEL_ON_PIN_SEQ "lge,panel-on-pin-seq"
#define LGE_PROPNAME_PANEL_OFF_PIN_SEQ "lge,panel-off-pin-seq"
#define LGE_PROPNAME_PANEL_PINS "lge,panel-pins"
static int lge_dsi_panel_parse_gpios(struct dsi_panel *panel, struct device_node *of_node)
{
	int rc = 0, i;

#if defined(CONFIG_DSV_SM5109)
	panel->lge.touch_reset_gpio = of_get_named_gpio(of_node, "lge,touch-reset-gpio", 0);
	if (gpio_is_valid(panel->lge.touch_reset_gpio))
		pr_info("touch_reset_gpio: %d\n", panel->lge.touch_reset_gpio);

	panel->lge.dsv_vpos_gpio_sm5109 = of_get_named_gpio(of_node, "lge,sm5109-dsv-vpos-gpio", 0);
	if (gpio_is_valid(panel->lge.dsv_vpos_gpio_sm5109))
		pr_info("dsv_vpos_gpio_sm5109: %d\n", panel->lge.dsv_vpos_gpio_sm5109);

	panel->lge.dsv_vneg_gpio_sm5109 = of_get_named_gpio(of_node, "lge,sm5109-dsv-vneg-gpio", 0);
	if (gpio_is_valid(panel->lge.dsv_vneg_gpio_sm5109))
		pr_info("dsv_vneg_gpio_sm5109: %d\n", panel->lge.dsv_vneg_gpio_sm5109);
#endif

	panel->lge.pins_num = of_gpio_named_count(of_node, LGE_PROPNAME_PANEL_PINS);

	if (panel->lge.pins_num <= 0) {
		pr_warn("no panel pin defined\n");
		return 0;
	}

	panel->lge.pins = kcalloc(panel->lge.pins_num, sizeof(int), GFP_KERNEL);
	if (!panel->lge.pins) {
		rc = -ENOMEM;
		goto error_alloc_gpio_array;
	}

	for (i = 0; i < panel->lge.pins_num; i++) {
		panel->lge.pins[i] = of_get_named_gpio(of_node, LGE_PROPNAME_PANEL_PINS, i);
	}

	rc = lge_dsi_panel_parse_pin_seq(panel, of_node, LGE_PROPNAME_PANEL_ON_PIN_SEQ, &panel->lge.panel_on_seq);
	if (rc) {
		goto error_parse_pins;
	}

	rc = lge_dsi_panel_parse_pin_seq(panel, of_node, LGE_PROPNAME_PANEL_OFF_PIN_SEQ, &panel->lge.panel_off_seq);
	if (rc) {
		goto error_parse_pins;
	}

#if defined(CONFIG_MFD_DW8768)
	panel->lge.dsv_ena_gpio_dw8768 = of_get_named_gpio(of_node, "lge,dw8768-dsv-ena-gpio", 0);
	if (gpio_is_valid(panel->lge.dsv_ena_gpio_dw8768))
		pr_info("dsv_ena_gpio_dw8768: %d\n", panel->lge.dsv_ena_gpio_dw8768);
#endif
	return rc;

error_parse_pins:
	kfree(panel->lge.pins);
	panel->lge.pins = NULL;
	panel->lge.pins_num = 0;

error_alloc_gpio_array:
	return rc;
}

static int lge_dsi_panel_parse_dt(struct dsi_panel *panel, struct device_node *of_node)
{
	int rc = 0;
	const char *ddic_name;
	const char *man_name;
	u32 tmp = 0;

	// TODO: change property name
	panel->lp11_init = of_property_read_bool(of_node, "qcom,mdss-dsi-lane-0-state");

	man_name = of_get_property(of_node, "lge,man-name", NULL);
	if (man_name) {
		strncpy(panel->lge.manufacturer_name, man_name, MAN_NAME_LEN);
		pr_info("manufacturer name=%s\n", panel->lge.manufacturer_name);
	} else {
		strncpy(panel->lge.manufacturer_name, "undefined", MAN_NAME_LEN);
		pr_info("manufacturer name is not set\n");
	}

	ddic_name = of_get_property(of_node, "lge,ddic-name", NULL);
	if (ddic_name) {
		strncpy(panel->lge.ddic_name, ddic_name, DDIC_NAME_LEN);
		pr_info("ddic name=%s\n", panel->lge.ddic_name);
	} else {
		strncpy(panel->lge.ddic_name, "undefined", DDIC_NAME_LEN);
		pr_info("ddic name is not set\n");
	}

	// TODO: temporal use
	panel->lge.use_labibb = of_property_read_bool(of_node, "lge,use-labibb");
	pr_info("use_labibb=%d\n", panel->lge.use_labibb);

	panel->lge.use_ext_dsv = of_property_read_bool(of_node, "lge,use-ext-dsv");
	pr_info("use_ext_dsv=%d\n", panel->lge.use_ext_dsv);

	panel->lge.reset_after_ddvd = of_property_read_bool(of_node, "lge,reset-after-ddvd");
	pr_info("reset_after_ddvd=%d\n", panel->lge.reset_after_ddvd);

	panel->lge.dcs_brightness_be = of_property_read_bool(of_node, "lge,dcs-brightness-bigendian");
	pr_info("dcs_brightness_be=%d\n", panel->lge.dcs_brightness_be);

	panel->lge.is_incell = of_property_read_bool(of_node, "lge,incell-panel");
	pr_info("is_incell=%d\n", panel->lge.is_incell);

	panel->lge.use_bist = of_property_read_bool(of_node, "lge,ddic-bist-enabled");
	pr_info("use bist pattern=%d\n", panel->lge.use_bist);
	if (panel->lge.use_bist) {
		int i = 0;
		rc = of_property_read_string_array(of_node, "lge,ddic-bist-usage-type",
						panel->lge.bist_usage_type,
						MAX_BIST_USAGE_TYPE);
		for (i = 0; i < MAX_BIST_USAGE_TYPE; i++) {
			pr_debug("bist type=%s\n", panel->lge.bist_usage_type[i]);
		}
	}

	panel->lge.update_pps_in_lp = of_property_read_bool(of_node, "lge,update-pps-in-lp-mode");
	pr_info("update_pps in lp state=%d\n", panel->lge.update_pps_in_lp);

	panel->lge.use_drs_mngr = of_property_read_bool(of_node, "lge,drs-mngr-enabled");
	pr_info("use drs manager=%d\n", panel->lge.use_drs_mngr);

	panel->lge.use_internal_pps_switch =
		of_property_read_bool(of_node, "lge,drs-mngr-internal-pps-switch-enabled");

	panel->lge.use_ddic_reg_backup = of_property_read_bool(of_node, "lge,ddic-register-backup");
	pr_info("use register backup=%d\n", panel->lge.use_ddic_reg_backup);

	panel->lge.bc_dim_en = of_property_read_bool(of_node, "lge,use-bc-dim");
	pr_info("use bc dim =%d\n", panel->lge.bc_dim_en);

	panel->lge.use_color_manager = of_property_read_bool(of_node, "lge,use-color-manager");
	pr_info("use color manager=%d\n", panel->lge.use_color_manager);

	panel->lge.use_color_manager_oled = of_property_read_bool(of_node, "lge,use-color-manager-oled");
	pr_info("use color manager oled=%d\n", panel->lge.use_color_manager_oled);

	rc = of_property_read_u32(of_node, "lge,hbm-mode", &tmp);
	if (rc) {
		panel->lge.hbm_mode = DEFAULT_HBM_MODE;
		pr_err("fail to parse lge.hbm_mode Set to Default %d\n", panel->lge.hbm_mode);
	} else {
		panel->lge.hbm_mode = tmp;
		pr_info("lge.hbm_mode %d\n", panel->lge.hbm_mode);
	}

	panel->lge.use_ambient = of_property_read_bool(of_node, "lge,use-ambient");
	pr_info("use ambient=%d\n", panel->lge.use_ambient);
	if (panel->lge.use_ambient) {
		rc = of_property_read_string_array(of_node, "lge,aod-interface-data",
						panel->lge.aod_interface_type, 3);
		lge_ambient_set_interface_data(panel);
	}

	panel->lge.use_cmd_wait_pa_changed = of_property_read_bool(of_node, "lge,cmd-wait-pa-changed");
	pr_info("use cmd_wait_pa_changed=%d\n", panel->lge.use_cmd_wait_pa_changed);

	panel->lge.use_line_detect = of_property_read_bool(of_node, "lge,use-line-detect");
	pr_info("use line detect=%d\n", panel->lge.use_line_detect);

	panel->lge.use_bc_dimming_work = of_property_read_bool(of_node, "lge,bc-dimming-work");
	pr_info("use bc dimming work=%d\n", panel->lge.use_bc_dimming_work);

	if (panel->lge.use_color_manager) {
		rc = of_property_read_u32(of_node, "lge,color-manager-default-status", &tmp);
		if (rc) {
			pr_err("fail to parse lge,color-manager-default-status\n");
			panel->lge.color_manager_default_status = false;
		} else {
			panel->lge.color_manager_default_status = (tmp > 0)? true : false;
			panel->lge.color_manager_status = 1;
			pr_info("color manager default status is %d\n", panel->lge.color_manager_default_status);
		}

		mdss_dsi_parse_color_manager_modes(of_node, panel->lge.color_manager_table,
					&(panel->lge.color_manager_table_len), "lge,mdss-dsi-color-manager-mode-table");

		panel->lge.dgc_absent = of_property_read_bool(of_node, "lge,digital-gamma-absent");
		pr_info("digital gamma absent = %d\n", panel->lge.dgc_absent);
	} else {
		panel->lge.color_manager_default_status = false;
	}

	panel->lge.use_panel_err_detect = of_property_read_bool(of_node, "lge,use-panel-err-detect");
	pr_info("use panel err detect = %d\n", panel->lge.use_panel_err_detect);

	if (panel->lge.use_panel_err_detect) {
		lge_panel_err_detect_parse_dt(panel, of_node);
	}

	panel->lge.use_panel_reset_low_before_lp11 = of_property_read_bool(of_node, "lge,use-panel-reset-low-before-lp11");
	pr_info("use panel reset low before lp11 = %d\n", panel->lge.use_panel_reset_low_before_lp11);

	panel->lge.use_extra_recovery_cmd = of_property_read_bool(of_node, "lge,use-extra-recovery-cmd");
	pr_info("use extra recovery command = %d\n", panel->lge.use_extra_recovery_cmd);

	panel->lge.use_dcs_brightness_short = of_property_read_bool(of_node, "lge,dcs-brightness-short-write");
	pr_info("use dcs_brightness_short=%d\n", panel->lge.use_dcs_brightness_short);

	panel->lge.use_ddic_reg_lock = of_property_read_bool(of_node, "lge,use-ddic-register-lock");
	pr_info("use ddic_register_lock=%d\n", panel->lge.use_ddic_reg_lock);

	panel->lge.use_irc_ctrl = of_property_read_bool(of_node, "lge,use-irc-ctrl");
	pr_info("use irc_ctrl=%d\n", panel->lge.use_irc_ctrl);

	panel->lge.use_ace_ctrl = of_property_read_bool(of_node, "lge,use-ace-ctrl");
	pr_info("use ace_ctrl=%d\n", panel->lge.use_ace_ctrl);

	if (panel->lge.use_ace_ctrl) {
		rc = of_property_read_u32(of_node, "lge,default-ace-mode", &tmp);
		if (rc) {
			pr_err("fail to get ace default, set %d\n", panel->lge.ace_mode);
		} else {
			panel->lge.ace_mode = tmp;
			pr_info("ace default mode=%d\n", panel->lge.ace_mode);
		}
	}

	panel->lge.true_view_supported = of_property_read_bool(of_node, "lge,true-view-supported");
	pr_info("use true_view supported=%d\n", panel->lge.true_view_supported);

	panel->lge.use_vr_lp_mode = of_property_read_bool(of_node, "lge,use-vr-lp-mode");
	pr_info("use vr_lp_mode=%d\n", panel->lge.use_vr_lp_mode);

	panel->lge.use_dim_ctrl = of_property_read_bool(of_node, "lge,use-dim-ctrl");
	pr_info("use_dim_ctrl=%d\n", panel->lge.use_dim_ctrl);

	panel->lge.use_br_ctrl_ext = of_property_read_bool(of_node, "lge,disp-br-ctrl-ext-supported");
	pr_info("use_br_ctrl_ext=%d\n", panel->lge.use_br_ctrl_ext);

	panel->lge.use_fp_lhbm = of_property_read_bool(of_node, "lge,use-fp-lhbm");
	pr_info("use_fp_lhbm=%d\n", panel->lge.use_fp_lhbm);
	if(panel->lge.use_fp_lhbm) {
		panel->lge.fp_lhbm_br_lvl = FP_LHBM_DEFAULT_BR_LVL;
		panel->lge.need_fp_lhbm_set = false;
		panel->lge.use_fps_mode1_new = of_property_read_bool(of_node, "lge,use-fps-mode1-new");
		pr_info("use_fps_mode1_new=%d\n", panel->lge.use_fps_mode1_new);
	}
	panel->lge.use_tc_perf = of_property_read_bool(of_node, "lge,use-tc-perf");
	pr_info("use_tc_perf=%d\n", panel->lge.use_tc_perf);

	panel->lge.use_cm_lut = of_property_read_bool(of_node, "lge,use-color-manager-lut");
	pr_info("use color mananger lut supported=%d\n", panel->lge.use_cm_lut);

	if (panel->lge.use_cm_lut) {
		panel->lge.cm_lut_cnt = of_property_count_strings(of_node, "lge,cm-lut-screen-mode-set-name");
		if (panel->lge.cm_lut_cnt) {
			panel->lge.cm_lut_name_list = kzalloc(panel->lge.cm_lut_cnt * 8, GFP_KERNEL);
			if (!panel->lge.cm_lut_name_list) {
				pr_err("Unable to cm_lut_name_list alloc fail\n");
			} else {
				of_property_read_string_array(of_node, "lge,cm-lut-screen-mode-set-name", panel->lge.cm_lut_name_list, panel->lge.cm_lut_cnt);
			}
		}
		lge_ddic_dsi_panel_parse_cm_lut_cmd_sets(panel, of_node);
	}
	return rc;
}

int lge_dsi_panel_get(struct dsi_panel *panel, struct device_node *of_node)
{
	int rc = 0;

	rc = lge_dsi_panel_parse_gpios(panel, of_node);
	if (rc)
		pr_err("failed to parse panel gpios, rc=%d\n", rc);

	rc = lge_dsi_panel_parse_blmap(panel, of_node);
	if (rc)
		pr_err("failed to parse blmap, rc=%d\n", rc);

	rc = lge_dsi_panel_parse_brightness(panel, of_node);
	if (rc)
		pr_err("failed to parse default brightness, rc=%d\n", rc);

	rc = lge_dsi_panel_parse_dt(panel, of_node);
	if (rc)
		pr_err("failed to parse dt, rc=%d\n", rc);

	rc = lge_ddic_dsi_panel_parse_cmd_sets(panel, of_node);
	if (rc)
		pr_err("failed to parse ddic cmds sets, rc=%d\n", rc);

	lge_ddic_ops_init(panel);

	lge_ddic_feature_init(panel);

	/* TO DO */
	if (lge_drs_mngr_is_enabled(panel)) {
		if (lge_drs_mngr_init(panel) < 0)
			pr_warn("failed to initialize drs mngr\n");
	}

	if (panel->lge.use_color_manager) {
		pr_info("default cm_preset_step 2\n");
		panel->lge.cm_preset_step = 2;

		if (panel->lge.use_bc_dimming_work)
			lge_bc_dim_work_init(panel);
	}

	INIT_DELAYED_WORK(&panel->lge.panel_dead_work, lge_mdss_panel_dead_work);

	return rc;
}

inline void lge_dsi_panel_pin_seq_deinit(struct lge_panel_pin_seq **pseq)
{
	struct lge_panel_pin_seq *seq = *pseq;
	if (seq) {
		*pseq = NULL;
		kfree(seq);
	}
}

static void lge_dsi_panel_pins_deinit(struct dsi_panel *panel)
{
	if (panel->lge.pins_num && panel->lge.pins) {
		panel->lge.pins_num = 0;
		kfree(panel->lge.pins);
		lge_dsi_panel_pin_seq_deinit(&panel->lge.panel_on_seq);
		lge_dsi_panel_pin_seq_deinit(&panel->lge.panel_off_seq);
	}
}

void lge_dsi_panel_put(struct dsi_panel *panel)
{
	lge_dsi_panel_blmap_free(panel);
	lge_dsi_panel_pins_deinit(panel);
}

int lge_dsi_panel_parse_cmd_state(struct device_node *of_node, const char *name, enum dsi_cmd_set_state *pstate)
{
	int rc = 0;
	const char *state;

	if (pstate == NULL)
		return -EINVAL;

	state = of_get_property(of_node, name, NULL);
	if (state) {
		if (!strcmp(state, "dsi_lp_mode")) {
			*pstate = DSI_CMD_SET_STATE_LP;
		} else if (!strcmp(state, "dsi_hs_mode")) {
			*pstate = DSI_CMD_SET_STATE_HS;
		}
	} else {
		pr_warn("%s is not set", name);
		rc = -EINVAL;
	}

	return rc;
}

int lge_dsi_panel_parse_cmd_sets_sub(struct dsi_panel_cmd_set *cmd,
					const char *data,
					u32 length)
{
	int rc = 0;
	u32 packet_count = 0;

	rc = dsi_panel_get_cmd_pkt_count(data, length, &packet_count);
	if (rc) {
		pr_err("commands failed, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_alloc_cmd_packets(cmd, packet_count);
	if (rc) {
		pr_err("failed to allocate cmd packets, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_create_cmd_packets(data, length, packet_count,
					  cmd->cmds);
	if (rc) {
		pr_err("failed to create cmd packets, rc=%d\n", rc);
		goto error_free_mem;
	}

	return rc;
error_free_mem:
	kfree(cmd->cmds);
	cmd->cmds = NULL;
error:
	return rc;
}

int lge_dsi_panel_tx_cmd_set(struct dsi_panel *panel,
				struct dsi_panel_cmd_set *cmd)
{
	int rc = 0, i = 0, j=0;
	ssize_t len;
	struct dsi_cmd_desc *cmds;

	u32 count;
	enum dsi_cmd_set_state state;
	const struct mipi_dsi_host_ops *ops = (panel != NULL)?panel->host->ops:NULL;

	if (!panel || !panel->cur_mode || !ops)
		return -EINVAL;

	cmds = cmd->cmds;
	count = cmd->count;
	state = cmd->state;

	if (count == 0) {
		pr_debug("[%s] No commands to be sent\n",
			 panel->name);
		goto error;
	}

	for (i = 0; i < count; i++) {
		/* TODO:  handle last command */
		if (state == DSI_CMD_SET_STATE_LP)
			cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;

		if (cmds->last_command)
			cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;

		len = ops->transfer(panel->host, &cmds->msg);
		if (len < 0) {
			rc = len;
			pr_err("failed to set cmds, rc=%d\n", rc);
			goto error;
		}
		if (cmds->post_wait_ms)
			usleep_range(cmds->post_wait_ms * 1000, ((cmds->post_wait_ms * 1000) + 10));
		for(j=0; j < cmd->cmds[i].msg.tx_len; j++)
		{
			pr_debug("0x%02x send\n", (*(u8 *)(cmd->cmds[i].msg.tx_buf+j)));
		}
		cmds++;
	}
error:
	return rc;
}

int lge_mdss_dsi_panel_cmd_read(struct dsi_panel *panel, u8 cmd, int cnt, char* ret_buf)
{
	u8 rx_buf[256] = {0x0};
	int i = 0, ret = 0, checksum = 0;
	const struct mipi_dsi_host_ops *ops;

	struct dsi_cmd_desc cmds = {
		.msg = {
			.channel = 0,
			.type = MIPI_DSI_DCS_READ,
			.tx_buf = &cmd,
			.tx_len = 1,
			.rx_buf = &rx_buf[0],
			.rx_len = cnt,
			.flags = MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_LASTCOMMAND | MIPI_DSI_MSG_REQ_ACK
		},
		.last_command = false,
		.post_wait_ms = 0,
	};

	/* TO DO : panel connection check */
	/* if (not_connected) return -EINVAL */

	if (!panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	ops = panel->host->ops;
	ret = ops->transfer(panel->host, &cmds.msg);

	for (i = 0; i < cnt; i++)
		checksum += rx_buf[i];

	pr_info("[Reg:0x%02x] checksum=%d\n", cmd, checksum);

	memcpy(ret_buf, rx_buf, cnt);

	return checksum;
}

int lge_mdss_dsi_panel_cmds_backup(struct dsi_panel *panel, char *owner,
				enum dsi_cmd_set_type type, u8 reg, int nth_cmds)
{
	struct dsi_cmd_desc *cmds = NULL;
	u32 count;
	char *payload;

	if (!strncmp(owner, "lge", 3)) {
		cmds = panel->lge.lge_cmd_sets[type].cmds;
		count = panel->lge.lge_cmd_sets[type].count;
	} else if (!strncmp(owner, "qct", 3)) {
		cmds = panel->cur_mode->priv_info->cmd_sets[type].cmds;
		count = panel->cur_mode->priv_info->cmd_sets[type].count;
	} else if (!strncmp(owner, "dummy", 5)) {
		mdelay(17);
		return 0;
	} else {
		pr_err("owner is not defined\n");
		return -EINVAL;
	}

	if (count == 0) {
		pr_warn("lge (%d) cmds is not founded\n", type);
		return -EINVAL;
	} else {
		int mipi_read_max = 5;
		int update_count;
		struct dsi_cmd_desc *update_cmd = NULL;

		mutex_lock(&panel->panel_lock);
		if (panel->lge.use_ddic_reg_lock)
			lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_REGISTER_UNLOCK);
		update_cmd = find_nth_cmd(cmds, count, reg, nth_cmds);
		if (update_cmd) {
			payload = (char *)update_cmd->msg.tx_buf;
			update_count = (int)update_cmd->msg.tx_len - 1;
			payload++;
			while (mipi_read_max) {
				if (lge_mdss_dsi_panel_cmd_read(panel, reg, update_count, payload) > 0) {
					break;
				} else {
					pr_warn("try_again\n");
					if (panel->lge.use_ddic_reg_lock)
						lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_REGISTER_UNLOCK);
					mipi_read_max--;
					if (mipi_read_max == 0) {
						mutex_unlock(&panel->panel_lock);
						panic("read fail - reboot");
						return -EINVAL;
					}
				}
				usleep_range(1000, 1100);
			}
		} else {
			pr_warn("cmd for addr 0x%02x not found\n", reg);
		}
		if (panel->lge.use_ddic_reg_lock)
			lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_REGISTER_LOCK);
		mutex_unlock(&panel->panel_lock);
	}

	return 0;
}
