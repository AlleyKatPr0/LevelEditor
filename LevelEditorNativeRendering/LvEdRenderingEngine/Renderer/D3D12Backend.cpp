//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

#include "D3D12Backend.h"

#include "../Core/WinHeaders.h"
#include <d3d12.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <dxcapi.h>
#include <stdlib.h>
#include <mutex>
#include <string>

using Microsoft::WRL::ComPtr;

namespace
{
typedef HRESULT (WINAPI* D3D12CreateDeviceProc)(
    IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
typedef HRESULT (WINAPI* DxcCreateInstanceProc)(
    REFCLSID, REFIID, LPVOID*);

class D3D12Backend
{
public:
    D3D12Backend()
        : m_d3d12Module(NULL)
        , m_dxcModule(NULL)
        , m_fenceEvent(NULL)
        , m_dxcCreateInstance(NULL)
    {
        ZeroMemory(&m_capabilities, sizeof(m_capabilities));
    }

    ~D3D12Backend()
    {
        Shutdown();
    }

    HRESULT Initialize()
    {
        if (m_device)
            return S_OK;

        m_d3d12Module = LoadLibraryW(L"d3d12.dll");
        if (!m_d3d12Module)
            return HRESULT_FROM_WIN32(GetLastError());

        D3D12CreateDeviceProc createDevice =
            reinterpret_cast<D3D12CreateDeviceProc>(
                GetProcAddress(m_d3d12Module, "D3D12CreateDevice"));
        if (!createDevice)
        {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            Shutdown();
            return hr;
        }

        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_factory));
        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }

