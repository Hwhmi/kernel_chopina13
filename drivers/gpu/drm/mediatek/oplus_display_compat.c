#include <drm/drmP.h>
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>

#include "mtk_drm_crtc.h"
#include "oplus_display_compat.h"

#define OPLUS_MIN_BRIGHTNESS 0
#define OPLUS_NORMAL_MAX_BRIGHTNESS 2047
#define OPLUS_MAX_BRIGHTNESS 4095

unsigned long oplus_display_brightness;
EXPORT_SYMBOL(oplus_display_brightness);

static struct drm_device *oplus_drm_device;
static struct kobject *oplus_display_kobj;
static unsigned int hbm_mode;
static unsigned int aod_light_mode;

static struct drm_crtc *oplus_get_primary_crtc(void)
{
	if (!oplus_drm_device ||
	    list_empty(&oplus_drm_device->mode_config.crtc_list))
		return NULL;

	return list_first_entry(&oplus_drm_device->mode_config.crtc_list,
				struct drm_crtc, head);
}

static ssize_t oplus_display_get_brightness(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", oplus_display_brightness);
}

static ssize_t oplus_display_set_brightness(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int level = 0;
	struct drm_crtc *crtc;
	int ret;

	ret = kstrtouint(buf, 10, &level);
	if (ret)
		return ret;

	if (level < OPLUS_MIN_BRIGHTNESS || level > OPLUS_MAX_BRIGHTNESS)
		return -EINVAL;

	crtc = oplus_get_primary_crtc();
	if (!crtc)
		return -ENODEV;

	ret = oplus_mtk_drm_setbacklight(crtc, level);
	if (!ret)
		oplus_display_brightness = level;

	return ret ? ret : count;
}

static ssize_t oplus_display_get_max_brightness(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", OPLUS_MAX_BRIGHTNESS);
}

static ssize_t oplus_display_get_normal_max_brightness(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", OPLUS_NORMAL_MAX_BRIGHTNESS);
}

static ssize_t oplus_display_get_hbm(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", hbm_mode);
}

static ssize_t oplus_display_set_hbm(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int mode = 0;
	struct drm_crtc *crtc;
	int ret;

	ret = kstrtouint(buf, 10, &mode);
	if (ret)
		return ret;

	crtc = oplus_get_primary_crtc();
	if (!crtc)
		return -ENODEV;

	ret = mtk_drm_crtc_set_panel_hbm(crtc, !!mode);
	if (!ret) {
		mtk_drm_crtc_hbm_wait(crtc, !!mode);
		hbm_mode = !!mode;
	}

	return ret ? ret : count;
}

static ssize_t oplus_display_get_aod_light_mode(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", aod_light_mode);
}

static ssize_t oplus_display_set_aod_light_mode(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int mode = 0;
	struct drm_crtc *crtc;
	int ret;

	ret = kstrtouint(buf, 10, &mode);
	if (ret)
		return ret;

	crtc = oplus_get_primary_crtc();
	if (!crtc)
		return -ENODEV;

	ret = mtk_drm_aod_setbacklight(crtc, mode);
	if (!ret)
		aod_light_mode = mode;

	return ret ? ret : count;
}

static struct kobj_attribute oplus_brightness_attr =
	__ATTR(oplus_brightness, 0664, oplus_display_get_brightness,
	       oplus_display_set_brightness);
static struct kobj_attribute oplus_max_brightness_attr =
	__ATTR(oplus_max_brightness, 0444, oplus_display_get_max_brightness,
	       NULL);
static struct kobj_attribute max_brightness_attr =
	__ATTR(max_brightness, 0444, oplus_display_get_normal_max_brightness,
	       NULL);
static struct kobj_attribute hbm_attr =
	__ATTR(hbm, 0664, oplus_display_get_hbm, oplus_display_set_hbm);
static struct kobj_attribute aod_light_mode_set_attr =
	__ATTR(aod_light_mode_set, 0664, oplus_display_get_aod_light_mode,
	       oplus_display_set_aod_light_mode);

static struct attribute *oplus_display_attrs[] = {
	&oplus_brightness_attr.attr,
	&oplus_max_brightness_attr.attr,
	&max_brightness_attr.attr,
	&hbm_attr.attr,
	&aod_light_mode_set_attr.attr,
	NULL,
};

static const struct attribute_group oplus_display_attr_group = {
	.attrs = oplus_display_attrs,
};

int oplus_display_compat_init(struct drm_device *drm)
{
	int ret;

	oplus_drm_device = drm;

	if (oplus_display_kobj)
		return 0;

	oplus_display_kobj = kobject_create_and_add("oplus_display", kernel_kobj);
	if (!oplus_display_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(oplus_display_kobj, &oplus_display_attr_group);
	if (ret) {
		kobject_put(oplus_display_kobj);
		oplus_display_kobj = NULL;
		oplus_drm_device = NULL;
		return ret;
	}

	pr_info("oplus_display: compat sysfs created\n");
	return 0;
}

void oplus_display_compat_deinit(void)
{
	if (oplus_display_kobj) {
		sysfs_remove_group(oplus_display_kobj,
				   &oplus_display_attr_group);
		kobject_put(oplus_display_kobj);
		oplus_display_kobj = NULL;
	}

	oplus_drm_device = NULL;
}
