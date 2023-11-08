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

using hipblasTpmvModel = ArgumentModel<e_a_type, e_uplo, e_transA, e_diag, e_N, e_incx>;

inline void testname_tpmv(const Arguments& arg, std::string& name)
{
    hipblasTpmvModel{}.test_name(arg, name);
}

template <typename T>
void testing_tpmv(const Arguments& arg)
{
    bool FORTRAN       = arg.api == hipblas_client_api::FORTRAN;
    auto hipblasTpmvFn = FORTRAN ? hipblasTpmv<T, true> : hipblasTpmv<T, false>;

    hipblasFillMode_t  uplo   = char2hipblas_fill(arg.uplo);
    hipblasOperation_t transA = char2hipblas_operation(arg.transA);
    hipblasDiagType_t  diag   = char2hipblas_diagonal(arg.diag);
    int                N      = arg.N;
    int                incx   = arg.incx;

    int    abs_incx = incx >= 0 ? incx : -incx;
    size_t x_size   = size_t(N) * abs_incx;
    size_t A_size   = size_t(N) * (N + 1) / 2;

    hipblasLocalHandle handle(arg);

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    bool invalid_size = N < 0 || !incx;
    if(invalid_size || !N)
    {
        hipblasStatus_t actual
            = hipblasTpmvFn(handle, uplo, transA, diag, N, nullptr, nullptr, incx);
        EXPECT_HIPBLAS_STATUS2(
            actual, (invalid_size ? HIPBLAS_STATUS_INVALID_VALUE : HIPBLAS_STATUS_SUCCESS));
        return;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> hA(A_size);
    host_vector<T> hx(x_size);
    host_vector<T> hres(x_size);

    device_vector<T> dA(A_size);
    device_vector<T> dx(x_size);

    double gpu_time_used, hipblas_error;

    // Initial Data on CPU
    hipblas_init_matrix(hA, arg, A_size, 1, 1, 0, 1, hipblas_client_never_set_nan, true, false);
    hipblas_init_vector(hx, arg, N, abs_incx, 0, 1, hipblas_client_never_set_nan, false, true);

    // copy vector is easy in STL; hz = hy: save a copy in hz which will be output of CPU BLAS
    hres = hx;

    // copy data from CPU to device
    ASSERT_HIP_SUCCESS(hipMemcpy(dA, hA.data(), sizeof(T) * A_size, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dx, hx.data(), sizeof(T) * x_size, hipMemcpyHostToDevice));

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
            HIPBLAS
        =================================================================== */
        ASSERT_HIPBLAS_SUCCESS(hipblasTpmvFn(handle, uplo, transA, diag, N, dA, dx, incx));

        // copy output from device to CPU
        ASSERT_HIP_SUCCESS(hipMemcpy(hres.data(), dx, sizeof(T) * x_size, hipMemcpyDeviceToHost));

        /* =====================================================================
           CPU BLAS
        =================================================================== */
        cblas_tpmv<T>(uplo, transA, diag, N, hA.data(), hx.data(), incx);

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(arg.unit_check)
        {
            unit_check_general<T>(1, N, abs_incx, hx, hres);
        }
        if(arg.norm_check)
        {
            hipblas_error = norm_check_general<T>('F', 1, N, abs_incx, hx.data(), hres.data());
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

            ASSERT_HIPBLAS_SUCCESS(hipblasTpmvFn(handle, uplo, transA, diag, N, dA, dx, incx));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used; // in microseconds

        hipblasTpmvModel{}.log_args<T>(std::cout,
                                       arg,
                                       gpu_time_used,
                                       tpmv_gflop_count<T>(N),
                                       tpmv_gbyte_count<T>(N),
                                       hipblas_error);
    }
}

template <typename T>
hipblasStatus_t testing_tpmv_ret(const Arguments& arg)
{
    testing_tpmv<T>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}