        hr = CreateDevice(createDevice);
        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }

        hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }

        hr = m_device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), NULL,
            IID_PPV_ARGS(&m_commandList));
        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }
        hr = m_commandList->Close();
        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }

        hr = m_device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        if (FAILED(hr))
        {
            Shutdown();
            return hr;
        }

        m_fenceEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!m_fenceEvent)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            Shutdown();
            return hr;
        }

        LoadDxc();
        QueryCapabilities();
        return S_OK;
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> lock(m_raytracingMutex);
        m_dxcIncludeHandler.Reset();
        m_dxcCompiler.Reset();
        m_dxcUtils.Reset();
        if (m_dxcModule)
        {
            FreeLibrary(m_dxcModule);
            m_dxcModule = NULL;
        }
        m_dxcCreateInstance = NULL;

        if (m_fenceEvent)
        {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = NULL;
        }

        m_fence.Reset();
        m_commandList.Reset();
        m_commandAllocator.Reset();
        m_commandQueue.Reset();
        m_device.Reset();
        m_adapter.Reset();
        m_factory.Reset();

        if (m_d3d12Module)
        {
            FreeLibrary(m_d3d12Module);
            m_d3d12Module = NULL;
        }

        ZeroMemory(&m_capabilities, sizeof(m_capabilities));
    }

    bool GetCapabilities(D3D12BackendCapabilities* capabilities) const
    {
        if (!capabilities || !m_device)
            return false;

        *capabilities = m_capabilities;
        return true;
    }

    HRESULT BuildBottomLevelAccelerationStructure(
        const D3D12RaytracingTriangleGeometry* geometry,
        ID3D12Resource** accelerationStructure)
    {
        std::lock_guard<std::mutex> lock(m_raytracingMutex);
        if (!geometry || !accelerationStructure || !geometry->vertexBuffer ||
            geometry->vertexCount < 3 || geometry->vertexStride < 12 ||
            geometry->vertexBufferOffset % 4 != 0)
            return E_INVALIDARG;

        *accelerationStructure = NULL;
        if ((geometry->indexCount == 0) != (geometry->indexBuffer == NULL) ||
            (geometry->indexCount != 0 &&
                (geometry->indexCount < 3 || geometry->indexCount % 3 != 0)) ||
            (geometry->indexCount != 0 &&
                geometry->indexFormatBits != 16 &&
                geometry->indexFormatBits != 32))
            return E_INVALIDARG;
        if (geometry->indexCount == 0 && geometry->vertexCount % 3 != 0)
            return E_INVALIDARG;

        ComPtr<ID3D12Device5> device;
        HRESULT hr = GetRaytracingDevice(&device);
        if (FAILED(hr))
            return hr;

        ComPtr<ID3D12Resource> vertices =
            static_cast<ID3D12Resource*>(geometry->vertexBuffer);
        const D3D12_RESOURCE_DESC vertexDesc = vertices->GetDesc();
        const uint64_t vertexBytes =
            static_cast<uint64_t>(geometry->vertexCount) * geometry->vertexStride;
        if (vertexDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
            geometry->vertexBufferOffset > vertexDesc.Width ||
            vertexBytes > vertexDesc.Width - geometry->vertexBufferOffset ||
            vertices->GetGPUVirtualAddress() == 0)
            return E_INVALIDARG;

        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geometryDesc.Triangles.VertexBuffer.StartAddress =
            vertices->GetGPUVirtualAddress() + geometry->vertexBufferOffset;
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = geometry->vertexStride;
        geometryDesc.Triangles.VertexCount = geometry->vertexCount;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

        ComPtr<ID3D12Resource> indices;
        if (geometry->indexCount != 0)
        {
            indices = static_cast<ID3D12Resource*>(geometry->indexBuffer);
            const D3D12_RESOURCE_DESC indexDesc = indices->GetDesc();
            const uint64_t indexBytes =
                static_cast<uint64_t>(geometry->indexCount) *
                (geometry->indexFormatBits / 8);
            if (indexDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
                geometry->indexBufferOffset > indexDesc.Width ||
                geometry->indexBufferOffset % (geometry->indexFormatBits / 8) != 0 ||
                indexBytes > indexDesc.Width - geometry->indexBufferOffset ||
                indices->GetGPUVirtualAddress() == 0)
                return E_INVALIDARG;

            geometryDesc.Triangles.IndexBuffer =
                indices->GetGPUVirtualAddress() + geometry->indexBufferOffset;
            geometryDesc.Triangles.IndexCount = geometry->indexCount;
            geometryDesc.Triangles.IndexFormat =
                geometry->indexFormatBits == 16
                    ? DXGI_FORMAT_R16_UINT
                    : DXGI_FORMAT_R32_UINT;
        }
        else
        {
            geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.NumDescs = 1;
        inputs.pGeometryDescs = &geometryDesc;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        return BuildAccelerationStructure(device.Get(), inputs, accelerationStructure);
    }

    HRESULT BuildTopLevelAccelerationStructure(
        const D3D12RaytracingInstance* instances,
        uint32_t instanceCount,
        ID3D12Resource** accelerationStructure)
    {
        std::lock_guard<std::mutex> lock(m_raytracingMutex);
        if (!instances || instanceCount == 0 || !accelerationStructure)
            return E_INVALIDARG;

        *accelerationStructure = NULL;
        ComPtr<ID3D12Device5> device;
        HRESULT hr = GetRaytracingDevice(&device);
        if (FAILED(hr))
            return hr;

        const uint64_t instanceBufferSize =
            static_cast<uint64_t>(instanceCount) *
            sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
        ComPtr<ID3D12Resource> instanceBuffer;
        hr = CreateBuffer(
            instanceBufferSize, D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE,
            &instanceBuffer);
        if (FAILED(hr))
            return hr;

        D3D12_RAYTRACING_INSTANCE_DESC* descriptions = NULL;
        D3D12_RANGE readRange = {};
        hr = instanceBuffer->Map(
            0, &readRange, reinterpret_cast<void**>(&descriptions));
        if (FAILED(hr))
            return hr;

        for (uint32_t i = 0; i < instanceCount; ++i)
        {
            if (!instances[i].bottomLevelAccelerationStructure ||
                instances[i].instanceId > 0xFFFFFF ||
                instances[i].hitGroupContribution > 0xFFFFFF ||
                instances[i].instanceMask > 0xFF ||
                instances[i].flags > 0x0F)
            {
                instanceBuffer->Unmap(0, NULL);
                return E_INVALIDARG;
            }

            ID3D12Resource* bottomLevel =
                static_cast<ID3D12Resource*>(
                    instances[i].bottomLevelAccelerationStructure);
            if (bottomLevel->GetGPUVirtualAddress() == 0)
            {
                instanceBuffer->Unmap(0, NULL);
                return E_INVALIDARG;
            }

            CopyMemory(descriptions[i].Transform, instances[i].transform,
                sizeof(descriptions[i].Transform));
            descriptions[i].InstanceID = instances[i].instanceId;
            descriptions[i].InstanceMask = instances[i].instanceMask;
            descriptions[i].InstanceContributionToHitGroupIndex =
                instances[i].hitGroupContribution;
            descriptions[i].Flags =
                static_cast<D3D12_RAYTRACING_INSTANCE_FLAGS>(instances[i].flags);
            descriptions[i].AccelerationStructure =
                bottomLevel->GetGPUVirtualAddress();
        }
        instanceBuffer->Unmap(0, NULL);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.NumDescs = instanceCount;
        inputs.InstanceDescs = instanceBuffer->GetGPUVirtualAddress();
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        return BuildAccelerationStructure(device.Get(), inputs, accelerationStructure);
    }

    HRESULT CompileShader(
        const wchar_t* sourcePath,
        const wchar_t* entryPoint,
        const wchar_t* targetProfile,
        IDxcBlob** bytecode)
    {
        if (!sourcePath || !entryPoint || !targetProfile || !bytecode)
            return E_INVALIDARG;

        *bytecode = NULL;
        if (!m_dxcCompiler || !m_dxcUtils || !m_dxcIncludeHandler)
            return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);

        ComPtr<IDxcBlobEncoding> source;
        UINT32 codePage = DXC_CP_UTF8;
        HRESULT hr = m_dxcUtils->LoadFile(sourcePath, &codePage, &source);
        if (FAILED(hr))
            return hr;

        DxcBuffer buffer = {};
        buffer.Ptr = source->GetBufferPointer();
        buffer.Size = source->GetBufferSize();
        buffer.Encoding = codePage;

        std::wstring includePath(sourcePath);
        const size_t slash = includePath.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
            includePath.resize(slash);
        else
            includePath = L".";

        LPCWSTR arguments[] =
        {
            L"-E", entryPoint,
            L"-T", targetProfile,
            L"-I", includePath.c_str()
        };

        ComPtr<IDxcResult> result;
        hr = m_dxcCompiler->Compile(
            &buffer, arguments, ARRAYSIZE(arguments), m_dxcIncludeHandler.Get(),
            IID_PPV_ARGS(&result));
        if (FAILED(hr))
            return hr;

        HRESULT compileStatus = E_FAIL;
        hr = result->GetStatus(&compileStatus);
        if (FAILED(hr))
            return hr;

        if (FAILED(compileStatus))
        {
            ComPtr<IDxcBlobUtf8> diagnostics;
            if (SUCCEEDED(result->GetOutput(
                DXC_OUT_ERRORS, IID_PPV_ARGS(&diagnostics), NULL)) &&
                diagnostics && diagnostics->GetStringLength() > 0)
            {
                OutputDebugStringA(diagnostics->GetStringPointer());
            }
            return compileStatus;
        }

        ComPtr<IDxcBlob> object;
        hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), NULL);
        if (FAILED(hr) || !object)
            return FAILED(hr) ? hr : E_FAIL;

        *bytecode = object.Detach();
        return S_OK;
    }

