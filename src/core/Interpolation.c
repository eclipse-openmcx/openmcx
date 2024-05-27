/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/Interpolation.h"
#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include <stdlib.h>
#include <limits.h>

mcx_table* mcx_interp_new_table( void ) {
    mcx_table * table = (mcx_table *) mcx_malloc(sizeof(mcx_table));
    if (!table) {
        return NULL;
    }

    table->interp = MCX_TABLE_INTERP_NOT_SET;
    table->extrap = MCX_TABLE_EXTRAP_NOT_SET;

    table->x_data = NULL;
    table->y_data = NULL;

    table->n = 0;

    return table;
}

mcx_table * mcx_interp_create_arbitrary_table(size_t numSamplingPts, size_t numOrdinates, mcx_table_interp_type interp, mcx_table_extrap_type extrap) {
    mcx_table * table = mcx_interp_new_table();
    if (!table) {
        mcx_log(LOG_ERROR, "Cannot create interpolation table");
        return NULL;
    }

    table->interp = interp;
    table->extrap = extrap;
    return table;
}

int mcx_interp_setup_table(mcx_table * s_table, mcx_table_interp_type interp, mcx_table_extrap_type extrap) {
    if (!s_table) {
        return EXIT_FAILURE;
    }

    s_table->interp = interp;
    s_table->extrap = extrap;

    return EXIT_SUCCESS;
}

void mcx_interp_change_table_data(mcx_table *s_table, double *x_data, double *y_data, int nPoints) {
    if (!s_table) {
        return;
    }

    s_table->x_data = x_data;
    s_table->y_data = y_data;

    s_table->n = nPoints;
}

void mcx_interp_change_multiple_ordinate_table_data(mcx_table *s_table, double *x_data, double **y_data, int nPoints) {
    mcx_log(LOG_ERROR, "mcx_interp_changeData_table_multiple_ordinate: Unimplemented");
    exit(-1);
}

static double _mcx_interp_interp_step_right(int n, double * xs, double * ys, double x) {
    if (n == 0) {
        return NAN;
    } else if (n == 1) {
        return ys[0];
    } else if (n == 2) {
        if (x <= xs[0]) {
            return ys[0];
        } else {
            return ys[1];
        }
   } else {
        // find new interval
        int m = n / 2;

        if (xs[m] == x) {
            return ys[m];
        } else if (xs[m] > x) {
            // left
            return _mcx_interp_interp_step_right(m+1, xs, ys, x);
        } else {
            // right
            return _mcx_interp_interp_step_right(m+1, xs + (n-m-1), ys + (n-m-1), x);
        }
    }
}

static double _mcx_interp_interp_linear(int n, double * xs, double * ys, double x) {
    if (n == 0) {
        return NAN;
    } else if (n == 1) {
        // no interp possible, use ys[0]
        return ys[0];
    } else if (n == 2) {
        // interp between ys[0] and ys[1]
        return ys[0] + (x - xs[0]) * (ys[1] - ys[0])/(xs[1] - xs[0]);
    } else {
        // find new interval
        int m = n / 2;

        if (xs[m] == x) {
            return ys[m];
        } else if (xs[m] > x) {
            // left
            return _mcx_interp_interp_linear(m+1, xs, ys, x);
        } else {
            // right
            return _mcx_interp_interp_linear(m+1, xs + (n - m - 1), ys + (n - m -1), x);
        }
    }
}

double mcx_interp_get_value_from_table( mcx_table* s_table, double x ) {
    double y = NAN;

    if (!s_table) {
        return NAN;
    }

    if (s_table->n <= 0) {
        return NAN;
    }

    // _mcx_interp_interp_linear below needs at least 2 points
    if (s_table->n == 1) {
        return s_table->y_data[0];
    }

    if (x < s_table->x_data[0]) {
        // extrap
        if (s_table->extrap == MCX_TABLE_EXTRAP_CONST ||
            (s_table->extrap == MCX_TABLE_EXTRAP_LINEAR && s_table->n < 2))
        {
            y = s_table->y_data[0];
        } else {
            y = _mcx_interp_interp_linear(2, s_table->x_data, s_table->y_data, x);
        }
    } else if (x > s_table->x_data[s_table->n - 1]) {
        // extrap
        if (s_table->extrap == MCX_TABLE_EXTRAP_CONST ||
            (s_table->extrap == MCX_TABLE_EXTRAP_LINEAR && s_table->n < 2))
        {
            y = s_table->y_data[s_table->n - 1];
        } else {
            y = _mcx_interp_interp_linear(2, s_table->x_data + s_table->n - 2, s_table->y_data + s_table->n - 2, x);
        }
    } else {
        // interp
        if (s_table->interp == MCX_TABLE_INTERP_LINEAR) {
            y = _mcx_interp_interp_linear(s_table->n, s_table->x_data, s_table->y_data, x);
        } else if (s_table->interp == MCX_TABLE_INTERP_STEP_RIGHT) {
            y = _mcx_interp_interp_step_right(s_table->n, s_table->x_data, s_table->y_data, x);
        }
    }

    return y;
}

