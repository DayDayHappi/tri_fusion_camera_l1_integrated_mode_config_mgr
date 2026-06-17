#include "media/app_fusion/OpenClFusionBackend.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef TRI_FUSION_APP_HAS_OPENCL
#define TRI_FUSION_APP_HAS_OPENCL 0
#endif

#if TRI_FUSION_APP_HAS_OPENCL
#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 120
#endif
#if __has_include(<CL/cl.h>)
#include <CL/cl.h>
#define TRI_FUSION_APP_HAS_OPENCL_HEADER 1
#else
#define TRI_FUSION_APP_HAS_OPENCL_HEADER 0
#endif
#else
#define TRI_FUSION_APP_HAS_OPENCL_HEADER 0
#endif

namespace tri::media::app_fusion {
namespace {

std::size_t rgbSizeBytes(int width, int height) {
    if (width <= 0 || height <= 0) return 0;
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3;
}

std::size_t nv12SizeBytes(int width, int height) {
    if (width <= 0 || height <= 0) return 0;
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3 / 2;
}

void setError(std::string* errorOut, const std::string& text) {
    if (errorOut != nullptr) *errorOut = text;
}

#if TRI_FUSION_APP_HAS_OPENCL && TRI_FUSION_APP_HAS_OPENCL_HEADER

std::string clErrorName(cl_int err) {
    switch (err) {
        case CL_SUCCESS: return "CL_SUCCESS";
        case CL_DEVICE_NOT_FOUND: return "CL_DEVICE_NOT_FOUND";
        case CL_DEVICE_NOT_AVAILABLE: return "CL_DEVICE_NOT_AVAILABLE";
        case CL_COMPILER_NOT_AVAILABLE: return "CL_COMPILER_NOT_AVAILABLE";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case CL_OUT_OF_RESOURCES: return "CL_OUT_OF_RESOURCES";
        case CL_OUT_OF_HOST_MEMORY: return "CL_OUT_OF_HOST_MEMORY";
        case CL_PROFILING_INFO_NOT_AVAILABLE: return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case CL_MEM_COPY_OVERLAP: return "CL_MEM_COPY_OVERLAP";
        case CL_IMAGE_FORMAT_MISMATCH: return "CL_IMAGE_FORMAT_MISMATCH";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case CL_BUILD_PROGRAM_FAILURE: return "CL_BUILD_PROGRAM_FAILURE";
        case CL_MAP_FAILURE: return "CL_MAP_FAILURE";
        case CL_INVALID_VALUE: return "CL_INVALID_VALUE";
        case CL_INVALID_DEVICE_TYPE: return "CL_INVALID_DEVICE_TYPE";
        case CL_INVALID_PLATFORM: return "CL_INVALID_PLATFORM";
        case CL_INVALID_DEVICE: return "CL_INVALID_DEVICE";
        case CL_INVALID_CONTEXT: return "CL_INVALID_CONTEXT";
        case CL_INVALID_QUEUE_PROPERTIES: return "CL_INVALID_QUEUE_PROPERTIES";
        case CL_INVALID_COMMAND_QUEUE: return "CL_INVALID_COMMAND_QUEUE";
        case CL_INVALID_HOST_PTR: return "CL_INVALID_HOST_PTR";
        case CL_INVALID_MEM_OBJECT: return "CL_INVALID_MEM_OBJECT";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case CL_INVALID_IMAGE_SIZE: return "CL_INVALID_IMAGE_SIZE";
        case CL_INVALID_SAMPLER: return "CL_INVALID_SAMPLER";
        case CL_INVALID_BINARY: return "CL_INVALID_BINARY";
        case CL_INVALID_BUILD_OPTIONS: return "CL_INVALID_BUILD_OPTIONS";
        case CL_INVALID_PROGRAM: return "CL_INVALID_PROGRAM";
        case CL_INVALID_PROGRAM_EXECUTABLE: return "CL_INVALID_PROGRAM_EXECUTABLE";
        case CL_INVALID_KERNEL_NAME: return "CL_INVALID_KERNEL_NAME";
        case CL_INVALID_KERNEL_DEFINITION: return "CL_INVALID_KERNEL_DEFINITION";
        case CL_INVALID_KERNEL: return "CL_INVALID_KERNEL";
        case CL_INVALID_ARG_INDEX: return "CL_INVALID_ARG_INDEX";
        case CL_INVALID_ARG_VALUE: return "CL_INVALID_ARG_VALUE";
        case CL_INVALID_ARG_SIZE: return "CL_INVALID_ARG_SIZE";
        case CL_INVALID_KERNEL_ARGS: return "CL_INVALID_KERNEL_ARGS";
        case CL_INVALID_WORK_DIMENSION: return "CL_INVALID_WORK_DIMENSION";
        case CL_INVALID_WORK_GROUP_SIZE: return "CL_INVALID_WORK_GROUP_SIZE";
        case CL_INVALID_WORK_ITEM_SIZE: return "CL_INVALID_WORK_ITEM_SIZE";
        case CL_INVALID_GLOBAL_OFFSET: return "CL_INVALID_GLOBAL_OFFSET";
        case CL_INVALID_EVENT_WAIT_LIST: return "CL_INVALID_EVENT_WAIT_LIST";
        case CL_INVALID_EVENT: return "CL_INVALID_EVENT";
        case CL_INVALID_OPERATION: return "CL_INVALID_OPERATION";
        case CL_INVALID_GL_OBJECT: return "CL_INVALID_GL_OBJECT";
        case CL_INVALID_BUFFER_SIZE: return "CL_INVALID_BUFFER_SIZE";
        case CL_INVALID_MIP_LEVEL: return "CL_INVALID_MIP_LEVEL";
        case CL_INVALID_GLOBAL_WORK_SIZE: return "CL_INVALID_GLOBAL_WORK_SIZE";
    }
    std::ostringstream ss;
    ss << "CL_ERROR(" << err << ")";
    return ss.str();
}

std::string getPlatformString(cl_platform_id platform, cl_platform_info info) {
    std::size_t size = 0;
    if (clGetPlatformInfo(platform, info, 0, nullptr, &size) != CL_SUCCESS || size == 0) return "unknown";
    std::vector<char> text(size, '\0');
    if (clGetPlatformInfo(platform, info, size, text.data(), nullptr) != CL_SUCCESS) return "unknown";
    if (!text.empty() && text.back() == '\0') text.pop_back();
    return std::string(text.begin(), text.end());
}

std::string getDeviceString(cl_device_id device, cl_device_info info) {
    std::size_t size = 0;
    if (clGetDeviceInfo(device, info, 0, nullptr, &size) != CL_SUCCESS || size == 0) return "unknown";
    std::vector<char> text(size, '\0');
    if (clGetDeviceInfo(device, info, size, text.data(), nullptr) != CL_SUCCESS) return "unknown";
    if (!text.empty() && text.back() == '\0') text.pop_back();
    return std::string(text.begin(), text.end());
}

template <typename T>
T getDeviceValue(cl_device_id device, cl_device_info info, T fallback = T{}) {
    T value{};
    if (clGetDeviceInfo(device, info, sizeof(T), &value, nullptr) != CL_SUCCESS) return fallback;
    return value;
}

std::string buildLog(cl_program program, cl_device_id device) {
    std::size_t logSize = 0;
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
    if (logSize == 0) return {};
    std::vector<char> log(logSize + 1, '\0');
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
    return std::string(log.data());
}

const char* fusionKernelSource() {
    return R"CLC(
inline int clamp_int_gpu(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline uchar clamp_u8_gpu(int v) {
    return (uchar)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

inline void fused_rgb_at_gpu(__global const uchar* visible_rgb,
                             __global const uchar* composite_rgb,
                             int visible_w,
                             int visible_h,
                             int out_w,
                             int out_h,
                             float visible_weight,
                             int visible_offset_x,
                             int visible_offset_y,
                             int x,
                             int y,
                             int* out_r,
                             int* out_g,
                             int* out_b) {
    int vx = (int)(((long)x * (long)visible_w) / (long)out_w) + visible_offset_x;
    int vy = (int)(((long)y * (long)visible_h) / (long)out_h) + visible_offset_y;
    vx = clamp_int_gpu(vx, 0, visible_w - 1);
    vy = clamp_int_gpu(vy, 0, visible_h - 1);

    const int vi = (vy * visible_w + vx) * 3;
    const int ci = (y * out_w + x) * 3;

    const float cw = 1.0f - visible_weight;
    int r = (int)((float)visible_rgb[vi + 0] * visible_weight + (float)composite_rgb[ci + 0] * cw);
    int g = (int)((float)visible_rgb[vi + 1] * visible_weight + (float)composite_rgb[ci + 1] * cw);
    int b = (int)((float)visible_rgb[vi + 2] * visible_weight + (float)composite_rgb[ci + 2] * cw);

    *out_r = clamp_int_gpu(r, 0, 255);
    *out_g = clamp_int_gpu(g, 0, 255);
    *out_b = clamp_int_gpu(b, 0, 255);
}

__kernel void fuse_rgb_to_nv12_2x2(__global const uchar* visible_rgb,
                                   __global const uchar* composite_rgb,
                                   __global uchar* out_nv12,
                                   int visible_w,
                                   int visible_h,
                                   int out_w,
                                   int out_h,
                                   float visible_weight,
                                   int visible_offset_x,
                                   int visible_offset_y,
                                   int enable_bilinear_resize) {
    (void)enable_bilinear_resize;

    const int bx = (int)get_global_id(0);
    const int by = (int)get_global_id(1);
    const int x0 = bx * 2;
    const int y0 = by * 2;
    if (x0 >= out_w || y0 >= out_h) return;

    int u_sum = 0;
    int v_sum = 0;
    int count = 0;

    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const int x = x0 + dx;
            const int y = y0 + dy;
            if (x >= out_w || y >= out_h) continue;

            int r = 0;
            int g = 0;
            int b = 0;
            fused_rgb_at_gpu(visible_rgb, composite_rgb,
                             visible_w, visible_h,
                             out_w, out_h,
                             visible_weight,
                             visible_offset_x, visible_offset_y,
                             x, y,
                             &r, &g, &b);

            const int yy = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            const int uu = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
            const int vv = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

            out_nv12[y * out_w + x] = clamp_u8_gpu(yy);
            u_sum += uu;
            v_sum += vv;
            ++count;
        }
    }

    if (count > 0) {
        __global uchar* uv_plane = out_nv12 + out_w * out_h;
        const int uv_index = (y0 / 2) * out_w + x0;
        uv_plane[uv_index + 0] = clamp_u8_gpu(u_sum / count);
        uv_plane[uv_index + 1] = clamp_u8_gpu(v_sum / count);
    }
}
)CLC";
}

#endif // TRI_FUSION_APP_HAS_OPENCL && TRI_FUSION_APP_HAS_OPENCL_HEADER

} // namespace

