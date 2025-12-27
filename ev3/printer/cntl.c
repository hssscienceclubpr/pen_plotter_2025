#include "ev3api.h"
#include "app.h"

static void reset_cntl(cntl_t *state) {
    state->kp = 0.0;
    state->kd = 0.0;
    state->waitD = 0;
    state->old_err = 0.0;
    state->old_err_empty = true;

    state->ki = 0.0;
    state->li = 1.0;
    state->sum_err = 0.0;
    state->kc = 0.0;
    state->row_pass_d = 0.0;
    state->row_pass_d_time = 0;
    state->use_row_pass_d = false;
}

void c_init_cntl_PD(cntl_t *state, float kp, float kd, int waitD) {
    reset_cntl(state);

    state->kp = kp;
    state->kd = kd;
    state->waitD = waitD;
    state->old_err_empty = true;
}
void c_init_cntl_PID(cntl_t *state, float kp, float ki, float kd, float li, int waitD) {
    reset_cntl(state);

    state->kp = kp;
    state->ki = ki;
    state->li = li;
    state->kd = kd;
    state->waitD = waitD;
    state->old_err_empty = true;
}
void c_init_cntl_PC(cntl_t *state, float kp, float kc) {
    reset_cntl(state);

    state->kp = kp;
    state->kc = kc;
}
void c_init_cntl_PIC(cntl_t *state, float kp, float ki, float kc, float li, int waitD) {
    reset_cntl(state);

    state->kp = kp;
    state->kc = kc;
    state->ki = ki;
    state->li = li;
    state->waitD = waitD;
}
void c_init_cntl_PCD(cntl_t *state, float kp, float kc, float kd, int waitD) {
    reset_cntl(state);

    state->kp = kp;
    state->kc = kc;
    state->kd = kd;
    state->waitD = waitD;
    state->old_err_empty = true;
}
void c_init_cntl_PD_rowpass(cntl_t *state, float kp, float kd, int t, int waitD) {
    reset_cntl(state);

    state->kp = kp;
    state->kd = kd;
    state->row_pass_d_time = t;
    state->use_row_pass_d = true;
    state->waitD = waitD;
    state->old_err_empty = true;
}
void c_init_cntl_PID_rowpass(cntl_t *state, float kp, float ki, float kd, float li, int t, int waitD) {
    reset_cntl(state);

    state->kp = kp;
    state->ki = ki;
    state->li = li;
    state->kd = kd;
    state->row_pass_d_time = t;
    state->use_row_pass_d = true;
    state->waitD = waitD;
    state->old_err_empty = true;
}
void c_init_cntl_PCD_rowpass(cntl_t *state, float kp, float kc, float kd, int t, int waitD) {
    reset_cntl(state);

    state->kp = kp;
    state->kc = kc;
    state->kd = kd;
    state->row_pass_d_time = t;
    state->use_row_pass_d = true;
    state->waitD = waitD;
    state->old_err_empty = true;
}

float c_cntl_next(cntl_t *state, float err) {
    float result = 0;

    // P
    result += (state->kp) * err;

    // D
    if(state->kd){
        if(state->old_err_empty){
            state->old_err_empty = false;
        }else{
            if(state->use_row_pass_d){
                state->row_pass_d += ((err - state->old_err) - state->row_pass_d) / (state->row_pass_d_time);
                result += (state->kd) * (state->row_pass_d);
            }else{
                result += (state->kd) * (err - (state->old_err));
            }
        }
        state->old_err = err;
    }

    // I
    if(state->ki){
        state->sum_err += err;
        result += (state->ki) * (state->sum_err);
        state->sum_err *= (state->li);
    }
    
    // C
    if(state->kc){
        result += (state->kc) * (err*err*err);
    }

    return result;
}

void c_cntl_wait(cntl_t *state) {
    if(state->waitD){
        tslp_tsk(state->waitD);
    }
}

void c_cntl_skip(cntl_t *state) {
    state->old_err = 0;
    state->sum_err = 0;
    state->old_err_empty = true;
}
