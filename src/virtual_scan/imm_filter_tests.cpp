#include <carmen/carmen.h>
#include <carmen/moving_objects_messages.h>
#include <carmen/velodyne_interface.h>
#include <carmen/matrix.h>
#include <iostream>
#include "virtual_scan.h"

using namespace std;

#define IMM_FILTER

#define CV_MODEL
#define CA_MODEL
#define CT_MODEL

//	[1] Joana Barbosa Bastos Gomes, "An Overview on Target Tracking Using Multiple Model Methods"
//	[2] Vanesa Ibanez Llanos, "Filtro IMM para Sistema de Vigilancia Aeroportuaria A-SMGCS"
//	[3] Yaakov Bar-Shalom, X.-Rong Li, Thiagalingam Kirubarajan, "Estimation with Applications To Tracking and Navigation"
//	[4] Anthony F. Genovese, "The Interacting Multiple Model Algorithm for Accurate State Estimation of Maneuvering Targets"
//  [5] Karl Granstrom, Peter Willett, Yaakov Bar-Shalom, "Systematic approach to IMM mixing for unequal dimension states"

#ifdef IMM_FILTER
#define r 3 // Numero de modelos
double u_k[r] = {1.0/3.0, 1.0/3.0, 1.0/3.0};
//static double p[r][r] = {
//		{0.950, 0.050, 0.001},
//		{0.050, 0.950, 0.001},
//		{0.500, 0.500, 0.001}};
static double p[r][r] = {
		{0.950, 0.010, 0.040},
		{0.010, 0.950, 0.040},
		{0.025, 0.025, 0.950}};
//static double p[r][r] = {
//		{0.800, 0.050, 0.150},
//		{0.050, 0.800, 0.150},
//		{0.025, 0.025, 0.950}};
#endif


/* This function draws an ellipse with a given mean and covariance parameters.
 It does this using PCA.  It figures out the eigenvectors and eigenvalues
 (major and minor axes) and draws the ellipse using this transformation
 of coordinates */

void
compute_error_ellipse(double &angle, double &major_axis, double &minor_axis,
		double x_var, double xy_cov, double y_var, double k)
{
	double len, discriminant, eigval1, eigval2, eigvec1x, eigvec1y, eigvec2x, eigvec2y;

	/* check for special case of axis-aligned */
	if (fabs(xy_cov) < (fabs(x_var) + fabs(y_var) + 1e-4) * 1e-4)
	{
		eigval1 = x_var;
		eigval2 = y_var;
		eigvec1x = 1.;
		eigvec1y = 0.;
		eigvec2x = 0.;
		eigvec2y = 1.;
	}
	else
	{
		/* compute axes and scales of ellipse */
		discriminant = sqrt(4 * carmen_square(xy_cov) + carmen_square(x_var - y_var));
		eigval1 = .5 * (x_var + y_var - discriminant);
		eigval2 = .5 * (x_var + y_var + discriminant);
		eigvec1x = (x_var - y_var - discriminant) / (2.0 * xy_cov);
		eigvec1y = 1.0;
		eigvec2x = (x_var - y_var + discriminant) / (2.0 * xy_cov);
		eigvec2y = 1.0;
		/* normalize eigenvectors */
		len = sqrt(carmen_square(eigvec1x) + 1.0);
		eigvec1x /= len;
		eigvec1y /= len;
		len = sqrt(carmen_square(eigvec2x) + 1.0);
		eigvec2x /= len;
		eigvec2y /= len;
	}

	/* take square root of eigenvalues and scale -- once this is
	 done, eigvecs are unit vectors along axes and eigvals are
	 corresponding radii */
	if (eigval1 < 0 || eigval2 < 0)
	{
		eigval1 = 0.001;
		eigval2 = 0.001;
	}
	eigval1 = sqrt(eigval1) * k;
	eigval2 = sqrt(eigval2) * k;
	if (eigval1 < 0.001)
		eigval1 = 0.001;
	if (eigval2 < 0.001)
		eigval2 = 0.001;

	if (eigval1 >= eigval2)
	{
		angle = atan2(eigvec1y, eigvec1x);
		major_axis = eigval1;
		minor_axis = eigval2;
	}
	else
	{
		angle = atan2(eigvec2y, eigvec2x);
		major_axis = eigval2;
		minor_axis = eigval1;
	}
}

