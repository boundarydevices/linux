/*
 * drivers/amlogic/media/camera/common/config_parser.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef CONFIG_PARSER
#define CONFIG_PARSER

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>

#define EFFECT_ITEM_MAX 16
#define AET_ITEM_MAX 32
#define HW_ITEM_MAX 16
#define WB_ITEM_MAX 10
#define CAPTURE_ITEM_MAX 9
#define NR_ITEM_MAX 5
#define PEAKING_ITEM_MAX 5
#define LENS_ITEM_MAX 5
#define SCENE_ITEM_MAX 1
#define EFFECT_MAX 18
#define HW_MAX 64
#define WB_MAX 2
#define GAMMA_MAX 257
#define SCENE_MAX 281
#define WB_SENSOR_MAX 4
#define CAPTURE_MAX 8
#define LENS_MAX 1027
#define WAVE_MAX 12
#define CM_MAX 188
#define NR_MAX 15
#define PEAKING_MAX 35
#define AE_LEN 119
#define AWB_LEN 120
#define AF_LEN	42
#define BUFFER_SIZE 1024

enum error_code {
	NO_MEM = 1,
	READ_ERROR,
	WRONG_FORMAT,
	CHECK_LEN_FAILED,
	CHECK_FAILED,
	HEAD_FAILED,
	BODY_HEAD_FAILED,
	BODY_ELEMENT_FAILED,
};

struct effect_type {
	int num;
	char name[40];
	unsigned int export[EFFECT_MAX];
};

struct effect_struct {
	int sum;
	struct effect_type eff[EFFECT_ITEM_MAX];
};

struct hw_type {
	int num;
	char name[40];
	int export[HW_MAX];
};

struct hw_struct {
	int sum;
	struct hw_type hw[HW_ITEM_MAX];
};

struct wb_type {
	int num;
	char name[40];
	int export[2];
};

struct wb_struct {
	int sum;
	struct wb_type wb[WB_ITEM_MAX];
};

struct scene_type {
	int num;
	char name[40];
	int export[SCENE_MAX];
};

struct scene_struct {
	int sum;
	struct scene_type scene[SCENE_ITEM_MAX];
};

struct capture_type {
	int num;
	char name[40];
	int export[CAPTURE_ITEM_MAX];
};

struct capture_struct {
	int sum;
	struct capture_type capture[CAPTURE_MAX];
};

struct sensor_aet_s {
	unsigned int exp;
	unsigned int ag;
	unsigned int vts;
	unsigned int gain;
	unsigned int fr;
};
/* sensor_aet_t */

struct sensor_aet_info_s {
	unsigned int fmt_main_fr;
	unsigned int fmt_capture; /* false: preview, true: capture */
	unsigned int fmt_hactive;
	unsigned int fmt_vactive;
	unsigned int fmt_rated_fr;
	unsigned int fmt_min_fr;
	unsigned int tbl_max_step;
	unsigned int tbl_rated_step;
	unsigned int tbl_max_gain;
	unsigned int tbl_min_gain;
	unsigned int format_transfer_parameter;
};
/* sensor_aet_info_t */

struct aet_type {
	int num;
	char name[40];
	struct sensor_aet_info_s *info;
	struct sensor_aet_s *aet_table;
};

struct aet_struct {
	int sum;
	struct aet_type aet[AET_ITEM_MAX];
};

struct wave_struct {
	int export[WAVE_MAX];
};

struct lens_type {
	int num;
	char name[40];
	int export[LENS_MAX];
};

struct lens_struct {
	int sum;
	struct lens_type lens[LENS_ITEM_MAX];
};

struct gamma_struct {
	unsigned int gamma_r[GAMMA_MAX];
	unsigned int gamma_g[GAMMA_MAX];
	unsigned int gamma_b[GAMMA_MAX];
};

struct wb_sensor_struct {
	int export[WB_SENSOR_MAX];
};

struct version_struct {
	char date[40];
	char module[30];
	char version[30];
};

struct cm_struct {
	int export[CM_MAX];
};

struct nr_type {
	int num;
	char name[40];
	int export[NR_MAX];
};

struct nr_struct {
	int sum;
	struct nr_type nr[NR_ITEM_MAX];
};

struct peaking_type {
	int num;
	char name[40];
	int export[PEAKING_MAX];
};

struct peaking_struct {
	int sum;
	struct peaking_type peaking[PEAKING_ITEM_MAX];
};

struct configure_s {
	struct effect_struct eff;
	int effect_valid;
	struct hw_struct hw;
	int hw_valid;
	struct aet_struct aet;
	int aet_valid;
	struct capture_struct capture;
	int capture_valid;
	struct scene_struct scene;
	int scene_valid;
	struct wb_struct wb;
	int wb_valid;
	struct wave_struct wave;
	int wave_valid;
	struct lens_struct lens;
	int lens_valid;
	struct gamma_struct gamma;
	int gamma_valid;
	struct wb_sensor_struct wb_sensor_data;
	int wb_sensor_data_valid;
	struct version_struct version;
	int version_info_valid;
	struct cm_struct cm;
	int cm_valid;
	struct nr_struct nr;
	int nr_valid;
	struct peaking_struct peaking;
	int peaking_valid;
};

struct para_index_s {
	unsigned int effect_index;
	unsigned int scenes_index;
	unsigned int wb_index;
	unsigned int capture_index;
	unsigned int nr_index;
	unsigned int peaking_index;
	unsigned int lens_index;
};

struct wb_pair_t {
	enum camera_wb_flip_e wb;
	char *name;
};

struct effect_pair_t {
	enum camera_special_effect_e effect;
	char *name;
};

struct sensor_dg_s {
	unsigned short r;
	unsigned short g;
	unsigned short b;
	unsigned short dg_default;
};

struct camera_priv_data_s {
	struct sensor_aet_info_s
		*sensor_aet_info; /* point to 1 of up to 16 aet information */
	struct sensor_aet_s *sensor_aet_table;
	unsigned int sensor_aet_step; /* current step of the current aet */
	struct configure_s *configure;
};

int parse_config(const char *path, struct configure_s *cf);
int generate_para(struct cam_parameter_s *para, struct para_index_s pindex,
		  struct configure_s *cf);
void free_para(struct cam_parameter_s *para);
int update_fmt_para(int width, int height, struct cam_parameter_s *para,
		    struct para_index_s *pindex, struct configure_s *cf);

unsigned int get_aet_current_step(void *priv);
unsigned int get_aet_current_gain(void *pirv);
unsigned int get_aet_min_gain(void *priv);
unsigned int get_aet_max_gain(void *priv);
unsigned int get_aet_max_step(void *priv);
unsigned int get_aet_gain_by_step(void *priv, unsigned int new_step);

int my_i2c_put_byte(struct i2c_adapter *adapter, unsigned short i2c_addr,
		    unsigned short addr, unsigned char data);
int my_i2c_put_byte_add8(struct i2c_adapter *adapter, unsigned short i2c_addr,
			 char *buf, int len);
int my_i2c_get_byte(struct i2c_adapter *adapter, unsigned short i2c_addr,
		    unsigned short addr);
int my_i2c_get_word(struct i2c_adapter *adapter, unsigned short i2c_addr);
#endif