void mcx_interp_get_value_from_table_multiple_ordinate( mcx_table* s_table, double x, double *y ) {
    mcx_log(LOG_ERROR, "mcx_interp_get_value_from_table_multiple_ordinate: Unimplemented");
    exit(-1);
}

double mcx_interp_get_value_from_table_multiple_ordinate_one_value( mcx_table* s_table, double x, int pos ) {
    mcx_log(LOG_ERROR, "mcx_interp_get_value_from_table_multiple_ordinate_one_value: Unimplemented");
    exit(-1);
}

void mcx_interp_free_table( mcx_table* s_table ) {
    if (s_table) {
        mcx_free(s_table);
    }
}

int mcx_interp_num_y_columns( mcx_table * s_table ) {
    return 1;
}

int mcx_poly_get_n( mcx_table_poly * s_poly ) {
    return s_poly->n;
}

double mcx_poly_get_x( mcx_table_poly * s_poly, int i) {
    if (i < s_poly->n) {
        return s_poly->x_data[i];
    } else {
        return NAN;
    }
}

double mcx_poly_get_y( mcx_table_poly * s_poly, int i) {
    if (i < s_poly->n) {
        return s_poly->y_data[i];
    } else {
        return NAN;
    }
}

void mcx_poly_evaluate_poly(mcx_table_poly* s_poly, double x, double *pd, int nd) {
    if (nd == 0) {
        pd[0] = _mcx_interp_interp_linear(s_poly->n, s_poly->x_data, s_poly->y_data, x);
    } else {
        int i = 0;
        for (i = 0; i < nd; i++) {
            pd[i] = NAN;
        }
    }
}

int mcx_poly_remove_last_points(mcx_table_poly *s_poly, int nRemovePoints) {
    s_poly->n = fmax(0, s_poly->n - nRemovePoints);
    if (s_poly->n == 0) {
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}

int mcx_poly_add_points(mcx_table_poly *s_poly, double *addX_data, double *addY_data, int addPoints) {
    if (s_poly->n + addPoints <= s_poly->alloc) {
        int i = 0;
        for (i = 0; i < addPoints; i++) {
            s_poly->x_data[s_poly->n + i] = addX_data[i];
            s_poly->y_data[s_poly->n + i] = addY_data[i];
        }
        s_poly->n += addPoints;

        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
}

void mcx_poly_shift_points(mcx_table_poly *s_poly, double *x_data, double *y_data, int nPoints) {
    int i = 0;
    for (i = 0; i < s_poly->n - nPoints; i++) {
        s_poly->x_data[i] = s_poly->x_data[i + nPoints];
        s_poly->y_data[i] = s_poly->y_data[i + nPoints];
    }

    for (i = 0; i < nPoints; i++) {
        s_poly->x_data[s_poly->n - nPoints + i] = x_data[i];
        s_poly->y_data[s_poly->n - nPoints + i] = y_data[i];
    }
}

int mcx_poly_calc_coef_N2(mcx_table_poly* s_poly) {
    // TODO: Calculate actual polynomial coefficients
    return EXIT_SUCCESS;
}

void mcx_poly_free_poly( mcx_table_poly* s_poly ) {
    if (s_poly->x_data) { mcx_free(s_poly->x_data); }
    if (s_poly->y_data) { mcx_free(s_poly->y_data); }

    s_poly->x_data = NULL;
    s_poly->y_data = NULL;

    s_poly->n     = 0;
    s_poly->alloc = 0;
}

mcx_table_poly* mcx_poly_create_poly(int maxPoints) {
    mcx_table_poly * s_poly = mcx_calloc(sizeof(mcx_table_poly), 1);
    if (!s_poly) { goto error_cleanup; }

    s_poly->n = 0;
    s_poly->alloc = maxPoints;

    s_poly->x_data = mcx_calloc(sizeof(double), maxPoints);
    if (!s_poly->x_data) { goto error_cleanup; }

    s_poly->y_data = mcx_calloc(sizeof(double), maxPoints);
    if (!s_poly->y_data) { goto error_cleanup; }

    return s_poly;

error_cleanup:
    if (!s_poly) { return NULL; }

    if (s_poly->x_data) { mcx_free(s_poly->x_data); }
    if (s_poly->y_data) { mcx_free(s_poly->y_data); }

    return NULL;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */