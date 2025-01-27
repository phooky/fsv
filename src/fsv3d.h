/* fsv3d.h */

/* The "fsv" name in 3D */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * SPDX-FileCopyrightText: 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_FSV3D_H
	#error
#endif
#define FSV_FSV3D_H

// Tells whether an edge index describes a sharp or smooth corner. Sharp corners
// need duplicated vertices with different normals.
typedef enum EdgeSmoothness {
	SMOOTH,
	SHARP
} EdgeSmoothness;

static const RGBcolor fsv_colors[] = {
        /* f */
	{ 1.0, 0.0, 0.0 },

        /* s */
	{ 0.0, 0.875, 0.0 },

	/* v */
	{ 0.0, 0.0, 1.0 }
};


static const float _fsv_f_vertices[] = {
	-55.2518, 28.4173,
	-38.9928, 38.7770,
	-44.9027, 44.8841,
	-56.4668, 31.4495,
	-58.3533, 34.0661,
	-52.0277, 48.6891,
	-60.4317, 50.0000,
	-61.2950, 35.1799,
	-67.7858, 49.2433,
	-86.1870, 19.0648,
	-95.5396, 19.3525,
	-95.5396, 3.6691,
	-86.1870, 3.9568,
	-63.6984, 34.1514,
	-74.1807, 47.0157,
	-54.3885, 3.6691,
	-54.3885, 19.3525,
	-64.8921, 19.0648,
	-64.8921, 3.9568,
	-54.8201, -48.7050,
	-54.8201, -33.1655,
	-64.8921, -33.3093,
	-96.2590, -48.7050,
	-86.1870, -33.3093,
	-96.2590, -33.1655,
	-64.8921, 28.9928,
	-86.1870, 26.2590,
	-85.3610, 33.0642,
	-83.0322, 38.7823,
	-64.6949, 31.7479,
	-79.4245, 43.3813
};

static const int _fsv_f_triangles[] = {
	0, 1, 2,
	3, 0, 2,
	4, 3, 2,
	4, 2, 5,
	4, 5, 6,
	7, 4, 6,
	7, 6, 8,
	9, 10, 11,
	9, 11, 12,
	13, 7, 8,
	13, 8, 14,
	15, 16, 17,
	18, 15, 17,
	19, 20, 21,
	22, 19, 21,
	23, 24, 22,
	23, 22, 21,
	23, 21, 18,
	12, 23, 18,
	12, 18, 17,
	9, 12, 17,
	9, 17, 25,
	26, 9, 25,
	27, 26, 25,
	28, 27, 25,
	28, 25, 29,
	30, 28, 29,
	14, 30, 29,
	13, 14, 29,

	-1, -1, -1
};

static const int _fsv_f_edges[] = {
	0, 1, 2, 5, 6, 8, 14, 30, 28, 27, 26, 9, 10, 11, 12, 23, 24, 22,
	19, 20, 21, 18, 15, 16, 17, 25, 29, 13, 7, 4, 3, 0, -1
};

static const EdgeSmoothness _fsv_f_edge_smooth[] = {
	SHARP, SHARP, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH,
	SMOOTH, SMOOTH /* index 26 */, SHARP, SHARP, SHARP, SHARP, SHARP,
	SHARP, SHARP, SHARP, SHARP, SHARP, SHARP, SHARP, SHARP, SHARP /* 17 */,
	SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SHARP
};


static const float _fsv_s_vertices[] = {
	-26.6187, -24.2446,
	-43.0216, -24.2446,
	-43.0216, -48.7050,
	-24.3592, -31.1564,
	-4.6043, -1.6547,
	11.0791, -1.6547,
	11.0791, 19.2087,
	-3.8849, 19.2087,
	-5.3237, 16.1871,
	-7.5193, 2.7418,
	-12.2888, 5.0280,
	-10.1785, 18.6491,
	-17.6978, 5.6835,
	-15.2891, 20.0560,
	-20.7194, 20.5036,
	-21.5774, 5.4490,
	-26.9704, 19.8322,
	-25.7128, 4.1913,
	-33.0296, 17.8178,
	-38.4173, 14.4604,
	-42.0997, 10.5809,
	-27.4820, 1.0791,
	-44.5670, 5.8700,
	-45.4676, 0.3597,
	-44.7748, -5.2784,
	-42.6112, -10.3091,
	-26.0911, -1.6280,
	-38.8489, -14.6043,
	-29.1074, -18.9262,
	-23.2614, -3.1202,
	-16.7439, -22.1290,
	-20.1439, -3.8129,
	-10.8766, -5.2784,
	-10.6475, -23.3813,
	-1.2577, -7.5753,
	-7.3115, -24.3512,
	7.3381, -11.4388,
	-4.2952, -25.9606,
	11.7772, -15.3131,
	14.7455, -20.4663,
	-2.5899, -28.8489,
	15.8273, -27.1223,
	14.9107, -33.7463,
	12.3315, -39.2513,
	-4.0021, -33.0322,
	8.3453, -43.6691,
	2.6965, -47.1649,
	-3.9435, -49.2859,
	-8.4199, -35.0733,
	-11.2230, -50.0000,
	-13.0935, -35.6115,
	-16.5254, -49.4671,
	-19.0941, -34.6469,
	-21.7000, -48.0389,
	-26.6187, -45.9712,
	-26.9065, -48.7050
};

