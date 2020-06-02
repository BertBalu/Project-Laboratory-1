//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "stdafx.h"
#include "DXSample.h"

#include "DxR/nv_helpers_dx12/TopLevelASGenerator.h"
#include "DxR/nv_helpers_dx12/ShaderBindingTableGenerator.h"

#include <vector>
#include "ObjectCreator.h"


using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class D3D12HelloTriangle : public DXSample {
public:
	enum class MaterialType {
		Diffuse,
		Specular,
		Refractive,
		Light
	};

	struct Material {
		XMVECTOR color;
		XMVECTOR emission;
		FLOAT type;
	};

	struct Light {
		XMVECTOR position;
		XMVECTOR size;
		XMVECTOR light;
	};

	struct VBObject {
		ComPtr<ID3D12Resource> pVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW sVertexBufferView;
		UINT uVertices;

		ComPtr<ID3D12Resource> pIndexBuffer;
		D3D12_INDEX_BUFFER_VIEW sIndexBufferView;
		UINT uIndices;

		std::vector<ComPtr<ID3D12Resource>> constantBuffers = {};
		XMMATRIX modelMatrix = XMMatrixIdentity( );
	};

	D3D12HelloTriangle(UINT width, UINT height, std::wstring name);

	virtual void OnInit( );
	virtual void OnUpdate( );
	virtual void OnRender( );
	virtual void OnDestroy( );

private:
	static const UINT FrameCount = 2;

	// DxR
	struct AccelerationStructureBuffers {
		ComPtr<ID3D12Resource> pScratch; // Scratch memory for AS builder
		ComPtr<ID3D12Resource> pResult;  // Where the AS is
		ComPtr<ID3D12Resource> pInstanceDesc; // Holt the matrices of the instance
	};

	nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
	nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper;

	AccelerationStructureBuffers m_topLevelASBuffers;
	std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> m_instances;

	ObjectCreator m_objectCreator;
	std::vector<VBObject> m_objects;
	ComPtr<ID3D12Resource> m_lights = {};

	// Raygen and trace
	ComPtr<IDxcBlob> m_rayGenLibrary;
	ComPtr<IDxcBlob> m_hitLibrary;
	ComPtr<IDxcBlob> m_missLibrary;
	ComPtr<IDxcBlob> m_shadowLiblary;

	ComPtr<ID3D12RootSignature> m_rayGenSignature;
	ComPtr<ID3D12RootSignature> m_hitSignature;
	ComPtr<ID3D12RootSignature> m_missSignature;
	ComPtr<ID3D12RootSignature> m_shadowSignature;

	ComPtr<ID3D12StateObject> m_rtStateObject;
	ComPtr<ID3D12StateObjectProperties> m_rtStateObjectProps;

	ComPtr<ID3D12RootSignature> m_compSignature;
	ComPtr<ID3D12PipelineState> m_compStateObject;

	ComPtr<ID3D12Resource> m_outputResource;
	ComPtr<ID3D12Resource> m_outputImage;
	ComPtr<ID3D12Resource> m_outputGradX;
	ComPtr<ID3D12Resource> m_outputGradY;
	ComPtr<ID3D12Resource> m_outputReconstruct;


	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

	ComPtr<ID3D12Resource> m_sbtStorage;

	// DxR extra
	ComPtr<ID3D12Resource> m_cameraBuffer;
	ComPtr<ID3D12Resource> m_frameBuffer;

	UINT32 m_cameraBufferSize = 0;
	UINT32 m_frameBufferSize = 0;
	XMUINT4 m_framesFromMove = {0,0,0,0};

	// Camera movement
	XMVECTOR Eye = XMVectorSet(-2.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMINT2 MousePosition;


	// Pipeline objects.
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device5> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList4> m_commandList;
	UINT m_rtvDescriptorSize;

	// Synchronization objects.
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	void LoadPipeline( );
	void LoadAssets( );
	void PopulateCommandList( );
	void CreateRayDesc(D3D12_DISPATCH_RAYS_DESC& desc);
	void WaitForPreviousFrame( );
	void CheckRaytracingSupport( );

	// DxR
	D3D12HelloTriangle::AccelerationStructureBuffers CreateBottomLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers, std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vIndexBuffers);
	void CreateTopLevelAS(const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances);
	void CreateAccelerationStructures( );

	ComPtr<ID3D12RootSignature> CreateRayGenSignature( );
	ComPtr<ID3D12RootSignature> CreateMissSignature( );
	ComPtr<ID3D12RootSignature> CreateHitSignature( );
	void CreateRaytracingPipeline( );

	void CreateRaytracingOutputBuffer( );
	void CreateShaderResourceHeap( );
	void CreateShaderBindingTable( );

	// DxR extra
	void CreateConstBuffers( );

	void UpdateCameraBuffer( );
	void UpdateFrameCountBuffer(UINT32 value);

	void CreateSphere( );
	void CreateSkyBox( );

	void CreateTable( );
	void CreateLight( );



	void CreateObject(std::vector<Vertex>& vertices, std::vector<UINT>& indices, D3D12HelloTriangle::Material material, XMMATRIX position = XMMatrixIdentity( ));

	void CreateVB(VBObject& object, std::vector<Vertex>& vertices);
	void CreateIB(VBObject& object, std::vector<UINT>& indices);
	void CreateMaterial(VBObject& object, Material material);
	void CreateLightBuffer(Light light);


	virtual void OnKeyDown(UINT8) override;
	virtual void OnButtonDown(UINT32) override;
	virtual void OnMouseMove(UINT8, UINT32) override;
};
