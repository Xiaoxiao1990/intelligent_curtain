//
// Created by root on 1/5/20.
//

#ifndef INTELLIGENT_CURTAIN_TOUCH_PAD_H
#define INTELLIGENT_CURTAIN_TOUCH_PAD_H

typedef void *tp_callback_func_t(void);

void red_led(int state);
void green_led(int state);

void touch_pad_initialize(void);

void touch_pad_unit_test(void);

#endif //INTELLIGENT_CURTAIN_TOUCH_PAD_H
