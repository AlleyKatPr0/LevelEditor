//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

#pragma once
#include "../Core/winheaders.h"
#include "../Core/NonCopyable.h"
#include <d3d11.h>

#ifndef LVED_ENABLE_NVRHI
#define LVED_ENABLE_NVRHI 0
#endif

#if LVED_ENABLE_NVRHI
#include <nvrhi/nvrhi.h>
#endif


namespace LvEdEngine
{

// A simple class for creating and initializing d3d11 device.
class DeviceManager : public NonCopyable
{

public:
	DeviceManager(void);
	~DeviceManager(void);

		
	ID3D11Device* GetDevice()
	{
		return m_pd3dDevice;
	}

	IDXGIFactory1* GetDXGIFactory1()
	{
		return m_pDXGIFactory1;
	}

	ID3D11DeviceContext* GetImmediateContext()
	{
		return m_pImmediateContext;
	}

#if LVED_ENABLE_NVRHI
	nvrhi::IDevice* GetNvrhiDevice() const
	{
		return m_nvrhiDevice.Get();
	}
#endif


private:	
	void InitializeNvrhi();
	void ShutdownNvrhi();

	ID3D11Device*           m_pd3dDevice;
	ID3D11DeviceContext*    m_pImmediateContext;		
	IDXGIFactory1*          m_pDXGIFactory1;

#if LVED_ENABLE_NVRHI
	nvrhi::DeviceHandle     m_nvrhiDevice;
#endif
    

};
extern DeviceManager* gD3D11;

}

//
//#ifdef _DEBUG
// deviceCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
// #endif
// ...