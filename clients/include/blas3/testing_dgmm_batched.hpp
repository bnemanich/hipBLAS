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

using hipblasDgmmBatchedModel
    = ArgumentModel<e_a_type, e_side, e_M, e_N, e_lda, e_incx, e_ldc, e_batch_count>;

inline void testname_dgmm_batched(const Arguments& arg, std::string& name)
{
    hipblasDgmmBatchedModel{}.test_name(arg, name);
}

template <typename T>
void testing_dgmm_batched(const Arguments& arg)
{
    bool FORTRAN = arg.fortran;
    auto hipblasDgmmBatchedFn
        = FORTRAN ? hipblasDgmmBatched<T, true> : hipblasDgmmBatched<T, false>;

    hipblasSideMode_t side = char2hipblas_side(arg.side);

    int M           = arg.M;
    int N           = arg.N;
    int lda         = arg.lda;
    int incx        = arg.incx;
    int ldc         = arg.ldc;
    int batch_count = arg.batch_count;

    size_t A_size = size_t(lda) * N;
    size_t C_size = size_t(ldc) * N;
    int    k      = (side == HIPBLAS_SIDE_RIGHT ? N : M);

    hipblasLocalHandle handle(arg);

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    bool invalid_size = M < 0 || N < 0 || ldc < M || lda < M || batch_count < 0;
    if(invalid_size || !N || !M || !batch_count)
    {
        hipblasStatus_t actual = hipblasDgmmBatchedFn(
            handle, side, M, N, nullptr, lda, nullptr, incx, nullptr, ldc, batch_count);
        EXPECT_HIPBLAS_STATUS2(
            actual, (invalid_size ? HIPBLAS_STATUS_INVALID_VALUE : HIPBLAS_STATUS_SUCCESS));
        return;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_batch_vector<T> hA(A_size, 1, batch_count);
    host_batch_vector<T> hA_copy(A_size, 1, batch_count);
    host_batch_vector<T> hx(k, incx, batch_count);
    host_batch_vector<T> hx_copy(k, incx, batch_count);
    host_batch_vector<T> hC(C_size, 1, batch_count);
    host_batch_vector<T> hC_1(C_size, 1, batch_count);
    host_batch_vector<T> hC_gold(C_size, 1, batch_count);

    device_batch_vector<T> dA(A_size, 1, batch_count);
    device_batch_vector<T> dx(k, incx, batch_count);
    device_batch_vector<T> dC(C_size, 1, batch_count);

    ASSERT_HIP_SUCCESS(dA.memcheck());
    ASSERT_HIP_SUCCESS(dx.memcheck());
    ASSERT_HIP_SUCCESS(dC.memcheck());

    double gpu_time_used, hipblas_error;

    // Initial Data on CPU
    hipblas_init_vector(hA, arg, hipblas_client_never_set_nan, true);
    hipblas_init_vector(hx, arg, hipblas_client_never_set_nan, false, true);
    hipblas_init_vector(hC, arg, hipblas_client_never_set_nan);

    hA_copy.copy_from(hA);
    hx_copy.copy_from(hx);
    hC_1.copy_from(hC);
    hC_gold.copy_from(hC_gold);

    ASSERT_HIP_SUCCESS(dA.transfer_from(hA));
    ASSERT_HIP_SUCCESS(dx.transfer_from(hx));
    ASSERT_HIP_SUCCESS(dC.transfer_from(hC));

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
            HIPBLAS
        =================================================================== */
        ASSERT_HIPBLAS_SUCCESS(hipblasDgmmBatchedFn(handle,
                                                    side,
                                                    M,
                                                    N,
                                                    dA.ptr_on_device(),
                                                    lda,
                                                    dx.ptr_on_device(),
                                                    incx,
                                                    dC.ptr_on_device(),
                                                    ldc,
                                                    batch_count));
        ASSERT_HIP_SUCCESS(hC_1.transfer_from(dC));

        /* =====================================================================
           CPU BLAS
        =================================================================== */

        // reference calculation
        ptrdiff_t shift_x = incx < 0 ? -ptrdiff_t(incx) * (N - 1) : 0;
        for(int b = 0; b < batch_count; b++)
        {
            for(size_t i1 = 0; i1 < M; i1++)
            {
                for(size_t i2 = 0; i2 < N; i2++)
                {
                    if(HIPBLAS_SIDE_RIGHT == side)
                    {
                        hC_gold[b][i1 + i2 * ldc]
                            = hA_copy[b][i1 + i2 * lda] * hx_copy[b][shift_x + i2 * incx];
                    }
                    else
                    {
                        hC_gold[b][i1 + i2 * ldc]
                            = hA_copy[b][i1 + i2 * lda] * hx_copy[b][shift_x + i1 * incx];
                    }
                }
            }
        }

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(arg.unit_check)
        {
            unit_check_general<T>(M, N, batch_count, ldc, hC_gold, hC_1);
        }

        if(arg.norm_check)
        {
            hipblas_error = norm_check_general<T>('F', M, N, ldc, hC_gold, hC_1, batch_count);
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

            ASSERT_HIPBLAS_SUCCESS(hipblasDgmmBatchedFn(handle,
                                                        side,
                                                        M,
                                                        N,
                                                        dA.ptr_on_device(),
                                                        lda,
                                                        dx.ptr_on_device(),
                                                        incx,
                                                        dC.ptr_on_device(),
                                                        ldc,
                                                        batch_count));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used; // in microseconds

        hipblasDgmmBatchedModel{}.log_args<T>(std::cout,
                                              arg,
                                              gpu_time_used,
                                              dgmm_gflop_count<T>(M, N),
                                              dgmm_gbyte_count<T>(M, N, k),
                                              hipblas_error);
    }
}

template <typename T>
hipblasStatus_t testing_dgmm_batched_ret(const Arguments& arg)
{
    testing_dgmm_batched<T>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}