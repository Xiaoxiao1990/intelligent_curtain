//
// Created by root on 3/22/20.
//

#ifndef INTELLIGENT_CURTAIN_MOTOR_CONTROLLER_H
#define INTELLIGENT_CURTAIN_MOTOR_CONTROLLER_H

typedef enum {
    MOTOR_STATE_STOP,
    MOTOR_STATE_FORWARD,
    MOTOR_STATE_BACKWARD,

    MOTOR_STATE_INVAILID
} motor_state_t;

typedef enum {
    MOTOR_RUN_FORWARD,
    MOTOR_RUN_BACKWORK
} direction_t;

void motor_init(void);

void motor_run(direction_t direction, float speed);

void motor_controller_unit_test(void);

#endif //INTELLIGENT_CURTAIN_MOTOR_CONTROLLER_H
