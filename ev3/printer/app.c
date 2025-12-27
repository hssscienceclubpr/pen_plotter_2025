/**
 * Hello EV3
 *
 * This is a program used to test the whole platform.
 */

#include "ev3api.h"
#include "app.h"
#include "math.h"
#include <unistd.h>
#include <ctype.h>
#include <string.h>

/*
ファイル構造：
N: 色の数 (1-64)
n: next polyline
e: end of color
color name: 64文字以下
x y: 座標(mm) float

N
color 0 name
color 1 name
...
color (N-1) name
color 0 data
color 1 data
...
color (N-1) data

color data:
x y
x y
...
n
x y
x y
...
n
...
e
*/

#define A4_WIDTH_MM (210.0)
#define A4_HEIGHT_MM (297.0)

#define Y0_MOTOR_PORT (EV3_PORT_A) // 右Lモーター
#define Y1_MOTOR_PORT (EV3_PORT_B) // 左Lモーター
#define X_MOTOR_PORT (EV3_PORT_C) // 左Mモーター
#define PEN_MOTOR_PORT (EV3_PORT_D) // 右Mモーター

#define PEN_BETWEEN_HALF_DEG (5474.0 * 0.5) // deg

float x = 0, y = 0; // Current position(mm)

//const float y_mm_to_deg = (2586.0 + 2096.0) / 275.5;
const float y_mm_to_deg = (25.5 + 4624) / 274.5;
const float x_mm_to_deg = (8620.0 + 17910.0) / 195.3;

const float y_adjust_speed_mm_per_sec = 10.0;
const float x_adjust_speed_mm_per_sec = 10.0;

typedef enum {
	LEFT_PEN = -1,
	RIGHT_PEN = 1,
	CENTER = 0 // pen is not used
} pen_mode_t;

pen_mode_t pen_mode = CENTER;
bool_t pen_is_down = false;

float my_min(float a, float b) {
	return a < b ? a : b;
}
float my_max(float a, float b) {
	return a > b ? a : b;
}

float diff_control(float target, int current) {
	if((int)round(target) == current) return 0;
	return target - current;
}

int to_valid_power(int power) {
	if(power > 100) return 100;
	if(power < -100) return -100;
	return power;
}

void pen_move_to(float angle) {
	cntl_t pen_cntl;
	c_init_cntl_PD(&pen_cntl, 5.0, 10.0, 0);
	const float pen_move_finish_err = 2.0; // deg
	const int timeout = 5000; // ms
	int time = 0;
	ev3_motor_stop(PEN_MOTOR_PORT, false);
	//ev3_motor_reset_counts(PEN_MOTOR_PORT);
	while(1){
		ev3_motor_set_power(PEN_MOTOR_PORT, to_valid_power(c_cntl_next(&pen_cntl, diff_control(angle, ev3_motor_get_counts(PEN_MOTOR_PORT)))));
		if(fabs(diff_control(angle, ev3_motor_get_counts(PEN_MOTOR_PORT))) < pen_move_finish_err){
			break;
		}
		tslp_tsk(10);
		time += 10;
		if(time >= timeout) {
			syslog(LOG_WARNING, "pen_move_to timeout");
			break;
		}
	}
	ev3_motor_stop(PEN_MOTOR_PORT, true);
}

