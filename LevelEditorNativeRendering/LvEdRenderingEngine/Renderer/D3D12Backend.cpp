//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

#include "D3D12Backend.h"

#include "../Core/WinHeaders.h"
#include <d3d12.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <dxcapi.h>
#include <stdlib.h>
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