#ifdef CT_MODEL
void
set_constant_angular_velocity_system_matrixes(Matrix &F_k_1, Matrix &fx_k_1, Matrix &Q_k_1, Matrix &H_k, Matrix &R_p_k,
		double T, double w, double vx, double vy, double sigma_w, double sigma_v, double sigma_r, double sigma_theta)
{
	double CW, SW, C, S;
	double DSW, DVXW, DCW, DVYW;

	S = sin(w * T);
	C = cos(w * T);
	DVXW = -(T * (vy * C + vx * S));
	DVYW = T * (vx * C - vy * S);
	if (fabs(w) > 0.000001)
	{
		SW = S / w;
		CW = (1.0 - C) / w;
		DSW = ((C * T * vx) / w) - ((S * vx) / (w * w)) - ((S * T * vy) / w) - (((-1.0 + C) * vy) / (w * w));
		DCW = ((S * T * vx) / w) - (((1.0 - C) * vx) / (w * w)) + ((C * T * vy) / w) - ((S * vy) / (w * w));
	}
	else
	{
		SW = T;
		CW = 0.0;
		DSW = T * T * vx / 2.0; // checar se o limite de DSW quando w tende a zero eh isso mesmo
		DCW = T * T * vy / 2.0; // checar se o limite de DCW quando w tende a zero eh isso mesmo
	}

	double aux_data1[5 * 5] =
	{
		1.0,		0.0, 		SW, 		-CW,		DSW,
		0.0, 		1.0,		CW,			SW,			DCW,
		0.0, 		0.0, 		C,	 		-S,			DVXW,
		0.0, 		0.0, 		S, 			C,			DVYW,
		0.0, 		0.0, 		0.0, 		0.0,		1.0,
	};
	Matrix aux_matrix1(5, 5, aux_data1);
	fx_k_1 = aux_matrix1;  // [3] eq. 11.7.2-3 e 11.7.2-4, pg. 469 (Jacobiano) (com troca da posicao de x' com a de y)

	double aux_data2[5 * 5] =
	{
		1.0,		0.0, 		SW, 		-CW,		0.0,
		0.0, 		1.0,		CW,			SW,			0.0,
		0.0, 		0.0, 		C,	 		-S,			0.0,
		0.0, 		0.0, 		S, 			C,			0.0,
		0.0, 		0.0, 		0.0, 		0.0,		1.0,
	};
	Matrix aux_matrix2(5, 5, aux_data2);
	F_k_1 = aux_matrix2;  // [3] eq. 11.7.1-4, pgs. 467, 468 (com troca da posicao de x' com a de y)

	double sigma_vx = sigma_v;
	double sigma_vy = sigma_v;

	double aux_data3[3 * 3] =
	{
		sigma_vx*sigma_vx, 	0.0,				0.0,
		0.0, 				sigma_vy*sigma_vy,	0.0,
		0.0,				0.0,				sigma_w*sigma_w,
	};
	Matrix aux_matrix3(3, 3, aux_data3);

	double aux_data4[5 * 3] =
	{
		(T*T)/2.0,	0.0,		0.0,
		0.0, 		(T*T)/2.0,	0.0,
		T, 			0.0,		0.0,
		0.0,		T,			0.0,
		0.0,		0.0,		T,
	};
	Matrix aux_matrix4(5, 3, aux_data4);
	Q_k_1 = aux_matrix4 * aux_matrix3 * ~aux_matrix4;	// [3] eq. 11.7.1-4 e 11.7.2-6 pgs. 467, 469 (com troca da posicao de x' com a de y)

	double aux_data5[2 * 5] =
	{
		1.0,			0.0, 			0.0, 			0.0,			0.0,
		0.0, 			1.0,	 		0.0, 			0.0,			0.0,
	};
	Matrix aux_matrix5(2, 5, aux_data5);
	H_k = aux_matrix5;  // [1] eq. 2.38, pg 15 (com troca da posicao de x' com a de y)

	double aux_data6[2 * 2] =
	{
		sigma_r*sigma_r,		0.0,
		0.0, 					sigma_theta*sigma_theta,
	};
	Matrix aux_matrix6(2, 2, aux_data6);
	R_p_k = aux_matrix6;  // [1] eq. 2.34, pg 15
}


void
constant_angular_velocity_system_setup(double &x, double &y, double &phi, double &v, double &w, Matrix &x_k_k, Matrix &P_k_k,
		Matrix &F_k_1, Matrix &fx_k_1, Matrix &Q_k_1, Matrix &H_k, Matrix &R_p_k,
		double T, double &sigma_w, double &sigma_v, double &sigma_r, double &sigma_theta)
{
	x = -170.0;
	y = -150.0;
	phi = carmen_degrees_to_radians(45.0);
	v = 5.0;
	w = 0.0;

	sigma_w = 0.05;	// 3 graus por segundo
	sigma_v = 0.5;
	sigma_r = 2.0;
	sigma_theta = carmen_degrees_to_radians(1.0);

// Initial state
	double X[5 * 1] =
	{
		x - 5.0,		// x + adicao de erro inicial
		y + 10.0,		// y + adicao de erro inicial
		v * cos(phi),	// x' (v_x)
		v * sin(phi),	// y' (v_y)
		w,				// w -> vellocidade angular
	};
	Matrix aux_matrix1(5, 1, X);
	x_k_k = aux_matrix1; // [3] eq. 11.7.1-3, pg. 467 (com troca da posicao de x' com a de y)


	double P[5 * 5] =
	{
		10.0 * 10.0,	0.0, 			0.0, 			0.0,			0.0,
		0.0, 			10.0 * 10.0,	0.0, 			0.0,			0.0,
		0.0, 			0.0, 			0.2 * 0.2, 		0.0,			0.0,
		0.0, 			0.0, 			0.0, 			0.2 * 0.2,		0.0,
		0.0, 			0.0, 			0.0, 			0.0,			0.1 * 0.1,
	};
	Matrix aux_matrix2(5, 5, P);
	P_k_k = aux_matrix2;  // [1] eq. 3.33, pg 25 (com troca da posicao de x' com a de y)

	// Constant Velocity System setup
	set_constant_angular_velocity_system_matrixes(F_k_1, fx_k_1, Q_k_1, H_k, R_p_k, T, x_k_k.val[4][0], x_k_k.val[1][0], x_k_k.val[3][0], sigma_w, sigma_v, sigma_r, sigma_theta);
}
#endif