/*static int pen_target_power = 0;
#define PEN_SAFE_POS (153.0) // deg

void pen_set_power_safe() {
	if(pen_is_down == false) {
		return; // do nothing
	}

	if(pen_target_power > 0){
		if(ev3_motor_get_counts(PEN_MOTOR_PORT) < PEN_SAFE_POS){
			ev3_motor_set_power(PEN_MOTOR_PORT, pen_target_power);
		}else{
			ev3_motor_stop(PEN_MOTOR_PORT, false);
		}
	}else if(pen_target_power < 0){
		if(ev3_motor_get_counts(PEN_MOTOR_PORT) > -PEN_SAFE_POS){
			ev3_motor_set_power(PEN_MOTOR_PORT, pen_target_power);
		}else{
			ev3_motor_stop(PEN_MOTOR_PORT, false);
		}
	}else{
		ev3_motor_stop(PEN_MOTOR_PORT, false);
	}
}

void pen_up() {
	if(pen_is_down == false) return;
	pen_is_down = false;

	if(pen_mode == CENTER) {
		; // do nothing
	}else if(pen_mode == LEFT_PEN) {
		pen_move_to(180.0);
	}else if(pen_mode == RIGHT_PEN) {
		pen_move_to(-180.0);
	}
}

void pen_down() {
	if(pen_is_down) return;
	pen_is_down = true;

	if(pen_mode == CENTER) {
		; // do nothing
	} else {
		pen_move_to(((int)pen_mode) * 80.0);
		if(pen_mode == LEFT_PEN) {
			pen_target_power = -10;
		} else if(pen_mode == RIGHT_PEN) {
			pen_target_power = 10;
		}
		ev3_motor_reset_counts(PEN_MOTOR_PORT);
		//tslp_tsk(250);
		for(int i=0; i<30; ++i){
			pen_set_power_safe();
			tslp_tsk(10);
		}
		if(pen_mode == LEFT_PEN) {
			pen_target_power = -7;
		} else if(pen_mode == RIGHT_PEN) {
			pen_target_power = 7;
		}
	}
}*/

void pen_set_power_safe() {
	; // do nothing
}

void pen_up() {
	if(pen_is_down == false) return;
	pen_is_down = false;

	if(pen_mode == CENTER) {
		; // do nothing
	}else if(pen_mode == LEFT_PEN) {
		pen_move_to(0.0);
	}else if(pen_mode == RIGHT_PEN) {
		pen_move_to(0.0);
	}
}

void pen_down() {
	if(pen_is_down) return;
	pen_is_down = true;

	if(pen_mode == CENTER) {
		; // do nothing
	} else if(pen_mode == LEFT_PEN) {
		pen_move_to(-145.0);
	} else if(pen_mode == RIGHT_PEN) {
		pen_move_to(145.0);
	}
}

void pen_set_mode(pen_mode_t mode) {
	if(pen_is_down) {
		pen_up();
	}
	pen_mode = mode;
}

