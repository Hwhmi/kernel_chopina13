#ifndef _OPLUS_DISPLAY_COMPAT_H_
#define _OPLUS_DISPLAY_COMPAT_H_

struct drm_device;

extern unsigned long oplus_display_brightness;

int oplus_display_compat_init(struct drm_device *drm);
void oplus_display_compat_deinit(void);

#endif /* _OPLUS_DISPLAY_COMPAT_H_ */