#ifdef CA_MODEL
void
set_constant_acceleration_system_matrixes(Matrix &F_k_1, Matrix &Q_k_1, Matrix &H_k, Matrix &R_p_k,
		double T, double sigma_vx, double sigma_vy, double sigma_r, double sigma_theta)
{
	double aux_data1[6 * 6] =
	{
		1.0,		0.0,		T, 			0.0, 		(T*T)/2.0, 	0.0,
		0.0, 		1.0,		0.0, 		T,			0.0, 		(T*T)/2.0,
		0.0, 		0.0,		1.0,		0.0,		T, 			0.0,
		0.0, 		0.0,		0.0, 		1.0,		0.0, 		T,
		0.0, 		0.0,		0.0, 		0.0,		1.0,		0.0,
		0.0, 		0.0,		0.0, 		0.0,		0.0, 		1.0,
	};
	Matrix aux_matrix1(6, 6, aux_data1);
	F_k_1 = aux_matrix1;  // [2] eq. 3.2.1.9, pg 32

	double aux_data22[2 * 2] =
	{
		sigma_vx*sigma_vx, 	0.0,
		0.0, 				sigma_vy*sigma_vy,
	};
	Matrix aux_matrix22(2, 2, aux_data22);	// [2] eq. 3.2.1.8, pg 31
	double aux_data23[6 * 2] =
	{
		(T*T)/2.0,	0.0,
		0.0,		(T*T)/2.0,
		T, 			0.0,
		0.0,		T,
		1.0,		0.0,
		0.0,		1.0,
	};
	Matrix aux_matrix23(6, 2, aux_data23);
	Q_k_1 = aux_matrix23 * aux_matrix22 * ~aux_matrix23;	// [2] eq. (sem numero) abaixo da eq. 3.2.1.10, pg. 32. qv vem da equacao eq. 3.2.1.8, pg 31.

	double aux_data3[2 * 6] =
	{
		1.0,			0.0, 			0.0,			0.0, 			0.0,			0.0,
		0.0, 			1.0,	 		0.0,			0.0, 			0.0,			0.0,
	};
	Matrix aux_matrix3(2, 6, aux_data3);
	H_k = aux_matrix3;  // [2] vem de eq. 5.1.3.1, pg. 47

	double aux_data4[2 * 2] =
	{
		sigma_r*sigma_r,		0.0,
		0.0, 					sigma_theta*sigma_theta,
	};
	Matrix aux_matrix4(2, 2, aux_data4);
	R_p_k = aux_matrix4;  // eq. 2.34, pg 15
}


void
constant_acceleration_system_setup(double &x, double &y, double &phi, double &v, Matrix &x_k_k, Matrix &P_k_k,
		Matrix &F_k_1, Matrix &Q_k_1, Matrix &H_k, Matrix &R_p_k,
		double T, double &sigma_vx, double &sigma_vy, double &sigma_r, double &sigma_theta)
{
	x = -170.0;
	y = -150.0;
	phi = carmen_degrees_to_radians(45.0);
	v = 5.0;

	sigma_vx = 0.05;
	sigma_vy = 0.05;
	sigma_r = 2.0;
	sigma_theta = carmen_degrees_to_radians(1.0);

	// Initial state
	double X[6 * 1] =
	{
		x - 5.0,		// x + adicao de erro inicial
		y + 10.0,		// y + adicao de erro inicial
		v * cos(phi),	// x' (v_x)
		v * sin(phi),	// y'
		0.0,			// x''
		0.0,			// y''
	};
	Matrix aux_matrix1(6, 1, X);
	x_k_k = aux_matrix1; // [2] eq. 3.2.1.6, pg 31

	double P[6 * 6] =
	{
		10.0 * 10.0,	0.0,			0.0, 			0.0,			0.0, 			0.0,
		0.0, 			10.0 * 10.0,	0.0, 			0.0,			0.0,			0.0,
		0.0, 			0.0,			0.2 * 0.2, 		0.0,			0.0, 			0.0,
		0.0, 			0.0,			0.0, 			0.2 * 0.2,		0.0, 			0.0,
		0.0, 			0.0,			0.0, 			0.0,			1.0 * 1.0,		0.0,
		0.0, 			0.0,			0.0, 			0.0,			0.0, 			1.0 * 1.0,
	};
	Matrix aux_matrix2(6, 6, P);
	P_k_k = aux_matrix2;  // vem de [1] eq. 3.33, pg 25, mas com mais termos para contemplar as aceleracoes, x'' e y''.

	// Constant Velocity System setup
	set_constant_acceleration_system_matrixes(F_k_1, Q_k_1, H_k, R_p_k, T, sigma_vx, sigma_vy, sigma_r, sigma_theta);
}
#endif