// use when pen is up only
void goto_position(float target_x, float target_y) {
	cntl_t y0_cntl, y1_cntl;
	cntl_t y_cntl, y_diff_cntl;
	cntl_t x_cntl;
	float h, diff;
	float pen_diff = -((int)pen_mode) * PEN_BETWEEN_HALF_DEG; // deg

	float y0_deg, y1_deg, x_deg;

	//if(pen_is_down) return;

	x = target_x;
	y = target_y;

	c_init_cntl_PD(&y_cntl, 5.0, 15.0, 0);
	c_init_cntl_PD(&y_diff_cntl, 3.0, 15.0, 0);
	c_init_cntl_PD(&x_cntl, 6.0, 15.0, 0);
	const float first_move_finish_err = 5.0; // deg
	const int max_h = 60; // power
	while(1){
		y0_deg = ev3_motor_get_counts(Y0_MOTOR_PORT);
		y1_deg = ev3_motor_get_counts(Y1_MOTOR_PORT);
		x_deg = ev3_motor_get_counts(X_MOTOR_PORT);
		h = c_cntl_next(&y_cntl, diff_control(y*y_mm_to_deg, (y0_deg + y1_deg)*0.5));
		diff = c_cntl_next(&y_diff_cntl, diff_control(0.0, y0_deg - y1_deg));
		if(h > max_h) h = max_h;
		if(h < -max_h) h = -max_h;
		ev3_motor_set_power(Y0_MOTOR_PORT, to_valid_power(h + diff));
		ev3_motor_set_power(Y1_MOTOR_PORT, to_valid_power(h - diff));
		ev3_motor_set_power(X_MOTOR_PORT, to_valid_power(c_cntl_next(&x_cntl, diff_control(x*x_mm_to_deg + pen_diff, x_deg))));
		if(fabs(diff_control(y*y_mm_to_deg, y0_deg)) < first_move_finish_err &&
		   fabs(diff_control(y*y_mm_to_deg, y1_deg)) < first_move_finish_err &&
		   fabs(diff_control(x*x_mm_to_deg + pen_diff, x_deg)) < first_move_finish_err){
			break;
		}
		pen_set_power_safe();
		tslp_tsk(10);
	}
	c_init_cntl_PD(&y0_cntl, 3.5, 15.0, 0);
	c_init_cntl_PD(&y1_cntl, 3.5, 15.0, 0);
	c_init_cntl_PID(&x_cntl, 4.5, 0.000015, 13.0, 0.99, 0);
	const float fine_move_finish_err = 1.0; // deg
	while(1){
		y0_deg = ev3_motor_get_counts(Y0_MOTOR_PORT);
		y1_deg = ev3_motor_get_counts(Y1_MOTOR_PORT);
		x_deg = ev3_motor_get_counts(X_MOTOR_PORT);
		ev3_motor_set_power(Y0_MOTOR_PORT, to_valid_power(c_cntl_next(&y0_cntl, diff_control(y*y_mm_to_deg, y0_deg))));
		ev3_motor_set_power(Y1_MOTOR_PORT, to_valid_power(c_cntl_next(&y1_cntl, diff_control(y*y_mm_to_deg, y1_deg))));
		ev3_motor_set_power(X_MOTOR_PORT, to_valid_power(c_cntl_next(&x_cntl, diff_control(x*x_mm_to_deg + pen_diff, x_deg))));
		if(fabs(diff_control(y*y_mm_to_deg, y0_deg)) < fine_move_finish_err &&
		   fabs(diff_control(y*y_mm_to_deg, y1_deg)) < fine_move_finish_err &&
		   fabs(diff_control(x*x_mm_to_deg + pen_diff, x_deg)) < fine_move_finish_err){
			break;
		}
		pen_set_power_safe();
		tslp_tsk(10);
	}

	ev3_motor_stop(Y0_MOTOR_PORT, true);
	ev3_motor_stop(Y1_MOTOR_PORT, true);
	ev3_motor_stop(X_MOTOR_PORT, true);
}

