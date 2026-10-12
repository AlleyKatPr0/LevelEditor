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

extern "C" LVEDRENDERINGENGINE_API long __stdcall LvEd_D3D12Initialize();
extern "C" LVEDRENDERINGENGINE_API void __stdcall LvEd_D3D12Shutdown();
extern "C" LVEDRENDERINGENGINE_API bool __stdcall LvEd_D3D12GetCapabilities(
    D3D12BackendCapabilities* capabilities);
extern "C" LVEDRENDERINGENGINE_API long __stdcall LvEd_D3D12CompileShader(
    const wchar_t* sourcePath,
    const wchar_t* entryPoint,
    const wchar_t* targetProfile,
    void** bytecode,
    uint64_t* bytecodeSize);
extern "C" LVEDRENDERINGENGINE_API void __stdcall LvEd_D3D12ReleaseShader(void* bytecode);