private:
    HRESULT GetRaytracingDevice(ComPtr<ID3D12Device5>* device)
    {
        if (!device)
            return E_INVALIDARG;
        device->Reset();
        if (!m_device || m_capabilities.raytracingTier == 0)
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        return m_device.As(device);
    }

    HRESULT CreateBuffer(
        uint64_t size,
        D3D12_HEAP_TYPE heapType,
        D3D12_RESOURCE_STATES initialState,
        D3D12_RESOURCE_FLAGS flags,
        ComPtr<ID3D12Resource>* resource)
    {
        if (!resource || size == 0)
            return E_INVALIDARG;
        resource->Reset();

        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = heapType;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = flags;

        return m_device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, NULL,
            IID_PPV_ARGS(resource->GetAddressOf()));
    }

    HRESULT BuildAccelerationStructure(
        ID3D12Device5* device,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
        ID3D12Resource** accelerationStructure)
    {
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
        device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
        if (info.ResultDataMaxSizeInBytes == 0 ||
            info.ScratchDataSizeInBytes == 0)
            return E_FAIL;

        ComPtr<ID3D12Resource> result;
        HRESULT hr = CreateBuffer(
            info.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &result);
        if (FAILED(hr))
            return hr;

        ComPtr<ID3D12Resource> scratch;
        hr = CreateBuffer(
            info.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &scratch);
        if (FAILED(hr))
            return hr;

        hr = m_commandAllocator->Reset();
        if (FAILED(hr))
            return hr;
        hr = m_commandList->Reset(m_commandAllocator.Get(), NULL);
        if (FAILED(hr))
            return hr;

        ComPtr<ID3D12GraphicsCommandList4> commandList;
        hr = m_commandList.As(&commandList);
        if (FAILED(hr))
            return hr;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
        build.Inputs = inputs;
        build.DestAccelerationStructureData = result->GetGPUVirtualAddress();
        build.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
        commandList->BuildRaytracingAccelerationStructure(&build, 0, NULL);
        hr = m_commandList->Close();
        if (FAILED(hr))
            return hr;

        ID3D12CommandList* commandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, commandLists);
        hr = WaitForGpu();
        if (FAILED(hr))
            return hr;

        *accelerationStructure = result.Detach();
        return S_OK;
    }

    HRESULT WaitForGpu()
    {
        if (m_fenceValue == UINT64_MAX)
            return E_FAIL;
        const UINT64 value = ++m_fenceValue;
        HRESULT hr = m_commandQueue->Signal(m_fence.Get(), value);
        if (FAILED(hr))
            return hr;
        if (m_fence->GetCompletedValue() < value)
        {
            hr = m_fence->SetEventOnCompletion(value, m_fenceEvent);
            if (FAILED(hr))
                return hr;
            if (WaitForSingleObject(m_fenceEvent, INFINITE) != WAIT_OBJECT_0)
                return HRESULT_FROM_WIN32(GetLastError());
        }
        return S_OK;
    }

    HRESULT CreateDevice(D3D12CreateDeviceProc createDevice)
    {
        HRESULT lastError = DXGI_ERROR_NOT_FOUND;
        for (UINT adapterIndex = 0; ; ++adapterIndex)
        {
            ComPtr<IDXGIAdapter1> adapter;
            HRESULT hr = m_factory->EnumAdapters1(adapterIndex, &adapter);
            if (hr == DXGI_ERROR_NOT_FOUND)
                break;
            if (FAILED(hr))
                return hr;

            DXGI_ADAPTER_DESC1 desc;
            hr = adapter->GetDesc1(&desc);
            if (FAILED(hr))
                continue;
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            const D3D_FEATURE_LEVEL featureLevels[] =
            {
                D3D_FEATURE_LEVEL_12_2,
                D3D_FEATURE_LEVEL_12_1,
                D3D_FEATURE_LEVEL_12_0,
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0
            };
            for (size_t i = 0; i < ARRAYSIZE(featureLevels); ++i)
            {
                hr = createDevice(
                    adapter.Get(), featureLevels[i], IID_PPV_ARGS(&m_device));
                if (SUCCEEDED(hr))
                {
                    m_adapter = adapter;
                    m_capabilities.featureLevel =
                        static_cast<uint32_t>(featureLevels[i]);
                    return S_OK;
                }
                lastError = hr;
            }
        }
        return lastError;
    }

    void LoadDxc()
    {
        m_dxcModule = LoadLibraryW(L"dxcompiler.dll");
        if (!m_dxcModule)
            return;

        m_dxcCreateInstance = reinterpret_cast<DxcCreateInstanceProc>(
            GetProcAddress(m_dxcModule, "DxcCreateInstance"));
        if (!m_dxcCreateInstance ||
            FAILED(m_dxcCreateInstance(
                CLSID_DxcUtils, __uuidof(IDxcUtils),
                reinterpret_cast<void**>(m_dxcUtils.GetAddressOf()))) ||
            FAILED(m_dxcCreateInstance(
                CLSID_DxcCompiler, __uuidof(IDxcCompiler3),
                reinterpret_cast<void**>(m_dxcCompiler.GetAddressOf()))) ||
            FAILED(m_dxcUtils->CreateDefaultIncludeHandler(
                &m_dxcIncludeHandler)))
        {
            m_dxcIncludeHandler.Reset();
            m_dxcCompiler.Reset();
            m_dxcUtils.Reset();
            FreeLibrary(m_dxcModule);
            m_dxcModule = NULL;
            m_dxcCreateInstance = NULL;
            return;
        }
        m_capabilities.dxcAvailable = 1;
    }

    void QueryCapabilities()
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
        if (SUCCEEDED(m_device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
            m_capabilities.raytracingTier =
                static_cast<uint32_t>(options5.RaytracingTier);

        D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = {};
        if (SUCCEEDED(m_device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6))))
            m_capabilities.variableRateShadingTier =
                static_cast<uint32_t>(options6.VariableShadingRateTier);

        D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
        if (SUCCEEDED(m_device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))))
        {
            m_capabilities.meshShaderTier =
                static_cast<uint32_t>(options7.MeshShaderTier);
            m_capabilities.samplerFeedbackTier =
                static_cast<uint32_t>(options7.SamplerFeedbackTier);
        }
    }

    HMODULE m_d3d12Module;
    HMODULE m_dxcModule;
    HANDLE m_fenceEvent;
    DxcCreateInstanceProc m_dxcCreateInstance;
    ComPtr<IDXGIFactory1> m_factory;
    ComPtr<IDXGIAdapter1> m_adapter;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12Fence> m_fence;
    ComPtr<IDxcUtils> m_dxcUtils;
    ComPtr<IDxcCompiler3> m_dxcCompiler;
    ComPtr<IDxcIncludeHandler> m_dxcIncludeHandler;
    D3D12BackendCapabilities m_capabilities;
    std::mutex m_raytracingMutex;
    UINT64 m_fenceValue = 0;
};