#ifdef CV_MODEL
void
set_constant_velocity_system_matrixes(Matrix &F_k_1, Matrix &Q_k_1, Matrix &H_k, Matrix &R_p_k,
		double T, double sigma_x, double sigma_y, double sigma_r, double sigma_theta)
{
	double aux_data1[4 * 4] =
	{
		1.0,		0.0, 		T, 			0.0,
		0.0, 		1.0,		0.0, 		T,
		0.0, 		0.0, 		1.0,	 	0.0,
		0.0, 		0.0, 		0.0, 		1.0,
	};
	Matrix aux_matrix1(4, 4, aux_data1);
	F_k_1 = aux_matrix1;  // [1] eq. 2.17, pg 11

	double aux_data22[2 * 2] =
	{
		sigma_x*sigma_x, 	0.0,
		0.0, 				sigma_y*sigma_y,
	};
	Matrix aux_matrix22(2, 2, aux_data22);	// [2] eq. 3.2.1.2, pg 30

	double aux_data23[4 * 2] =
	{
		(T*T)/2.0,	0.0,
		0.0, 		(T*T)/2.0,
		T, 			0.0,
		0.0,		T,
	};
	Matrix aux_matrix23(4, 2, aux_data23);
	Q_k_1 = aux_matrix23 * aux_matrix22 * ~aux_matrix23;	// [2] eq. 3.2.1.5, pg 31

	double aux_data3[2 * 4] =
	{
		1.0,			0.0, 			0.0, 			0.0,
		0.0, 			1.0,	 		0.0, 			0.0,
	};
	Matrix aux_matrix3(2, 4, aux_data3);
	H_k = aux_matrix3;  // [1] eq. 2.38, pg 15

	double aux_data4[2 * 2] =
	{
		sigma_r*sigma_r,		0.0,
		0.0, 					sigma_theta*sigma_theta,
	};
	Matrix aux_matrix4(2, 2, aux_data4);
	R_p_k = aux_matrix4;  // eq. 2.34, pg 15
}

void
constant_velocity_system_setup(double &x, double &y, double &phi, double &v, Matrix &x_k_k, Matrix &P_k_k,
		Matrix &F_k_1, Matrix &Q_k_1, Matrix &H_k, Matrix &R_p_k,
		double T, double &sigma_x, double &sigma_y, double &sigma_r, double &sigma_theta)
{
	x = -170.0;
	y = -150.0;
	phi = carmen_degrees_to_radians(45.0);
	v = 5.0;

	sigma_x = 0.001;
	sigma_y = 0.001;
	sigma_r = 2.0;
	sigma_theta = carmen_degrees_to_radians(1.0);

// Initial state
	double X[4 * 1] =
	{
		x - 5.0,		// x + adicao de erro inicial
		y + 10.0,		// y + adicao de erro inicial
		v * cos(phi),	// x' (v_x)
		v * sin(phi)	// y' (v_y)
	};
	Matrix aux_matrix1(4, 1, X);
	x_k_k = aux_matrix1; // [1] eq. 3.27, pg 24

	double P[4 * 4] =
	{
		10.0 * 10.0,	0.0, 			0.0, 			0.0,
		0.0, 			10.0 * 10.0,	0.0, 			0.0,
		0.0, 			0.0, 			0.2 * 0.2, 		0.0,
		0.0, 			0.0, 			0.0, 			0.2 * 0.2
	};
	Matrix aux_matrix2(4, 4, P);
	P_k_k = aux_matrix2;  // [1] eq. 3.33, pg 25

	// Constant Velocity System setup
	set_constant_velocity_system_matrixes(F_k_1, Q_k_1, H_k, R_p_k, T, sigma_x, sigma_y, sigma_r, sigma_theta);
}
#endif

void
true_position_observation(Matrix &z_k, Matrix &R_k, Matrix R_p_k, double x, double y, double sigma_r, double sigma_theta)
{
	double v_p_k_[2 * 1] =
	{
		carmen_gaussian_random(0.0, sigma_r),
		carmen_gaussian_random(0.0, sigma_theta),
	};
	Matrix v_p_k(2, 1, v_p_k_);  // [1] eqs. 2.33, 2.34, pg 14

	double radius = sqrt(x * x + y * y) + v_p_k.val[0][0]; 	// perfect observation + noise
	double theta = atan2(y, x) +  + v_p_k.val[1][0]; 	// perfect observation + noise

	double _z_k[2 * 1] =
	{
		radius * cos(theta),
		radius * sin(theta),
	};
	Matrix z_k_(2, 1, _z_k);	// [1] eq. 2.37, 2.38, pg 15
	z_k = z_k_;

	double j_z_p_k[2 * 2] =
	{
		cos(theta), -radius * sin(theta),
		sin(theta), radius * cos(theta),
	};
	Matrix J_z_p_k(2, 2, j_z_p_k);  // [1] eq. 2.40, pg 15

	if ((radius * sigma_theta * sigma_theta / sigma_r) >= 0.4)
	{
		printf("Noise sequence converted to the Cartesian coordinates (v_k) reveals state dependency - condition 1 %lf >= 0.4\n", radius * sigma_theta * sigma_theta / sigma_r);
		exit(1);
	}
	if (sigma_theta >= 0.4)
	{
		printf("Noise sequence converted to the Cartesian coordinates (v_k) reveals state dependency - condition 2 %lf >= 0.4\n", sigma_theta);
		exit(1);
	}
	R_k = J_z_p_k * R_p_k * ~J_z_p_k;	// [1] eq. 2.42, pg 15
}


