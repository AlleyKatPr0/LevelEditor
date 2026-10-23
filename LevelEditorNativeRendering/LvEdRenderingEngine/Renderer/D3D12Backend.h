//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

#pragma once

#include <stdint.h>

#ifdef LVEDRENDERINGENGINE_EXPORTS
#define LVEDRENDERINGENGINE_API __declspec(dllexport)
#else
#define LVEDRENDERINGENGINE_API __declspec(dllimport)
#endif

struct D3D12BackendCapabilities
{
    uint32_t featureLevel;
    uint32_t raytracingTier;
    uint32_t variableRateShadingTier;
    uint32_t meshShaderTier;
    uint32_t samplerFeedbackTier;
    uint32_t dxcAvailable;
};

struct D3D12RaytracingTriangleGeometry
{
    void* vertexBuffer;
    uint64_t vertexBufferOffset;
    uint32_t vertexCount;
    uint32_t vertexStride;
    void* indexBuffer;
    uint64_t indexBufferOffset;
    uint32_t indexCount;
    uint32_t indexFormatBits;
};

struct D3D12RaytracingInstance
{
    float transform[3][4];
    uint32_t instanceId;
    uint32_t instanceMask;
    uint32_t hitGroupContribution;
    uint32_t flags;
    void* bottomLevelAccelerationStructure;
};

extern "C" LVEDRENDERINGENGINE_API long __stdcall LvEd_D3D12Initialize();
extern "C" LVEDRENDERINGENGINE_API void __stdcall LvEd_D3D12Shutdown();
extern "C" LVEDRENDERINGENGINE_API bool __stdcall LvEd_D3D12GetCapabilities(
    D3D12BackendCapabilities* capabilities);
extern "C" LVEDRENDERINGENGINE_API long __stdcall LvEd_D3D12BuildBottomLevelAccelerationStructure(
    const D3D12RaytracingTriangleGeometry* geometry,
    void** accelerationStructure,
    uint64_t* gpuAddress);
extern "C" LVEDRENDERINGENGINE_API long __stdcall LvEd_D3D12BuildTopLevelAccelerationStructure(
    const D3D12RaytracingInstance* instances,
    uint32_t instanceCount,
    void** accelerationStructure,
    uint64_t* gpuAddress);
extern "C" LVEDRENDERINGENGINE_API void __stdcall LvEd_D3D12ReleaseResource(
    void* resource);
extern "C" LVEDRENDERINGENGINE_API long __stdcall LvEd_D3D12CompileShader(
    const wchar_t* sourcePath,
    const wchar_t* entryPoint,
    const wchar_t* targetProfile,
    void** bytecode,
    uint64_t* bytecodeSize);
extern "C" LVEDRENDERINGENGINE_API void __stdcall LvEd_D3D12ReleaseShader(void* bytecode);
