#ifndef INK_APP_PHOTO_H
#define INK_APP_PHOTO_H
#include "ink/sys/image.h"
#include "ink/sys/ui.h"
/* UI only; transfers pixels to the image object's lifetime. */
void ink_photo_image(lv_obj_t *parent, ink_image_t *image);
#endif