D3D12Backend s_backend;
}

extern "C" long __stdcall LvEd_D3D12Initialize()
{
    return s_backend.Initialize();
}

extern "C" void __stdcall LvEd_D3D12Shutdown()
{
    s_backend.Shutdown();
}

extern "C" bool __stdcall LvEd_D3D12GetCapabilities(
    D3D12BackendCapabilities* capabilities)
{
    return s_backend.GetCapabilities(capabilities);
}

extern "C" long __stdcall LvEd_D3D12BuildBottomLevelAccelerationStructure(
    const D3D12RaytracingTriangleGeometry* geometry,
    void** accelerationStructure,
    uint64_t* gpuAddress)
{
    if (!accelerationStructure || !gpuAddress)
        return E_INVALIDARG;

    *accelerationStructure = NULL;
    *gpuAddress = 0;
    ID3D12Resource* result = NULL;
    HRESULT hr = s_backend.BuildBottomLevelAccelerationStructure(geometry, &result);
    if (FAILED(hr))
        return hr;

    *gpuAddress = result->GetGPUVirtualAddress();
    *accelerationStructure = result;
    return S_OK;
}

extern "C" long __stdcall LvEd_D3D12BuildTopLevelAccelerationStructure(
    const D3D12RaytracingInstance* instances,
    uint32_t instanceCount,
    void** accelerationStructure,
    uint64_t* gpuAddress)
{
    if (!accelerationStructure || !gpuAddress)
        return E_INVALIDARG;

    *accelerationStructure = NULL;
    *gpuAddress = 0;
    ID3D12Resource* result = NULL;
    HRESULT hr = s_backend.BuildTopLevelAccelerationStructure(
        instances, instanceCount, &result);
    if (FAILED(hr))
        return hr;

    *gpuAddress = result->GetGPUVirtualAddress();
    *accelerationStructure = result;
    return S_OK;
}

