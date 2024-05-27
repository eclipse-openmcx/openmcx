/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_INTERPOLATION_H
#define MCX_CORE_INTERPOLATION_H

#include "CentralParts.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    MCX_TABLE_INTERP_NOT_SET = -1,
    MCX_TABLE_INTERP_STEP_RIGHT = 0,
    MCX_TABLE_INTERP_LINEAR = 1
} mcx_table_interp_type;

typedef enum {
    MCX_TABLE_EXTRAP_NOT_SET = -1,
    MCX_TABLE_EXTRAP_CONST = 0,
    MCX_TABLE_EXTRAP_LINEAR = 1,
    MCX_TABLE_EXTRAP_PRD_LEFT = 2
} mcx_table_extrap_type;

typedef struct {
    mcx_table_interp_type interp;
    mcx_table_extrap_type extrap;

    double * x_data;
    double * y_data; // TODO: if this becomes multi-dimensional,
                     // update mcx_interp_get_columnsY

    int n;
} mcx_table;
mcx_table* mcx_interp_new_table( void );

mcx_table * mcx_interp_create_arbitrary_table(size_t numSamplingPts, size_t numOrdinates, mcx_table_interp_type interp, mcx_table_extrap_type extrap);

int    mcx_interp_setup_table(mcx_table * s_table, mcx_table_interp_type interp, mcx_table_extrap_type extrap);
void   mcx_interp_free_table( mcx_table* s_table );
void   mcx_interp_change_table_data(mcx_table *s_table, double *x_data, double *y_data, int nPoints);
void   mcx_interp_change_multiple_ordinate_table_data(mcx_table *s_table, double *x_data, double **y_data, int nPoints);
double mcx_interp_get_value_from_table( mcx_table* s_table, double x );
void   mcx_interp_get_value_from_table_multiple_ordinate( mcx_table* s_table, double x, double *y );
double mcx_interp_get_value_from_table_multiple_ordinate_one_value( mcx_table* s_table, double x, int pos );
int    mcx_interp_num_y_columns( mcx_table * s_table );

typedef struct {
    double * x_data;
    double * y_data; // TODO: if this becomes multi-dimensional,
                     // update mcx_interp_get_columnsY

    int n;
    int alloc;
} mcx_table_poly;
mcx_table_poly* mcx_poly_create_poly(int maxPoints);

int    mcx_poly_get_n( mcx_table_poly * s_poly );
double mcx_poly_get_x( mcx_table_poly * s_poly, int i );
double mcx_poly_get_y( mcx_table_poly * s_poly, int i );
void   mcx_poly_evaluate_poly(mcx_table_poly* s_poly, double x, double *pd, int nd);
int    mcx_poly_remove_last_points(mcx_table_poly *s_poly, int nRemovePoints);
int    mcx_poly_add_points(mcx_table_poly *s_poly, double *addX_data, double *addY_data, int addPoints);
void   mcx_poly_shift_points(mcx_table_poly *s_poly, double *x_data, double *y_data, int nPoints);
int    mcx_poly_calc_coef_N2(mcx_table_poly* s_poly);
void   mcx_poly_free_poly( mcx_table_poly* s_poly );

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_INTERPOLATION_H */