void
kalman_filter(Matrix &x_k_k, Matrix &P_k_k, Matrix &delta_zk, Matrix &S_k,
		Matrix z_k, Matrix F_k_1, Matrix Q_k_1, Matrix H_k, Matrix R_k)
{	// [1] Table 3.1, pg. 23
	Matrix x_k_1_k_1 = x_k_k;
	Matrix P_k_1_k_1 = P_k_k;

	// Prediction
	Matrix x_k_k_1 = F_k_1 * x_k_1_k_1; // Target tracking nao tem entrada de controle
	Matrix P_k_k_1 = F_k_1 * P_k_1_k_1 * ~F_k_1 + Q_k_1;

	// Correction
	delta_zk = z_k - H_k * x_k_k_1;
	S_k = H_k * P_k_k_1 * ~H_k + R_k;
	Matrix K_k = P_k_k_1 * ~H_k * Matrix::inv(S_k);
	x_k_k = x_k_k_1 + K_k * delta_zk;
	Matrix aux = Matrix::eye(x_k_k.m) - K_k * H_k;
	P_k_k = aux * P_k_k_1 * ~aux + K_k * R_k * ~K_k;
}


void
extended_kalman_filter(Matrix &x_k_k, Matrix &P_k_k, Matrix &delta_zk, Matrix &S_k,
		Matrix z_k, Matrix F_k_1, Matrix fx_k_1, Matrix Q_k_1, Matrix H_k, Matrix R_k)
{	// https://en.wikipedia.org/wiki/Extended_Kalman_filter
	Matrix x_k_1_k_1 = x_k_k;
	Matrix P_k_1_k_1 = P_k_k;

	// Prediction
	Matrix x_k_k_1 = F_k_1 * x_k_1_k_1; // Traget tracking nao tem entrada de controle
	Matrix P_k_k_1 = fx_k_1 * P_k_1_k_1 * ~fx_k_1 + Q_k_1;

	// Correction
	delta_zk = z_k - H_k * x_k_k_1;
	S_k = H_k * P_k_k_1 * ~H_k + R_k;
	Matrix K_k = P_k_k_1 * ~H_k * Matrix::inv(S_k);
	x_k_k = x_k_k_1 + K_k * delta_zk;
	Matrix aux = Matrix::eye(x_k_k.m) - K_k * H_k;
	P_k_k = aux * P_k_k_1 * ~aux + K_k * R_k * ~K_k;
}


#ifdef IMM_FILTER

void
compute_mixing_probabilities(double c_bar[r], double u_k_1_k_1[r][r], double p[r][r], double *u_k_1)
{
	for (int j = 0; j < r; j++)
	{
		c_bar[j] = 0.0;
		for (int i = 0; i < r; i++)
			c_bar[j] += p[i][j] * u_k_1[i]; // [3] eq. 11.6.6-8, pg. 455
	}
	for (int i = 0; i < r; i++)
		for (int j = 0; j < r; j++)
			u_k_1_k_1[i][j] = p[i][j] * u_k_1[i] / c_bar[j]; // [3] eq. 11.6.6-7, pg. 455
}


void
mix_modes(vector<Matrix> &x_0_k_1_k_1, vector<Matrix> &P_0_k_1_k_1, double u_k_1_k_1[r][r], vector<Matrix> x_k_1_k_1, vector<Matrix> P_k_1_k_1)
{
	for (int j = 0; j < r; j++)
	{
		Matrix sum(x_k_1_k_1[0].m, 1);
		sum.zero();
		for (int i = 0; i < r; i++)
			sum = sum + x_k_1_k_1[i] * u_k_1_k_1[i][j];
		x_0_k_1_k_1.push_back(sum); // [3] eq. 11.6.6-9, pg. 455
	}

	for (int j = 0; j < r; j++)
	{
		Matrix sum(P_k_1_k_1[0].m, P_k_1_k_1[0].n);
		sum.zero();
		for (int i = 0; i < r; i++)
		{
			Matrix x_x_0 = x_k_1_k_1[i] - x_0_k_1_k_1[i];
			sum = sum + (P_k_1_k_1[i] + x_x_0 * ~x_x_0) * u_k_1_k_1[i][j];
		}
		P_0_k_1_k_1.push_back(sum); // [3] eq. 11.6.6-10, pg. 456
	}
}