class OpenClFusionBackend::Impl {
public:
    Impl() = default;
    ~Impl() { release(); }

    bool init(int maxVisibleWidth,
              int maxVisibleHeight,
              int outputWidth,
              int outputHeight,
              std::string* errorOut) {
        release();
        ready_ = false;

        if (maxVisibleWidth <= 0 || maxVisibleHeight <= 0 || outputWidth <= 0 || outputHeight <= 0) {
            setError(errorOut, "invalid OpenCL fusion dimensions");
            return false;
        }
        if ((outputWidth % 2) != 0 || (outputHeight % 2) != 0) {
            setError(errorOut, "OpenCL fusion rejects odd output size because NV12 requires even width/height");
            return false;
        }

        maxVisibleWidth_ = maxVisibleWidth;
        maxVisibleHeight_ = maxVisibleHeight;
        outputWidth_ = outputWidth;
        outputHeight_ = outputHeight;
        visibleCapacityBytes_ = rgbSizeBytes(maxVisibleWidth_, maxVisibleHeight_);
        compositeCapacityBytes_ = rgbSizeBytes(outputWidth_, outputHeight_);
        outputCapacityBytes_ = nv12SizeBytes(outputWidth_, outputHeight_);

#if TRI_FUSION_APP_HAS_OPENCL && TRI_FUSION_APP_HAS_OPENCL_HEADER
        cl_int err = CL_SUCCESS;

        cl_uint platformCount = 0;
        err = clGetPlatformIDs(0, nullptr, &platformCount);
        if (err != CL_SUCCESS) {
            setError(errorOut, "clGetPlatformIDs failed: " + clErrorName(err));
            return false;
        }
        if (platformCount == 0) {
            setError(errorOut, "no OpenCL platform found");
            return false;
        }

        std::vector<cl_platform_id> platforms(platformCount);
        err = clGetPlatformIDs(platformCount, platforms.data(), nullptr);
        if (err != CL_SUCCESS) {
            setError(errorOut, "clGetPlatformIDs(list) failed: " + clErrorName(err));
            return false;
        }

        for (cl_platform_id platform : platforms) {
            cl_uint deviceCount = 0;
            const cl_int deviceErr = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &deviceCount);
            if (deviceErr != CL_SUCCESS || deviceCount == 0) continue;
            std::vector<cl_device_id> devices(deviceCount);
            if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, deviceCount, devices.data(), nullptr) != CL_SUCCESS) continue;
            platform_ = platform;
            device_ = devices.front();
            break;
        }

        if (platform_ == nullptr || device_ == nullptr) {
            setError(errorOut, "no GPU device found");
            return false;
        }

        platformName_ = getPlatformString(platform_, CL_PLATFORM_NAME);
        platformVendor_ = getPlatformString(platform_, CL_PLATFORM_VENDOR);
        deviceName_ = getDeviceString(device_, CL_DEVICE_NAME);
        deviceVendor_ = getDeviceString(device_, CL_DEVICE_VENDOR);
        openclVersion_ = getDeviceString(device_, CL_DEVICE_VERSION);
        computeUnits_ = getDeviceValue<cl_uint>(device_, CL_DEVICE_MAX_COMPUTE_UNITS, 0);
        maxWorkGroupSize_ = getDeviceValue<std::size_t>(device_, CL_DEVICE_MAX_WORK_GROUP_SIZE, 0);
        globalMemBytes_ = getDeviceValue<cl_ulong>(device_, CL_DEVICE_GLOBAL_MEM_SIZE, 0);

        context_ = clCreateContext(nullptr, 1, &device_, nullptr, nullptr, &err);
        if (err != CL_SUCCESS || context_ == nullptr) {
            setError(errorOut, "clCreateContext failed: " + clErrorName(err));
            release();
            return false;
        }

        queue_ = clCreateCommandQueue(context_, device_, 0, &err);
        if (err != CL_SUCCESS || queue_ == nullptr) {
            setError(errorOut, "clCreateCommandQueue failed: " + clErrorName(err));
            release();
            return false;
        }

        const char* source = fusionKernelSource();
        const std::size_t sourceLen = std::strlen(source);
        program_ = clCreateProgramWithSource(context_, 1, &source, &sourceLen, &err);
        if (err != CL_SUCCESS || program_ == nullptr) {
            setError(errorOut, "clCreateProgramWithSource failed: " + clErrorName(err));
            release();
            return false;
        }

        err = clBuildProgram(program_, 1, &device_, "", nullptr, nullptr);
        if (err != CL_SUCCESS) {
            const std::string log = buildLog(program_, device_);
            setError(errorOut, "clBuildProgram failed: " + clErrorName(err) + "; build log = " + log);
            release();
            return false;
        }

        kernel_ = clCreateKernel(program_, "fuse_rgb_to_nv12_2x2", &err);
        if (err != CL_SUCCESS || kernel_ == nullptr) {
            setError(errorOut, "clCreateKernel failed: " + clErrorName(err));
            release();
            return false;
        }

        visibleBuffer_ = clCreateBuffer(context_, CL_MEM_READ_ONLY, visibleCapacityBytes_, nullptr, &err);
        if (err != CL_SUCCESS || visibleBuffer_ == nullptr) {
            setError(errorOut, "clCreateBuffer visible failed: " + clErrorName(err));
            release();
            return false;
        }
        compositeBuffer_ = clCreateBuffer(context_, CL_MEM_READ_ONLY, compositeCapacityBytes_, nullptr, &err);
        if (err != CL_SUCCESS || compositeBuffer_ == nullptr) {
            setError(errorOut, "clCreateBuffer composite failed: " + clErrorName(err));
            release();
            return false;
        }
        outputBuffer_ = clCreateBuffer(context_, CL_MEM_WRITE_ONLY, outputCapacityBytes_, nullptr, &err);
        if (err != CL_SUCCESS || outputBuffer_ == nullptr) {
            setError(errorOut, "clCreateBuffer output failed: " + clErrorName(err));
            release();
            return false;
        }

        {
            std::ostringstream ss;
            ss << "platform='" << platformName_ << "' vendor='" << platformVendor_
               << "' device='" << deviceName_ << "' device_vendor='" << deviceVendor_
               << "' version='" << openclVersion_ << "' compute_units=" << computeUnits_
               << " max_work_group_size=" << maxWorkGroupSize_
               << " global_mem_bytes=" << static_cast<unsigned long long>(globalMemBytes_)
               << " output=" << outputWidth_ << "x" << outputHeight_;
            description_ = ss.str();
        }

        std::cerr << "[GPU_FUSION][DEVICE] platform name=" << platformName_
                  << " vendor=" << platformVendor_ << "\n";
        std::cerr << "[GPU_FUSION][DEVICE] device name=" << deviceName_
                  << " vendor=" << deviceVendor_
                  << " version=" << openclVersion_ << "\n";
        std::cerr << "[GPU_FUSION][DEVICE] compute_units=" << computeUnits_
                  << " max_work_group_size=" << maxWorkGroupSize_
                  << " global_mem_bytes=" << static_cast<unsigned long long>(globalMemBytes_) << "\n";

        ready_ = true;
        return true;
