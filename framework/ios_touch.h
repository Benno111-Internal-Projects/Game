#ifndef RENDER_FRAMEWORK_IOS_TOUCH_H
#define RENDER_FRAMEWORK_IOS_TOUCH_H

#include "input.h"

bool InitIosTouch(void);
void CloseIosTouch(void);
size_t ReadIosTouch(InputTouch *points, size_t capacity);

#endif