#define MAX_COLOR (64)
#define MAX_COLOR_NAME_LENGTH (64)
#define MAX_POLYLINE_POINTS (8192)
int print_file(char filename[]) {
	FILE *fp;
	int current_line = 0;
	char str[128];
	static char color_names[MAX_COLOR][MAX_COLOR_NAME_LENGTH];
	static float x_points[MAX_POLYLINE_POINTS];
	static float y_points[MAX_POLYLINE_POINTS];
	int points_n;
	int color_n;
	int len;
	float old_x = 0, old_y = 0;
	float next_x, next_y;
	const int timer_once = 4; // ms
	int line_time, line_counts;
	cntl_t y0_cntl, y1_cntl, x_cntl;
	float y0_deg, y1_deg, x_deg;

	fp = fopen(filename, "r");
	if(fp == NULL) {
		syslog(LOG_ERROR, "Cannot open file: %s", filename);
		return -1;
	}

	fscanf(fp, "%d\n", &color_n);
	++current_line;
	if(color_n < 1 || color_n > MAX_COLOR) {
		fclose(fp);
		syslog(LOG_ERROR, "Invalid color n: %d", color_n);
		syslog(LOG_ERROR, "at line %d", current_line);
		return -1;
	}
	for(int i=0; i<color_n; ++i) {
		fgets(color_names[i], MAX_COLOR_NAME_LENGTH, fp);
		++current_line;
		len = strlen(color_names[i]);
		if(len > 0 && color_names[i][len-1] == '\n') {
			color_names[i][len-1] = '\0';
		}else{
			fclose(fp);
			syslog(LOG_ERROR, "Invalid color name: %s", color_names[i]);
			syslog(LOG_ERROR, "at line %d", current_line);
			return -1;
		}
	}

	for(int i=0; i<color_n; ++i){
		if(color_n == 1 && strcmp(color_names[0], "black") == 0) {
			pen_set_mode(RIGHT_PEN);
		}else if(color_n == 1 && strcmp(color_names[0], "red") == 0) {
			pen_set_mode(LEFT_PEN);
		}else if(color_n == 2 && strcmp(color_names[i], "red") == 0 && strcmp(color_names[1-i], "black") == 0) {
			pen_set_mode(LEFT_PEN);
		}else if(color_n == 2 && strcmp(color_names[i], "black") == 0 && strcmp(color_names[1-i], "red") == 0) {
			pen_set_mode(RIGHT_PEN);
		}else if(i % 2 == 0){
			ev3_lcd_fill_rect(0, 0, EV3_LCD_WIDTH, EV3_LCD_HEIGHT, EV3_LCD_WHITE);
			if(i == color_n - 1){
				sprintf(str, "Left Color: %s", color_names[i]);
				ev3_lcd_draw_string(str, 0, 0);
			}else{
				sprintf(str, "Left Color: %s", color_names[i]);
				ev3_lcd_draw_string(str, 0, 0);
				sprintf(str, "Right Color: %s", color_names[i+1]);
				ev3_lcd_draw_string(str, 0, 20);
			}
			ev3_speaker_play_tone(NOTE_C4, 100);
			tslp_tsk(100);
			while(ev3_button_is_pressed(ENTER_BUTTON)) {
				tslp_tsk(10);
			}
			while(ev3_button_is_pressed(ENTER_BUTTON) == false) {
				tslp_tsk(10);
			}
			pen_set_mode(LEFT_PEN);
		}else{
			pen_set_mode(RIGHT_PEN);
		}

		int next_color = 0;
		while(next_color == 0){
			points_n = 0;
			while(1){
				fgets(str, sizeof(str), fp);
				++current_line;
				if(strlen(str) == 0) {
					fclose(fp);
					syslog(LOG_ERROR, "empty line in polyline data");
					syslog(LOG_ERROR, "at line %d", current_line);
					return -1;
				}
				str[strlen(str)-1] = '\0';
				if(strcmp(str, "n") == 0){
					break;
				}else if(strcmp(str, "e") == 0){
					next_color = 1;
					break;
				}else{
					if(points_n >= MAX_POLYLINE_POINTS) {
						fclose(fp);
						syslog(LOG_ERROR, "too many points in polyline");
						syslog(LOG_ERROR, "at line %d", current_line);
						return -1;
					}
					if(sscanf(str, "%f %f", &x_points[points_n], &y_points[points_n]) != 2) {
						fclose(fp);
						syslog(LOG_ERROR, "invalid point data: %s", str);
						syslog(LOG_ERROR, "at line %d", current_line);
						return -1;
					}
					++points_n;
				}
			}

			float pen_diff = -((int)pen_mode) * PEN_BETWEEN_HALF_DEG; // deg

			if(points_n < 2){
				fclose(fp);
				syslog(LOG_ERROR, "not enough points in polyline: %d", points_n);
				return -1;
			}

			goto_position(x_points[0] - A4_WIDTH_MM * 0.5, A4_HEIGHT_MM * 0.5 - y_points[0]);
			pen_down();

			for(int j=1; j<points_n; ++j){
				old_x = x;
				old_y = y;
				next_x = x_points[j] - A4_WIDTH_MM * 0.5;
				next_y = A4_HEIGHT_MM * 0.5 - y_points[j];
				line_time = my_max(fabs(next_x - x) * 1.0, fabs(next_y - y) * 0.4) / 10.0 * 1000.0; // ms
				line_counts = line_time / timer_once + 1;

				c_init_cntl_PID(&y0_cntl, 3.7, 0.000005, 20.0, 0.99, 0);
				c_init_cntl_PID(&y1_cntl, 3.7, 0.000005, 20.0, 0.99, 0);
				c_init_cntl_PID(&x_cntl , 4.0, 0.000005, 15.0, 0.99, 0);
				for(int k=0; k<line_counts; ++k){
					y0_deg = ev3_motor_get_counts(Y0_MOTOR_PORT);
					y1_deg = ev3_motor_get_counts(Y1_MOTOR_PORT);
					x_deg = ev3_motor_get_counts(X_MOTOR_PORT);
					x = old_x + (next_x - old_x) * (k + 1) / line_counts;
					y = old_y + (next_y - old_y) * (k + 1) / line_counts;
					ev3_motor_set_power(Y0_MOTOR_PORT, to_valid_power(c_cntl_next(&y0_cntl, diff_control(y*y_mm_to_deg, y0_deg))));
					ev3_motor_set_power(Y1_MOTOR_PORT, to_valid_power(c_cntl_next(&y1_cntl, diff_control(y*y_mm_to_deg, y1_deg))));
					ev3_motor_set_power(X_MOTOR_PORT, to_valid_power(c_cntl_next(&x_cntl, diff_control(x*x_mm_to_deg + pen_diff, x_deg))));
					pen_set_power_safe();
					tslp_tsk(timer_once);
				}

				if((j > 0 && j < points_n - 1) || j == points_n - 1) {
					// 角度が急(鋭角)な場合はペンが目標値に到達するまで待つ
					float ax = x_points[j-1] - x_points[j];
					float ay = y_points[j-1] - y_points[j];
					float bx = x_points[j+1] - x_points[j];
					float by = y_points[j+1] - y_points[j];
					if(ax * bx + ay * by > -0.0001) { // 内積で鋭角か判定
						while(1){
							y0_deg = ev3_motor_get_counts(Y0_MOTOR_PORT);
							y1_deg = ev3_motor_get_counts(Y1_MOTOR_PORT);
							x_deg = ev3_motor_get_counts(X_MOTOR_PORT);
							ev3_motor_set_power(Y0_MOTOR_PORT, to_valid_power(c_cntl_next(&y0_cntl, diff_control(y*y_mm_to_deg, y0_deg))));
							ev3_motor_set_power(Y1_MOTOR_PORT, to_valid_power(c_cntl_next(&y1_cntl, diff_control(y*y_mm_to_deg, y1_deg))));
							ev3_motor_set_power(X_MOTOR_PORT, to_valid_power(c_cntl_next(&x_cntl, diff_control(x*x_mm_to_deg + pen_diff, x_deg))));
							if(fabs(diff_control(y*y_mm_to_deg, y0_deg)) < 2.0 &&
								fabs(diff_control(y*y_mm_to_deg, y1_deg)) < 2.0 &&
								fabs(diff_control(x*x_mm_to_deg + pen_diff, x_deg)) < 2.0){
								break;
							}
							pen_set_power_safe();
							tslp_tsk(timer_once);
						}
					}
				}

				x = next_x;
				y = next_y;
			}

			ev3_motor_stop(Y0_MOTOR_PORT, true);
			ev3_motor_stop(Y1_MOTOR_PORT, true);
			ev3_motor_stop(X_MOTOR_PORT, true);
			//tslp_tsk(100);
			for(int i=0; i<10; ++i){
				pen_set_power_safe();
				tslp_tsk(10);
			}

			pen_up();
		}
	}

	pen_set_mode(CENTER);
	goto_position(0.0, 0.0);

	fclose(fp);
	syslog(LOG_INFO, "Finished printing %s", filename);
	return 0;
}

