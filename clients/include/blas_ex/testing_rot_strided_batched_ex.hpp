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

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "testing_common.hpp"

/* ============================================================================================ */

using hipblasRotStridedBatchedExModel = ArgumentModel<e_a_type,
                                                      e_b_type,
                                                      e_c_type,
                                                      e_compute_type,
                                                      e_N,
                                                      e_incx,
                                                      e_incy,
                                                      e_stride_scale,
                                                      e_batch_count>;

inline void testname_rot_strided_batched_ex(const Arguments& arg, std::string& name)
{
    hipblasRotStridedBatchedExModel{}.test_name(arg, name);
}

template <typename Tx, typename Ty = Tx, typename Tcs = Ty, typename Tex = Tcs>
void testing_rot_strided_batched_ex(const Arguments& arg)
{
    bool FORTRAN = arg.api == hipblas_client_api::FORTRAN;
    auto hipblasRotStridedBatchedExFn
        = FORTRAN ? hipblasRotStridedBatchedExFortran : hipblasRotStridedBatchedEx;

    int    N            = arg.N;
    int    incx         = arg.incx;
    int    incy         = arg.incy;
    double stride_scale = arg.stride_scale;
    int    batch_count  = arg.batch_count;

    int           abs_incx = incx >= 0 ? incx : -incx;
    int           abs_incy = incy >= 0 ? incy : -incy;
    hipblasStride stridex  = N * abs_incx * stride_scale;
    hipblasStride stridey  = N * abs_incy * stride_scale;

    size_t size_x = stridex * batch_count;
    size_t size_y = stridey * batch_count;
    if(!size_x)
        size_x = 1;
    if(!size_y)
        size_y = 1;

    hipblasDatatype_t xType         = arg.a_type;
    hipblasDatatype_t yType         = arg.b_type;
    hipblasDatatype_t csType        = arg.c_type;
    hipblasDatatype_t executionType = arg.compute_type;

    hipblasLocalHandle handle(arg);

    // check to prevent undefined memory allocation error
    if(N <= 0 || batch_count <= 0)
    {
        ASSERT_HIPBLAS_SUCCESS(hipblasRotStridedBatchedExFn(handle,
                                                            N,
                                                            nullptr,
                                                            xType,
                                                            incx,
                                                            stridex,
                                                            nullptr,
                                                            yType,
                                                            incy,
                                                            stridey,
                                                            nullptr,
                                                            nullptr,
                                                            csType,
                                                            batch_count,
                                                            executionType));

        return;
    }

    double gpu_time_used, hipblas_error_host, hipblas_error_device;

    device_vector<Tx>  dx(size_x);
    device_vector<Ty>  dy(size_y);
    device_vector<Tcs> dc(1);
    device_vector<Tcs> ds(1);

    // Initial Data on CPU
    host_vector<Tx>  hx_host(size_x);
    host_vector<Ty>  hy_host(size_y);
    host_vector<Tx>  hx_device(size_x);
    host_vector<Ty>  hy_device(size_y);
    host_vector<Tx>  hx_cpu(size_x);
    host_vector<Ty>  hy_cpu(size_y);
    host_vector<Tcs> hc(1);
    host_vector<Tcs> hs(1);

    hipblas_init_vector(
        hx_host, arg, N, abs_incx, stridex, batch_count, hipblas_client_never_set_nan, true);
    hipblas_init_vector(
        hy_host, arg, N, abs_incy, stridey, batch_count, hipblas_client_never_set_nan, false);
    hipblas_init_vector(hc, arg, 1, 1, 0, 1, hipblas_client_never_set_nan, false);
    hipblas_init_vector(hs, arg, 1, 1, 0, 1, hipblas_client_never_set_nan, false);

    hx_cpu = hx_device = hx_host;
    hy_cpu = hy_device = hy_host;

    ASSERT_HIP_SUCCESS(hipMemcpy(dx, hx_host, sizeof(Tx) * size_x, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dy, hy_host, sizeof(Ty) * size_y, hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(dc, hc, sizeof(Tcs), hipMemcpyHostToDevice));
    ASSERT_HIP_SUCCESS(hipMemcpy(ds, hs, sizeof(Tcs), hipMemcpyHostToDevice));

    if(arg.unit_check || arg.norm_check)
    {
        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
        ASSERT_HIPBLAS_SUCCESS(hipblasRotStridedBatchedExFn(handle,
                                                            N,
                                                            dx,
                                                            xType,
                                                            incx,
                                                            stridex,
                                                            dy,
                                                            yType,
                                                            incy,
                                                            stridey,
                                                            hc,
                                                            hs,
                                                            csType,
                                                            batch_count,
                                                            executionType));

        ASSERT_HIP_SUCCESS(hipMemcpy(hx_host, dx, sizeof(Tx) * size_x, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(hy_host, dy, sizeof(Ty) * size_y, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(dx, hx_device, sizeof(Tx) * size_x, hipMemcpyHostToDevice));
        ASSERT_HIP_SUCCESS(hipMemcpy(dy, hy_device, sizeof(Ty) * size_y, hipMemcpyHostToDevice));

        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        ASSERT_HIPBLAS_SUCCESS(hipblasRotStridedBatchedExFn(handle,
                                                            N,
                                                            dx,
                                                            xType,
                                                            incx,
                                                            stridex,
                                                            dy,
                                                            yType,
                                                            incy,
                                                            stridey,
                                                            dc,
                                                            ds,
                                                            csType,
                                                            batch_count,
                                                            executionType));

        ASSERT_HIP_SUCCESS(hipMemcpy(hx_device, dx, sizeof(Tx) * size_x, hipMemcpyDeviceToHost));
        ASSERT_HIP_SUCCESS(hipMemcpy(hy_device, dy, sizeof(Ty) * size_y, hipMemcpyDeviceToHost));

        for(int b = 0; b < batch_count; b++)
        {
            cblas_rot<Tx, Tcs, Tcs>(
                N, hx_cpu.data() + b * stridex, incx, hy_cpu.data() + b * stridey, incy, *hc, *hs);
        }

        if(arg.unit_check)
        {
            unit_check_general<Tx>(1, N, batch_count, abs_incx, stridex, hx_cpu, hx_host);
            unit_check_general<Tx>(1, N, batch_count, abs_incy, stridey, hy_cpu, hy_host);
            unit_check_general<Ty>(1, N, batch_count, abs_incx, stridex, hx_cpu, hx_device);
            unit_check_general<Ty>(1, N, batch_count, abs_incy, stridey, hy_cpu, hy_device);
        }

        if(arg.norm_check)
        {
            hipblas_error_host = norm_check_general<Tx>(
                'F', 1, N, abs_incx, stridex, hx_cpu, hx_host, batch_count);
            hipblas_error_host += norm_check_general<Ty>(
                'F', 1, N, abs_incy, stridey, hy_cpu, hy_host, batch_count);
            hipblas_error_device = norm_check_general<Tx>(
                'F', 1, N, abs_incx, stridex, hx_cpu, hx_device, batch_count);
            hipblas_error_device += norm_check_general<Ty>(
                'F', 1, N, abs_incy, stridey, hy_cpu, hy_device, batch_count);
        }
    }

    if(arg.timing)
    {
        hipStream_t stream;
        ASSERT_HIPBLAS_SUCCESS(hipblasGetStream(handle, &stream));
        ASSERT_HIPBLAS_SUCCESS(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));

        int runs = arg.cold_iters + arg.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == arg.cold_iters)
                gpu_time_used = get_time_us_sync(stream);

            ASSERT_HIPBLAS_SUCCESS(hipblasRotStridedBatchedExFn(handle,
                                                                N,
                                                                dx,
                                                                xType,
                                                                incx,
                                                                stridex,
                                                                dy,
                                                                yType,
                                                                incy,
                                                                stridey,
                                                                dc,
                                                                ds,
                                                                csType,
                                                                batch_count,
                                                                executionType));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasRotStridedBatchedExModel{}.log_args<Tx>(std::cout,
                                                       arg,
                                                       gpu_time_used,
                                                       rot_gflop_count<Tx, Ty, Tcs, Tcs>(N),
                                                       rot_gbyte_count<Tx>(N),
                                                       hipblas_error_host,
                                                       hipblas_error_device);
    }
}

template <typename Tx, typename Ty = Tx, typename Tcs = Ty, typename Tex = Tcs>
hipblasStatus_t testing_rot_strided_batched_ex_ret(const Arguments& arg)
{
    testing_rot_strided_batched_ex<Tx, Ty, Tcs, Tex>(arg);
    return HIPBLAS_STATUS_SUCCESS;
}
