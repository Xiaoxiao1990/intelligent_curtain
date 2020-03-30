//
// Created by root on 3/22/20.
//

#ifndef INTELLIGENT_CURTAIN_MOTOR_CONTROLLER_H
#define INTELLIGENT_CURTAIN_MOTOR_CONTROLLER_H

typedef enum {
    MOTOR_STATE_STOP,
    MOTOR_STATE_FORWARD,
    MOTOR_STATE_BACKWARD,
    // Curtain adjust state
    MOTOR_STATE_ADJUST,
    MOTOR_ADJUST_BACK_TO_ORIGIN,
    MOTOR_ADJUST_ARRIVE_END,
    // Curtain position adjust
    MOTOR_POSITION_ADJUST,
    MOTOR_POSITION_ADJUST_LEFT,
    MOTOR_POSITION_ADJUST_RIGHT,

    MOTOR_STATE_INVAILID
} motor_state_t;

typedef enum {
    MOTOR_RUN_FORWARD,
    MOTOR_RUN_BACKWORK
} direction_t;

typedef enum {
    MOTOR_DISABLE,
    MOTOR_ENABLE
} motor_power_enable_t;

typedef struct {
    motor_state_t state;
    direction_t direction;
    float speed;
    motor_power_enable_t power_enable;
} motor_t;

extern motor_t motor;

void motor_init(void);
void motor_run(direction_t direction, float speed);
void motor_controller_task(void);
uint32_t motor_target_position(uint32_t position, bool set_or_get);
void motor_controller_unit_test(void);

#endif //INTELLIGENT_CURTAIN_MOTOR_CONTROLLER_H
