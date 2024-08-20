#define pr_fmt(fmt)	"[Display][lge-ambient:%s:%d] " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>

#include "dsi_panel.h"
#include "lge_factory.h"

static ssize_t panel_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel = NULL;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	pr_err("%s-%s\n", panel->lge.manufacturer_name, panel->lge.ddic_name);

	/* The number of characters should not exceed 30 characters. */
	return sprintf(buf, "%s-%s\n", panel->lge.manufacturer_name, panel->lge.ddic_name);
}
static DEVICE_ATTR(panel_type, S_IRUGO,
		panel_type_show, NULL);

static ssize_t mfts_auto_touch_test_mode_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dsi_panel *panel;

	panel = dev_get_drvdata(dev);

	if (!panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	return sprintf(buf, "%d\n", panel->lge.mfts_auto_touch);
}

static ssize_t mfts_auto_touch_test_mode_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsi_panel *panel;
	int input;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &input);
	mutex_lock(&panel->panel_lock);
	panel->lge.mfts_auto_touch = input;
	mutex_unlock(&panel->panel_lock);

	pr_info("auto touch test : %d\n", input);

	return size;
}
static DEVICE_ATTR(mfts_auto_touch_test_mode, S_IRUGO | S_IWUSR | S_IWGRP,
		mfts_auto_touch_test_mode_get, mfts_auto_touch_test_mode_set);

static ssize_t check_vert_black_line(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->lge_check_vert_black_line)
		panel->lge.ddic_ops->lge_check_vert_black_line(panel);
	else
		pr_err("Can not find lge_check_vert_white_line");

	return ret;
}
static DEVICE_ATTR(check_black_line, S_IWUSR | S_IWGRP, NULL, check_vert_black_line);

static ssize_t check_vert_white_line(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->lge_check_vert_white_line)
		panel->lge.ddic_ops->lge_check_vert_white_line(panel);
	else
		pr_err("Can not find lge_check_vert_white_line");

	return ret;
}
static DEVICE_ATTR(check_white_line, S_IWUSR | S_IWGRP, NULL, check_vert_white_line);

static ssize_t check_vert_line_restore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct dsi_panel *panel;

	panel = dev_get_drvdata(dev);
	if (!panel) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->lge_check_vert_line_restore)
		panel->lge.ddic_ops->lge_check_vert_line_restore(panel);
	else
		pr_err("Can not find lge_check_vert_white_line");

	return ret;
}
static DEVICE_ATTR(check_line_restore, S_IWUSR | S_IWGRP, NULL, check_vert_line_restore);

static struct attribute *factory_attrs[] = {
	&dev_attr_panel_type.attr,
	&dev_attr_mfts_auto_touch_test_mode.attr,
	NULL,
};

static struct attribute *line_detect_attrs[] = {
	&dev_attr_check_black_line.attr,
	&dev_attr_check_white_line.attr,
	&dev_attr_check_line_restore.attr,
	NULL,
};

static const struct attribute_group factory_attr_group = {
	.name	= "factory",
	.attrs	= factory_attrs,
};

static const struct attribute_group line_detect_attr_group = {
	.name	= "factory",
	.attrs	= line_detect_attrs,
};

void lge_panel_factory_create_sysfs(struct dsi_panel *panel, struct device *panel_sysfs_dev)
{
	if (panel_sysfs_dev) {
		if (sysfs_create_group(&panel_sysfs_dev->kobj, &factory_attr_group) < 0)
			pr_err("create factory group fail!");

		if (panel->lge.use_line_detect)
			if (sysfs_merge_group(&panel_sysfs_dev->kobj, &line_detect_attr_group) < 0)
				pr_err("merge line_detect group fail!");
	}
}