#else
        (void)visibleCapacityBytes_;
        (void)compositeCapacityBytes_;
        (void)outputCapacityBytes_;
#if TRI_FUSION_APP_HAS_OPENCL
        setError(errorOut, "TRI_FUSION_APP_HAS_OPENCL=1 but CL/cl.h is not available at compile time");
#else
        setError(errorOut, "TRI_FUSION_APP_HAS_OPENCL=0; OpenCL library/header not found at configure time");
#endif
        return false;
#endif
    }

    bool isReady() const { return ready_; }

    bool fuseToNv12(const RgbFrame& visible,
                    const RgbFrame& composite,
                    const GpuFusionParams& params,
                    std::vector<std::uint8_t>* outNv12,
                    std::string* errorOut) {
        if (outNv12 == nullptr) {
            setError(errorOut, "outNv12 is null");
            return false;
        }
        outNv12->clear();
        if (!ready_) {
            setError(errorOut, "OpenCL fusion backend is not ready");
            return false;
        }
        if (params.outputWidth != outputWidth_ || params.outputHeight != outputHeight_) {
            std::ostringstream ss;
            ss << "OpenCL output size changed, init=" << outputWidth_ << "x" << outputHeight_
               << " request=" << params.outputWidth << "x" << params.outputHeight;
            setError(errorOut, ss.str());
            return false;
        }
        if ((params.outputWidth % 2) != 0 || (params.outputHeight % 2) != 0) {
            setError(errorOut, "OpenCL fusion rejects odd output size because NV12 requires even width/height");
            return false;
        }
        if (visible.width <= 0 || visible.height <= 0 || composite.width != outputWidth_ || composite.height != outputHeight_) {
            setError(errorOut, "invalid visible/composite frame dimensions for OpenCL fusion");
            return false;
        }

        const std::size_t visibleBytes = rgbSizeBytes(visible.width, visible.height);
        const std::size_t compositeBytes = rgbSizeBytes(composite.width, composite.height);
        const std::size_t outputBytes = nv12SizeBytes(outputWidth_, outputHeight_);
        if (visible.rgb.size() < visibleBytes || composite.rgb.size() < compositeBytes) {
            setError(errorOut, "input RGB frame buffer is smaller than expected");
            return false;
        }
        if (visibleBytes > visibleCapacityBytes_) {
            std::ostringstream ss;
            ss << "visible RGB frame exceeds OpenCL buffer capacity: frame_bytes=" << visibleBytes
               << " capacity=" << visibleCapacityBytes_;
            setError(errorOut, ss.str());
            return false;
        }
        if (compositeBytes > compositeCapacityBytes_ || outputBytes > outputCapacityBytes_) {
            setError(errorOut, "composite/output frame exceeds OpenCL buffer capacity");
            return false;
        }

#if TRI_FUSION_APP_HAS_OPENCL && TRI_FUSION_APP_HAS_OPENCL_HEADER
        cl_int err = CL_SUCCESS;
        err = clEnqueueWriteBuffer(queue_, visibleBuffer_, CL_TRUE, 0, visibleBytes, visible.rgb.data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            setError(errorOut, "clEnqueueWriteBuffer visible failed: " + clErrorName(err));
            return false;
        }
        err = clEnqueueWriteBuffer(queue_, compositeBuffer_, CL_TRUE, 0, compositeBytes, composite.rgb.data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            setError(errorOut, "clEnqueueWriteBuffer composite failed: " + clErrorName(err));
            return false;
        }

        const int visibleW = visible.width;
        const int visibleH = visible.height;
        const int outW = outputWidth_;
        const int outH = outputHeight_;
        float visibleWeight = static_cast<float>(std::max(0.0, std::min(1.0, params.visibleWeight)));
        const int offsetX = params.visibleOffsetX;
        const int offsetY = params.visibleOffsetY;
        const int bilinear = params.enableBilinearResize ? 1 : 0;

        int arg = 0;
        err  = clSetKernelArg(kernel_, arg++, sizeof(cl_mem), &visibleBuffer_);
        err |= clSetKernelArg(kernel_, arg++, sizeof(cl_mem), &compositeBuffer_);
        err |= clSetKernelArg(kernel_, arg++, sizeof(cl_mem), &outputBuffer_);
        err |= clSetKernelArg(kernel_, arg++, sizeof(int), &visibleW);
        err |= clSetKernelArg(kernel_, arg++, sizeof(int), &visibleH);
        err |= clSetKernelArg(kernel_, arg++, sizeof(int), &outW);
        err |= clSetKernelArg(kernel_, arg++, sizeof(int), &outH);
        err |= clSetKernelArg(kernel_, arg++, sizeof(float), &visibleWeight);
        err |= clSetKernelArg(kernel_, arg++, sizeof(int), &offsetX);
        err |= clSetKernelArg(kernel_, arg++, sizeof(int), &offsetY);
        err |= clSetKernelArg(kernel_, arg++, sizeof(int), &bilinear);
        if (err != CL_SUCCESS) {
            setError(errorOut, "clSetKernelArg failed: " + clErrorName(err));
            return false;
        }

        const std::size_t globalWorkSize[2] = {
            static_cast<std::size_t>(outputWidth_ / 2),
            static_cast<std::size_t>(outputHeight_ / 2)
        };
        err = clEnqueueNDRangeKernel(queue_, kernel_, 2, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            setError(errorOut, "clEnqueueNDRangeKernel failed: " + clErrorName(err));
            return false;
        }
        err = clFinish(queue_);
        if (err != CL_SUCCESS) {
            setError(errorOut, "clFinish failed: " + clErrorName(err));
            return false;
        }

        outNv12->assign(outputBytes, 0);
        err = clEnqueueReadBuffer(queue_, outputBuffer_, CL_TRUE, 0, outputBytes, outNv12->data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            outNv12->clear();
            setError(errorOut, "clEnqueueReadBuffer output failed: " + clErrorName(err));
            return false;
        }
        return true;
#else
        setError(errorOut, "OpenCL backend was compiled without OpenCL support");
        return false;
#endif
    }

    std::string description() const {
        return description_.empty() ? std::string("OpenCL backend inactive") : description_;
    }

