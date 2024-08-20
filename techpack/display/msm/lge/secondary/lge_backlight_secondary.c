/*
 * Copyright(c) 2019, LG Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[Display][lge-backlight-secondary:%s:%d] " fmt, __func__, __LINE__
#include "msm_drv.h"
#include "sde_dbg.h"

#include "sde_kms.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include <linux/backlight.h>
#include "dsi_drm.h"
#include "dsi_display.h"
#include "sde_crtc.h"

#include "../brightness/lge_brightness_def.h"
#include "../lge_dsi_panel.h"
#include "lge_backlight_secondary.h"

int br_to_offset_br_sec(struct dsi_panel *panel, int br, int max_br)
{
	int offset, infp;
	int weight = 2;
	int min_br = 10;
	int offset_br = br;

	if (!panel) {
		pr_err("null ptr\n");
		return -EINVAL;
	}

	if (br < min_br)
		goto exit;

	if (panel->lge.br_offset_bypass) {
		pr_info("offset bypass type %s\n", panel->type);
		if (!(strcmp(panel->type, "primary")))
			offset_br = br;
		else
			offset_br = br + (panel->lge.br_offset * weight);
		goto exit;
	}

	if (panel->lge.br_offset_update) {
		offset_br = br + (panel->lge.br_offset * weight);
		goto exit;
	}

	offset = panel->lge.br_offset;
	offset = abs(offset);
	offset *= weight;
	infp = min_br + (offset << 1);

	if (br > infp)
		offset_br = br - offset;
	else {
		int tmp = infp - offset - min_br;
		tmp *= (br - min_br);
		tmp /= (infp - min_br);
		offset_br = min_br + tmp;
	}

exit:
	if (offset_br < min_br)
		offset_br = min_br;
	else if (offset_br > max_br)
		offset_br = max_br;

	pr_info("%d -> %d\n", br, offset_br);
	return offset_br;
}