void main_task(intptr_t unused) {
	ev3_motor_config(Y0_MOTOR_PORT, LARGE_MOTOR);
	ev3_motor_config(Y1_MOTOR_PORT, LARGE_MOTOR);
	ev3_motor_config(X_MOTOR_PORT, MEDIUM_MOTOR);
	ev3_motor_config(PEN_MOTOR_PORT, MEDIUM_MOTOR);

	ev3_speaker_set_volume(3);
	ev3_lcd_set_font(EV3_FONT_MEDIUM);

	tslp_tsk(1000);

	ev3_motor_reset_counts(Y0_MOTOR_PORT);
	ev3_motor_reset_counts(Y1_MOTOR_PORT);
	ev3_motor_reset_counts(X_MOTOR_PORT);

	ev3_speaker_play_tone(NOTE_C4, 100);
	tslp_tsk(100);

	cntl_t y0_cntl, y1_cntl, x_cntl;
	c_init_cntl_PD(&y0_cntl, 3.5, 15.0, 0);
	c_init_cntl_PD(&y1_cntl, 3.5, 15.0, 0);
	c_init_cntl_PID(&x_cntl, 4.0, 0.000015, 13.0, 0.99, 0);

	char str[32];

	while(ev3_button_is_pressed(ENTER_BUTTON)) {
		tslp_tsk(10);
	}
	ev3_lcd_draw_string("Adjust position", 0, 0);
	while(ev3_button_is_pressed(ENTER_BUTTON) == false) {
		if(ev3_button_is_pressed(UP_BUTTON)) {
			y += y_adjust_speed_mm_per_sec * 0.01;
		}else if(ev3_button_is_pressed(DOWN_BUTTON)) {
			y -= y_adjust_speed_mm_per_sec * 0.01;
		}
		if(ev3_button_is_pressed(RIGHT_BUTTON)) {
			x += x_adjust_speed_mm_per_sec * 0.01;
		}else if(ev3_button_is_pressed(LEFT_BUTTON)) {
			x -= x_adjust_speed_mm_per_sec * 0.01;
		}
		ev3_motor_set_power(Y0_MOTOR_PORT, to_valid_power(c_cntl_next(&y0_cntl, diff_control(y*y_mm_to_deg, ev3_motor_get_counts(Y0_MOTOR_PORT)))));
		ev3_motor_set_power(Y1_MOTOR_PORT, to_valid_power(c_cntl_next(&y1_cntl, diff_control(y*y_mm_to_deg, ev3_motor_get_counts(Y1_MOTOR_PORT)))));
		sprintf(str, "Y: %d,%d deg", (int)ev3_motor_get_counts(Y0_MOTOR_PORT), (int)ev3_motor_get_counts(Y1_MOTOR_PORT));
		ev3_lcd_draw_string(str, 0, 20);
		ev3_motor_set_power(X_MOTOR_PORT, to_valid_power(c_cntl_next(&x_cntl, diff_control(x*x_mm_to_deg, ev3_motor_get_counts(X_MOTOR_PORT)))));
		sprintf(str, "X: %d deg", (int)ev3_motor_get_counts(X_MOTOR_PORT));
		ev3_lcd_draw_string(str, 0, 40);

		tslp_tsk(10);
	}

	ev3_motor_stop(Y0_MOTOR_PORT, true);
	ev3_motor_stop(Y1_MOTOR_PORT, true);
	ev3_motor_stop(X_MOTOR_PORT, true);

	ev3_speaker_play_tone(NOTE_C4, 100);
	tslp_tsk(100);

	while(ev3_button_is_pressed(ENTER_BUTTON)) {
		tslp_tsk(10);
	}
	ev3_lcd_fill_rect(0, 0, EV3_LCD_WIDTH, EV3_LCD_HEIGHT, EV3_LCD_WHITE);
	ev3_lcd_draw_string("Adjust pen", 0, 0);
	while(ev3_button_is_pressed(ENTER_BUTTON) == false) {
		if(ev3_button_is_pressed(UP_BUTTON)) {
			ev3_motor_set_power(PEN_MOTOR_PORT, 5);
		}else if(ev3_button_is_pressed(DOWN_BUTTON)) {
			ev3_motor_set_power(PEN_MOTOR_PORT, -5);
		}else{
			ev3_motor_stop(PEN_MOTOR_PORT, true);
		}
		sprintf(str, "PEN: %d deg", (int)ev3_motor_get_counts(PEN_MOTOR_PORT));
		ev3_lcd_draw_string(str, 0, 20);
	}

	ev3_motor_stop(PEN_MOTOR_PORT, true);

	ev3_speaker_play_tone(NOTE_C4, 100);
	tslp_tsk(100);

	ev3_motor_reset_counts(X_MOTOR_PORT);
	ev3_motor_reset_counts(Y0_MOTOR_PORT);
	ev3_motor_reset_counts(Y1_MOTOR_PORT);
	ev3_motor_reset_counts(PEN_MOTOR_PORT);

	x = 0.0;
	y = 0.0;

	print_file("ev3rt/print_data/path.txt");

	ext_tsk();
}