private:
    void release() {
#if TRI_FUSION_APP_HAS_OPENCL && TRI_FUSION_APP_HAS_OPENCL_HEADER
        if (outputBuffer_ != nullptr) { clReleaseMemObject(outputBuffer_); outputBuffer_ = nullptr; }
        if (compositeBuffer_ != nullptr) { clReleaseMemObject(compositeBuffer_); compositeBuffer_ = nullptr; }
        if (visibleBuffer_ != nullptr) { clReleaseMemObject(visibleBuffer_); visibleBuffer_ = nullptr; }
        if (kernel_ != nullptr) { clReleaseKernel(kernel_); kernel_ = nullptr; }
        if (program_ != nullptr) { clReleaseProgram(program_); program_ = nullptr; }
        if (queue_ != nullptr) { clReleaseCommandQueue(queue_); queue_ = nullptr; }
        if (context_ != nullptr) { clReleaseContext(context_); context_ = nullptr; }
#endif
        ready_ = false;
        platformName_.clear();
        platformVendor_.clear();
        deviceName_.clear();
        deviceVendor_.clear();
        openclVersion_.clear();
        description_.clear();
        computeUnits_ = 0;
        maxWorkGroupSize_ = 0;
        globalMemBytes_ = 0;
    }

private:
    bool ready_ = false;
    int maxVisibleWidth_ = 0;
    int maxVisibleHeight_ = 0;
    int outputWidth_ = 0;
    int outputHeight_ = 0;
    std::size_t visibleCapacityBytes_ = 0;
    std::size_t compositeCapacityBytes_ = 0;
    std::size_t outputCapacityBytes_ = 0;

    std::string platformName_;
    std::string platformVendor_;
    std::string deviceName_;
    std::string deviceVendor_;
    std::string openclVersion_;
    std::string description_;
    unsigned int computeUnits_ = 0;
    std::size_t maxWorkGroupSize_ = 0;
    std::uint64_t globalMemBytes_ = 0;