void
mode_matched_filtering(double A[r], vector<Matrix> &x_k_k_1, vector<Matrix> &P_k_k_1, Matrix z_k, vector<Matrix> &F_k_1, Matrix &fx_k_1,
		vector<Matrix> &H_k, vector<Matrix> &Q_k_1, Matrix R_k, Matrix R_p_k, double T, double sigma_w, double sigma_v, double sigma_r,
		double sigma_theta)
{
	vector<Matrix> delta_zk, S_k;
	Matrix aux(z_k.m, z_k.n); delta_zk.push_back(aux); delta_zk.push_back(aux); delta_zk.push_back(aux);
	Matrix aux2(R_k.m, R_k.n); S_k.push_back(aux2); S_k.push_back(aux2); S_k.push_back(aux2);

	// CV_MODEL filter
	kalman_filter(x_k_k_1[0], P_k_k_1[0], delta_zk[0], S_k[0], z_k, F_k_1[0], Q_k_1[0], H_k[0], R_k);

	// CA_MODEL filter
	kalman_filter(x_k_k_1[1], P_k_k_1[1], delta_zk[1], S_k[1], z_k, F_k_1[1], Q_k_1[1], H_k[1], R_k);

	// CT_MODEL
	set_constant_angular_velocity_system_matrixes(F_k_1[2], fx_k_1, Q_k_1[2], H_k[2], R_p_k, T, x_k_k_1[2].val[4][0], x_k_k_1[2].val[2][0],
			x_k_k_1[2].val[3][0], sigma_w, sigma_v, sigma_r, sigma_theta);
	extended_kalman_filter(x_k_k_1[2], P_k_k_1[2], delta_zk[2], S_k[2], z_k, F_k_1[2], fx_k_1, Q_k_1[2], H_k[2], R_k);

	for (int j = 0; j < r; j++)
	{
		Matrix aux = S_k[j] * 2 * M_PI;
		Matrix exponent = ~delta_zk[j] * Matrix::inv(S_k[j]) * delta_zk[j];
		double exponent_double = exponent.val[0][0];
		double denominator = sqrt(aux.det());
		A[j] = exp(-0.5 * exponent_double) / denominator; // [3] eq. 11.6.6-12, ou [4] Section Model Probability Update
	}
}


void
mode_probability_update(double u_k[r], double A[r], double c_bar[r])
{
	double c = 0.0;
	for (int j = 0; j < r; j++)
		c += A[j] * c_bar[j]; // [3] eq. 11.6.6-16, pg. 456

	for (int j = 0; j < r; j++)
		u_k[j] = A[j] * c_bar[j] / c; // [3] eq. 11.6.6-15, pg. 456
}


void
mode_estimate_and_covariance_combination(Matrix &x_k_k, Matrix &P_k_k, vector<Matrix> x_k_k_1, vector<Matrix> P_k_k_1, double u_k[r])
{
	x_k_k.zero();
	for (int j = 0; j < r; j++)
		x_k_k = x_k_k + x_k_k_1[j] * u_k[j]; // [3] eq. 11.6.6-17, pg. 457

	P_k_k.zero();
	for (int j = 0; j < r; j++)
	{
		Matrix x_x = x_k_k_1[j] - x_k_k;
		P_k_k = (P_k_k_1[j] + x_x * ~x_x) * u_k[j];
	}
}


vector<Matrix>
extend_vector_dimensions(vector<Matrix> x)
{
	vector<Matrix> x_extended;

	Matrix cv(7, 1);
	cv.zero();
	cv.setMat(x[0], 0, 0);
	x_extended.push_back(cv);

	Matrix ca(7, 1);
	ca.zero();
	ca.setMat(x[1], 0, 0);
	x_extended.push_back(ca);

	Matrix ct(7, 1);
	ct.zero();
	ct.setMat(x[2], 0, 0);
	ct.val[6][0] = ct.val[4][0];
	ct.val[4][0] = 0.0;
	x_extended.push_back(cv);

	return (x_extended);
}


vector<Matrix>
restore_vector_dimensions(vector<Matrix> x)
{
	vector<Matrix> restored_x;

	Matrix cv = x[0].getMat(0, 0, 3, 0);
	restored_x.push_back(cv);

	Matrix ca = x[1].getMat(0, 0, 5, 0);
	restored_x.push_back(ca);

	Matrix ct = x[2].getMat(0, 0, 4, 0);
	ct.val[0][4] = x[2].val[0][6];
	restored_x.push_back(ct);

	return (restored_x);
}


vector<Matrix>
restore_matrix_dimensions(vector<Matrix> x)
{
	vector<Matrix> restored_x;

	Matrix cv = x[0].getMat(0, 0, 3, 3);
	restored_x.push_back(cv);

	Matrix ca = x[1].getMat(0, 0, 5, 5);
	restored_x.push_back(ca);

	Matrix ct = x[2].getMat(0, 0, 4, 4);
	for (int i = 0; i < 4; i++)
	{
		ct.val[i][4] = x[2].val[i][6];

		ct.val[4][i] = x[2].val[6][i];
	}
	ct.val[4][4] = x[2].val[6][6];
	restored_x.push_back(ct);

	return (restored_x);
}


