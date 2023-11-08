/* ************************************************************************
 * Copyright (C) 2016-2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "testing_common.hpp"

/* ============================================================================================ */

using hipblasSetGetMatrixAsyncModel = ArgumentModel<e_a_type, e_M, e_N, e_lda, e_ldb, e_ldc>;

inline void testname_set_get_matrix_async(const Arguments& arg, std::string& name)
{
    hipblasSetGetMatrixAsyncModel{}.test_name(arg, name);
}

template <typename T>
void testing_set_get_matrix_async(const Arguments& arg)
{
    bool FORTRAN                 = arg.fortran;
    auto hipblasSetMatrixAsyncFn = FORTRAN ? hipblasSetMatrixAsyncFortran : hipblasSetMatrixAsync;
    auto hipblasGetMatrixAsyncFn = FORTRAN ? hipblasGetMatrixAsyncFortran : hipblasGetMatrixAsync;

    int rows = arg.rows;
    int cols = arg.cols;
    int lda  = arg.lda;
    int ldb  = arg.ldb;
    int ldc  = arg.ldc;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(rows < 0 || cols < 0 || lda <= 0 || ldb <= 0 || ldc <= 0)
    {
        return;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> ha(cols * lda);
    host_vector<T> hb(cols * ldb);
    host_vector<T> hb_ref(cols * ldb);
    host_vector<T> hc(cols * ldc);

    device_vector<T> dc(cols * ldc);

    double             hipblas_error = 0.0, gpu_time_used = 0.0;
    hipblasLocalHandle handle(arg);

    hipStream_t stream;
    hipblasGetStream(handle, &stream);

    // Initial Data on CPU
    srand(1);
    hipblas_init<T>(ha, rows, cols, lda);
    hipblas_init<T>(hb, rows, cols, ldb);
    hb_ref = hb;
    for(int i = 0; i < cols * ldc; i++)
    {
        hc[i] = 100 + i;
    };
    ASSERT_HIP_SUCCESS(hipMemcpy(dc, hc.data(), sizeof(T) * ldc * cols, hipMemcpyHostToDevice));
    for(int i = 0; i < cols * ldc; i++)
    {
        hc[i] = 99.0;
    };

    /* =====================================================================
           HIPBLAS
    =================================================================== */
    ASSERT_HIPBLAS_SUCCESS(
        hipblasSetMatrixAsyncFn(rows, cols, sizeof(T), (void*)ha, lda, (void*)dc, ldc, stream));
    ASSERT_HIPBLAS_SUCCESS(
        hipblasGetMatrixAsyncFn(rows, cols, sizeof(T), (void*)dc, ldc, (void*)hb, ldb, stream));

    ASSERT_HIP_SUCCESS(hipStreamSynchronize(stream));

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
           CPU BLAS
        =================================================================== */

        // reference calculation
        for(int i1 = 0; i1 < rows; i1++)
        {
            for(int i2 = 0; i2 < cols; i2++)
            {
                hb_ref[i1 + i2 * ldb] = ha[i1 + i2 * lda];
            }
        }

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(arg.unit_check)
        {
            unit_check_general<T>(rows, cols, ldb, hb, hb_ref);
        }
        if(arg.norm_check)
        {
            hipblas_error = norm_check_general<T>('F', rows, cols, ldb, hb, hb_ref);
        }
    }

    if(arg.timing)
    {
        hipStream_t stream;
        ASSERT_HIPBLAS_SUCCESS(hipblasGetStream(handle, &stream));

        int runs = arg.cold_iters + arg.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == arg.cold_iters)
                gpu_time_used = get_time_us_sync(stream);

            ASSERT_HIPBLAS_SUCCESS(hipblasSetMatrixAsyncFn(
                rows, cols, sizeof(T), (void*)ha, lda, (void*)dc, ldc, stream));
            ASSERT_HIPBLAS_SUCCESS(hipblasGetMatrixAsyncFn(
                rows, cols, sizeof(T), (void*)dc, ldc, (void*)hb, ldb, stream));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasSetGetMatrixAsyncModel{}.log_args<T>(std::cout,
                                                    arg,
                                                    gpu_time_used,
                                                    ArgumentLogging::NA_value,
                                                    set_get_matrix_gbyte_count<T>(rows, cols),
                                                    hipblas_error);
    }
}

template <typename T>
hipblasStatus_t testing_set_get_matrix_async_ret(const Arguments& arg)
{
    testing_set_get_matrix_async<T>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}