#if TRI_FUSION_APP_HAS_OPENCL && TRI_FUSION_APP_HAS_OPENCL_HEADER
    cl_platform_id platform_ = nullptr;
    cl_device_id device_ = nullptr;
    cl_context context_ = nullptr;
    cl_command_queue queue_ = nullptr;
    cl_program program_ = nullptr;
    cl_kernel kernel_ = nullptr;
    cl_mem visibleBuffer_ = nullptr;
    cl_mem compositeBuffer_ = nullptr;
    cl_mem outputBuffer_ = nullptr;
#endif
};

OpenClFusionBackend::OpenClFusionBackend() : impl_(new Impl()) {}
OpenClFusionBackend::~OpenClFusionBackend() { delete impl_; impl_ = nullptr; }

bool OpenClFusionBackend::init(int maxVisibleWidth,
                               int maxVisibleHeight,
                               int outputWidth,
                               int outputHeight,
                               std::string* errorOut) {
    return impl_ != nullptr && impl_->init(maxVisibleWidth, maxVisibleHeight, outputWidth, outputHeight, errorOut);
}

bool OpenClFusionBackend::isReady() const {
    return impl_ != nullptr && impl_->isReady();
}

bool OpenClFusionBackend::fuseToNv12(const RgbFrame& visible,
                                     const RgbFrame& composite,
                                     const GpuFusionParams& params,
                                     std::vector<std::uint8_t>* outNv12,
                                     std::string* errorOut) {
    if (impl_ == nullptr) {
        setError(errorOut, "OpenCL fusion impl is null");
        return false;
    }
    return impl_->fuseToNv12(visible, composite, params, outNv12, errorOut);
}

std::string OpenClFusionBackend::description() const {
    return impl_ != nullptr ? impl_->description() : std::string("OpenCL backend inactive");
}

} // namespace tri::media::app_fusion