vector<Matrix>
extend_matrix_dimensions(vector<Matrix> x, double max_a, double max_w)
{
	vector<Matrix> x_extended;

	double sigma_a = pow(2.0 * max_a, 2.0) / 12; // [5] Section C
	double sigma_w = pow(2.0 * max_w, 2.0) / 12; // [5] Section C

	Matrix cv(7, 7);
	cv.zero();
	cv.setMat(x[0], 0, 0);
	cv.val[4][4] = cv.val[5][5] = sigma_a;
	cv.val[6][6] = sigma_w;
	x_extended.push_back(cv);

	Matrix ca(7, 7);
	ca.zero();
	ca.setMat(x[1], 0, 0);
	cv.val[6][6] = sigma_w;
	x_extended.push_back(ca);

	Matrix ct(7, 7);
	ct.zero();
	ct.setMat(x[2], 0, 0);
	for (int i = 0; i < 4; i++)
	{
		ct.val[i][6] = ct.val[i][4];
		ct.val[i][4] = 0.0;

		ct.val[6][i] = ct.val[4][i];
		ct.val[4][i] = 0.0;
	}
	cv.val[6][6] = cv.val[4][4];
	ct.val[4][4] = ct.val[5][5] = sigma_a;
	x_extended.push_back(cv);

	return (x_extended);
}


void
imm_filter(Matrix &x_k_k, Matrix &P_k_k, vector<Matrix> &x_k_1_k_1, vector<Matrix> &P_k_1_k_1,
		Matrix z_k, Matrix R_k, Matrix R_p_k, double sigma_r, double sigma_theta,
		vector<Matrix> F_k_1, vector<Matrix> Q_k_1, vector<Matrix> H_k,
		Matrix fx_k_1, double T, double sigma_w, double sigma_v, double max_a, double max_w)
{
	// [3] pg. 455, Section The Algorithm (IMM)
	double *u_k_1 = u_k;
	double c_bar[r];
	double u_k_1_k_1[r][r];
	compute_mixing_probabilities(c_bar, u_k_1_k_1, p, u_k_1);

	vector<Matrix> x_0_k_1_k_1;
	vector<Matrix> P_0_k_1_k_1;
	mix_modes(x_0_k_1_k_1, P_0_k_1_k_1, u_k_1_k_1, extend_vector_dimensions(x_k_1_k_1), extend_matrix_dimensions(P_k_1_k_1, max_a, max_w));

	double A[r];
	vector<Matrix> x_k_k_1 = restore_vector_dimensions(x_0_k_1_k_1);
	vector<Matrix> P_k_k_1 = restore_matrix_dimensions(P_0_k_1_k_1);
	mode_matched_filtering(A, x_k_k_1, P_k_k_1, z_k, F_k_1, fx_k_1, H_k, Q_k_1, R_k, R_p_k, T, sigma_w, sigma_v, sigma_r, sigma_theta);
	x_k_1_k_1 = x_k_k_1; // Aqui eu retorno os estados para a proxima iteracao [5]
	P_k_1_k_1 = P_k_k_1;

	mode_probability_update(u_k, A, c_bar);
	mode_estimate_and_covariance_combination(x_k_k, P_k_k, extend_vector_dimensions(x_k_k_1), extend_matrix_dimensions(P_k_k_1, max_a, max_w), u_k);
}


int
main()
{
	Matrix x_k_k, P_k_k, z_k, R_k, R_p_k, fx_k_1, delta_zk, S_k;
	vector<Matrix> x_k_1_k_1, P_k_1_k_1, F_k_1_m, Q_k_1_m, H_k_m;
	Matrix F_k_1, Q_k_1, H_k;

	double angle, major_axis, minor_axis;

	double max_a = 3.0, max_w = carmen_degrees_to_radians(20.0);
	double delta_t = 1.0;

	double x, y, phi, v, sigma_x, sigma_y, sigma_r, sigma_theta;
	constant_velocity_system_setup(x, y, phi, v, x_k_k, P_k_k, F_k_1, Q_k_1, H_k, R_p_k, delta_t, sigma_x, sigma_y, sigma_r, sigma_theta);
	x_k_1_k_1.push_back(x_k_k); P_k_1_k_1.push_back(P_k_k); F_k_1_m.push_back(F_k_1); Q_k_1_m.push_back(Q_k_1); H_k_m.push_back(H_k);

	double sigma_vx, sigma_vy;
	constant_acceleration_system_setup(x, y, phi, v, x_k_k, P_k_k, F_k_1, Q_k_1, H_k, R_p_k, delta_t, sigma_vx, sigma_vy, sigma_r, sigma_theta);
	x_k_1_k_1.push_back(x_k_k); P_k_1_k_1.push_back(P_k_k); F_k_1_m.push_back(F_k_1); Q_k_1_m.push_back(Q_k_1); H_k_m.push_back(H_k);

	double w, sigma_w, sigma_v;
	constant_angular_velocity_system_setup(x, y, phi, v, w, x_k_k, P_k_k, F_k_1, fx_k_1, Q_k_1, H_k, R_p_k, delta_t, sigma_w, sigma_v, sigma_r, sigma_theta);
	x_k_1_k_1.push_back(x_k_k); P_k_1_k_1.push_back(P_k_k); F_k_1_m.push_back(F_k_1); Q_k_1_m.push_back(Q_k_1); H_k_m.push_back(H_k);

	Matrix imm_x_k_k(7, 1), imm_P_k_k(7, 7);
	mode_estimate_and_covariance_combination(imm_x_k_k, imm_P_k_k, extend_vector_dimensions(x_k_1_k_1), extend_matrix_dimensions(P_k_1_k_1, max_a, max_w), u_k);
	compute_error_ellipse(angle, major_axis, minor_axis,
			imm_P_k_k.val[0][0], imm_P_k_k.val[0][1], imm_P_k_k.val[1][1], 2.4477);
	printf("%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf\n", x, y, imm_x_k_k.val[0][0], imm_x_k_k.val[1][0], major_axis, minor_axis, angle * 180.0 / M_PI,
			u_k[0], u_k[1], u_k[2]);

	double t = 0.0;
	do
	{
		true_position_observation(z_k, R_k, R_p_k, x, y, sigma_r, sigma_theta);

		imm_filter(imm_x_k_k, imm_P_k_k, x_k_1_k_1, P_k_1_k_1,
				z_k, R_k, R_p_k, sigma_r, sigma_theta,
				F_k_1_m, Q_k_1_m, H_k_m,
				fx_k_1, delta_t, sigma_w, sigma_v, max_a, max_w);

		compute_error_ellipse(angle, major_axis, minor_axis,
				imm_P_k_k.val[0][0], imm_P_k_k.val[0][1], imm_P_k_k.val[1][1], 2.4477);
		printf("%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf\n", x, y, imm_x_k_k.val[0][0], imm_x_k_k.val[1][0], major_axis, minor_axis, angle * 180.0 / M_PI,
				u_k[0], u_k[1], u_k[2]);

		x = x + v * cos(phi) * delta_t;	// true position update
		y = y + v * sin(phi) * delta_t;	// true position update

		if ((t > 40.0) && (t < 60.0))
			phi += 0.3 * delta_t;

		t += delta_t;
	} while (t < 100.0);
}

#else

int
main()
{
	double angle, major_axis, minor_axis;
	Matrix x_k_k, P_k_k, F_k_1, Q_k_1, H_k, R_p_k;

	double delta_t = 1.0;
#ifdef CV_MODEL
	double x, y, phi, v, sigma_x, sigma_y, sigma_r, sigma_theta;
	constant_velocity_system_setup(x, y, phi, v, x_k_k, P_k_k, F_k_1, Q_k_1, H_k, R_p_k, delta_t, sigma_x, sigma_y, sigma_r, sigma_theta);
#endif

#ifdef CA_MODEL
	double x, y, phi, v, sigma_vx, sigma_vy, sigma_r, sigma_theta;
	constant_acceleration_system_setup(x, y, phi, v, x_k_k, P_k_k, F_k_1, Q_k_1, H_k, R_p_k, delta_t, sigma_vx, sigma_vy, sigma_r, sigma_theta);
#endif

#ifdef CT_MODEL
	double x, y, phi, v, w, sigma_w, sigma_v, sigma_r, sigma_theta;
	Matrix fx_k_1;
	constant_angular_velocity_system_setup(x, y, phi, v, w, x_k_k, P_k_k, F_k_1, fx_k_1, Q_k_1, H_k, R_p_k, delta_t, sigma_w, sigma_v, sigma_r, sigma_theta);
#endif

	compute_error_ellipse(angle, major_axis, minor_axis,
			P_k_k.val[0][0], P_k_k.val[0][1], P_k_k.val[1][1], 2.4477);
	printf("%lf %lf %lf %lf %lf %lf %lf\n", x, y, x_k_k.val[0][0], x_k_k.val[1][0], major_axis, minor_axis, angle * 180.0 / M_PI);

	Matrix z_k, R_k, delta_zk, S_k;
	double t = 0.0;
	do
	{
		true_position_observation(z_k, R_k, R_p_k, x, y, sigma_r, sigma_theta);

#if defined(CV_MODEL) || defined(CA_MODEL)
		kalman_filter(x_k_k, P_k_k, delta_zk, S_k, z_k, F_k_1, Q_k_1, H_k, R_k);
#endif

#ifdef CT_MODEL
		set_constant_angular_velocity_system_matrixes(F_k_1, fx_k_1, Q_k_1, H_k, R_p_k, delta_t, x_k_k.val[4][0], x_k_k.val[2][0], x_k_k.val[3][0], sigma_w, sigma_v, sigma_r, sigma_theta);
		extended_kalman_filter(x_k_k, P_k_k, delta_zk, S_k, z_k, F_k_1, fx_k_1, Q_k_1, H_k, R_k);
#endif

		compute_error_ellipse(angle, major_axis, minor_axis,
				P_k_k.val[0][0], P_k_k.val[0][1], P_k_k.val[1][1], 2.4477);
		printf("%lf %lf %lf %lf %lf %lf %lf\n", x, y, x_k_k.val[0][0], x_k_k.val[1][0], major_axis, minor_axis, angle * 180.0 / M_PI);

		x = x + v * cos(phi) * delta_t;	// true position update
		y = y + v * sin(phi) * delta_t;	// true position update

		if ((t > 55.0) && (t < 75.0))
			phi += 0.3 * delta_t;

		t += delta_t;
	} while (t < 100.0);
}
#endif