static const int _fsv_s_triangles[] = {
	0, 1, 2,
	3, 0, 2,
	4, 5, 6,
	4, 6, 7,
	4, 7, 8,
	9, 4, 8,
	10, 9, 8,
	10, 8, 11,
	12, 10, 11,
	12, 11, 13,
	12, 13, 14,
	15, 12, 14,
	15, 14, 16,
	17, 15, 16,
	17, 16, 18,
	17, 18, 19,
	17, 19, 20,
	21, 17, 20,
	21, 20, 22,
	21, 22, 23,
	21, 23, 24,
	21, 24, 25,
	26, 21, 25,
	26, 25, 27,
	26, 27, 28,
	29, 26, 28,
	29, 28, 30,
	31, 29, 30,
	32, 31, 30,
	32, 30, 33,
	34, 32, 33,
	34, 33, 35,
	36, 34, 35,
	36, 35, 37,
	38, 36, 37,
	39, 38, 37,
	39, 37, 40,
	41, 39, 40,
	42, 41, 40,
	43, 42, 40,
	43, 40, 44,
	45, 43, 44,
	46, 45, 44,
	47, 46, 44,
	47, 44, 48,
	49, 47, 48,
	49, 48, 50,
	51, 49, 50,
	51, 50, 52,
	53, 51, 52,
	54, 53, 52,
	54, 52, 3,
	54, 3, 2,
	54, 2, 55,

	-1, -1, -1
};

static const int _fsv_s_edges[] = {
	0, 1, 2, 55, 54, 53, 51, 49, 47, 46, 45, 43, 42, 41, 39, 38, 36,
	34, 32, 31, 29, 26, 21, 17, 15, 12, 10, 9, 4, 5, 6, 7, 8, 11, 13,
	14, 16, 18, 19, 20, 22, 23, 24, 25, 27, 28, 30, 33, 35, 37, 40,
	44, 48, 50, 52, 3, 0, -1
};

static const EdgeSmoothness _fsv_s_edge_smooth[] = {
	SHARP, SHARP, SHARP, SHARP, SHARP, SMOOTH, SMOOTH, SMOOTH, SMOOTH,
	SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH /* 38 */,
	SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH /* 15 */,
	SMOOTH, SMOOTH, SMOOTH, SHARP /* 4 */, SHARP, SHARP, SHARP, SHARP /* 8 */,
	SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH,
	SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH,
	SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH, SHARP
};


static const float _fsv_v_vertices[] = {
	63.8849, 19.3525,
	63.8849, 3.8130,
	71.9425, 3.9568,
	98.4173, 19.3525,
	49.3525, 3.9568,
	57.5540, 3.8130,
	57.5540, 19.3525,
	18.1295, 19.3525,
	88.9209, 3.9568,
	98.4173, 3.8130,
	68.0576, -48.7050,
	59.8561, -28.2734,
	49.3525, -48.7050,
	27.9137, 3.9568,
	18.1295, 3.8130
};

static const int _fsv_v_triangles[] = {
	0, 1, 2,
	3, 0, 2,
	4, 5, 6,
	4, 6, 7,
	8, 9, 3,
	8, 3, 2,
	10, 8, 2,
	10, 2, 11,
	12, 10, 11,
	13, 12, 11,
	13, 11, 4,
	13, 4, 7,
	13, 7, 14,

	-1, -1, -1
};

static const int _fsv_v_edges[] = {
	0, 1, 2, 11, 4, 5, 6, 7, 14, 13, 12, 10, 8, 9, 3, 0, -1
};

static const EdgeSmoothness _fsv_v_edge_smooth[] = {
	SHARP, SHARP, SHARP, SHARP, SHARP, SHARP, SHARP, SHARP, SHARP, SHARP,
	SHARP, SHARP, SHARP, SHARP, SHARP, SHARP
};

static const float *fsv_vertices[] = {
	_fsv_f_vertices, _fsv_s_vertices, _fsv_v_vertices
};

static const int *fsv_triangles[] = {
	_fsv_f_triangles, _fsv_s_triangles, _fsv_v_triangles
};

static const int *fsv_edges[] = {
	_fsv_f_edges, _fsv_s_edges, _fsv_v_edges
};

static const EdgeSmoothness *fsv_edge_smoothness[] = {
	_fsv_f_edge_smooth, _fsv_s_edge_smooth, _fsv_v_edge_smooth
};


/* end fsv3d.h */