extern "C" void __stdcall LvEd_D3D12ReleaseResource(void* resource)
{
    if (resource)
        static_cast<ID3D12Resource*>(resource)->Release();
}

extern "C" long __stdcall LvEd_D3D12CompileShader(
    const wchar_t* sourcePath,
    const wchar_t* entryPoint,
    const wchar_t* targetProfile,
    void** bytecode,
    uint64_t* bytecodeSize)
{
    if (!bytecode || !bytecodeSize)
        return E_INVALIDARG;

    *bytecode = NULL;
    *bytecodeSize = 0;

    IDxcBlob* shader = NULL;
    HRESULT hr = s_backend.CompileShader(
        sourcePath, entryPoint, targetProfile, &shader);
    if (FAILED(hr))
        return hr;

    const size_t size = shader->GetBufferSize();
    void* copy = malloc(size);
    if (!copy)
    {
        shader->Release();
        return E_OUTOFMEMORY;
    }

    CopyMemory(copy, shader->GetBufferPointer(), size);
    shader->Release();
    *bytecode = copy;
    *bytecodeSize = static_cast<uint64_t>(size);
    return S_OK;
}

extern "C" void __stdcall LvEd_D3D12ReleaseShader(void* bytecode)
{
    if (bytecode)
        free(bytecode);
}
