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


#include "D3D12HelloTriangle.h"
#include <stdexcept>

#include "DxR/DXRHelper.h"
#include "DxR/nv_helpers_dx12/BottomLevelASGenerator.h"
#include "DxR/nv_helpers_dx12/RaytracingPipelineGenerator.h"
#include "DxR/nv_helpers_dx12/RootSignatureGenerator.h"

#include <windowsx.h>

#include <system_error>

D3D12HelloTriangle::D3D12HelloTriangle(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_rtvDescriptorSize(0) { }

void D3D12HelloTriangle::OnInit( ) {
	LoadPipeline( );
	LoadAssets( );

	CheckRaytracingSupport( );
	CreateAccelerationStructures( );

	ThrowIfFailed(m_commandList->Close( ));

	CreateRaytracingPipeline( );
	CreateRaytracingOutputBuffer( );


	CreateConstBuffers( );
	CreateShaderResourceHeap( );
	CreateShaderBindingTable( );
}

// Load the rendering pipeline dependencies.
void D3D12HelloTriangle::LoadPipeline( ) {
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer( );

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_useWarpDevice) {
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get( ),
			D3D_FEATURE_LEVEL_12_1,
			IID_PPV_ARGS(&m_device)
		));
	} else {
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get( ), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get( ),
			D3D_FEATURE_LEVEL_12_1,
			IID_PPV_ARGS(&m_device)
		));
	}

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get( ),		// Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd( ),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd( ), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex( );

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart( ));

		// Create a RTV for each frame.
		for (UINT n = 0; n < FrameCount; n++) {
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get( ), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}
	}

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void D3D12HelloTriangle::LoadAssets( ) {
	{
		CD3DX12_DESCRIPTOR_RANGE uav;
		uav.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0);

		CD3DX12_ROOT_PARAMETER param;
		param.InitAsDescriptorTable(1, &uav, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(1, &param, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer( ), signature->GetBufferSize( ), IID_PPV_ARGS(&m_compSignature)));
	}

	{
		ComPtr<ID3DBlob> computeShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"compute.hlsl").c_str( ), nullptr, nullptr, "CSMain", "cs_5_0", compileFlags, 0, &computeShader, nullptr));

		D3D12_COMPUTE_PIPELINE_STATE_DESC compDesc = {};
		compDesc.pRootSignature = m_compSignature.Get( );
		compDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get( ));

		ThrowIfFailed(m_device->CreateComputePipelineState(&compDesc, IID_PPV_ARGS(&m_compStateObject)));
	}
	
	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get( ), m_compStateObject.Get( ), IID_PPV_ARGS(&m_commandList)));

	{
		CreateSphere( );
		CreateSkyBox( );
		CreateTable( );
		CreateLight( );
	}

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr) {
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError( )));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForPreviousFrame( );
	}
}

// Update frame-based values.
void D3D12HelloTriangle::OnUpdate( ) {
	UpdateCameraBuffer( );
	UpdateFrameCountBuffer(m_framesFromMove.x + 1);
}

// Render the scene.
void D3D12HelloTriangle::OnRender( ) {
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList( );

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = {m_commandList.Get( )};
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame( );
}

void D3D12HelloTriangle::OnDestroy( ) {
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame( );

	CloseHandle(m_fenceEvent);
}

void D3D12HelloTriangle::PopulateCommandList( ) {
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocator->Reset( ));

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get( ), m_compStateObject.Get( )));

	// Set necessary state.
	//m_commandList->SetGraphicsRootSignature(m_rootSignature.Get( ));
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get( ), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart( ), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	std::vector<ID3D12DescriptorHeap*> heaps = {m_srvUavHeap.Get( )};
	m_commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size( )), heaps.data( ));

	CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
		m_outputResource.Get( ), D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_commandList->ResourceBarrier(1, &transition);

	D3D12_DISPATCH_RAYS_DESC desc = {};
	CreateRayDesc(desc);

	m_commandList->SetPipelineState1(m_rtStateObject.Get( ));
	m_commandList->DispatchRays(&desc);

	//m_commandList->ResourceBarrier(1, &transition);


	//m_commandList->SetComputeRootSignature(m_compSignature.Get( ));
	//m_commandList->SetPipelineState(m_compStateObject.Get());
	//m_commandList->Dispatch(1, 1, 1);

	


	transition = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_frameIndex].Get( ), D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_COPY_DEST);

	m_commandList->ResourceBarrier(1, &transition);
	m_commandList->CopyResource(m_renderTargets[m_frameIndex].Get( ), m_outputResource.Get( ));

	transition = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_frameIndex].Get( ), D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &transition);


	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get( ), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close( ));
}

void D3D12HelloTriangle::CreateRayDesc(D3D12_DISPATCH_RAYS_DESC& desc) {
	UINT32 rayGenerationSectionSizeInBytes = m_sbtHelper.GetRayGenSectionSize( );
	desc.RayGenerationShaderRecord.StartAddress = m_sbtStorage->GetGPUVirtualAddress( );
	desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

	UINT32 missSectionSizeInBytes = m_sbtHelper.GetMissSectionSize( );
	desc.MissShaderTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress( ) + rayGenerationSectionSizeInBytes;
	desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
	desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize( );

	UINT32 hitGroupsSectionSize = m_sbtHelper.GetHitGroupSectionSize( );
	desc.HitGroupTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress( ) +
		rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
	desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
	desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize( );

	desc.Width = GetWidth( );
	desc.Height = GetHeight( );
	desc.Depth = 1;
}

void D3D12HelloTriangle::WaitForPreviousFrame( ) {
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get( ), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue( ) < fence) {
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}


	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex( );
}

void D3D12HelloTriangle::CheckRaytracingSupport( ) {
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
				  &options5, sizeof(options5)));
	if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
		throw std::runtime_error("Raytracing not supported on device");
}





D3D12HelloTriangle::AccelerationStructureBuffers
D3D12HelloTriangle::CreateBottomLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers,
										std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vIndexBuffers) {
	nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

	// Adding all vertex buffers and not transforming their position.
	for (size_t i = 0; i < vVertexBuffers.size( ); i++) {
		bottomLevelAS.AddVertexBuffer(vVertexBuffers[i].first.Get( ), 0, vVertexBuffers[i].second, sizeof(Vertex),
									  vIndexBuffers[i].first.Get( ), 0, vIndexBuffers[i].second,
									  nullptr, 0, true);
	}

	UINT64 scratchSizeInBytes = 0;
	UINT64 resultSizeInBytes = 0;

	bottomLevelAS.ComputeASBufferSizes(m_device.Get( ), false, &scratchSizeInBytes, &resultSizeInBytes);

	AccelerationStructureBuffers buffers;
	buffers.pScratch = nv_helpers_dx12::CreateBuffer(m_device.Get( ), scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
													 D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);

	buffers.pResult = nv_helpers_dx12::CreateBuffer(m_device.Get( ), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
													D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps);

	bottomLevelAS.Generate(m_commandList.Get( ), buffers.pScratch.Get( ), buffers.pResult.Get( ), false, nullptr);
	return buffers;
}

void D3D12HelloTriangle::CreateTopLevelAS(const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances) {
	for (size_t i = 0; i < instances.size( ); i++) {
		m_topLevelASGenerator.AddInstance(instances[i].first.Get( ), instances[i].second, static_cast<UINT>(i), static_cast<UINT>(2 * i));
	}

	UINT64 scratchSize, resultSize, instanceDescsSize;

	m_topLevelASGenerator.ComputeASBufferSizes(m_device.Get( ), true, &scratchSize, &resultSize, &instanceDescsSize);


	m_topLevelASBuffers.pScratch = nv_helpers_dx12::CreateBuffer(
		m_device.Get( ), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps);
	m_topLevelASBuffers.pResult = nv_helpers_dx12::CreateBuffer(
		m_device.Get( ), resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps);
	m_topLevelASBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(
		m_device.Get( ), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	m_topLevelASGenerator.Generate(m_commandList.Get( ), m_topLevelASBuffers.pScratch.Get( ), m_topLevelASBuffers.pResult.Get( ), m_topLevelASBuffers.pInstanceDesc.Get( ));
}

void D3D12HelloTriangle::CreateAccelerationStructures( ) {
	for (size_t i = 0; i < m_objects.size( ); i++) {
		AccelerationStructureBuffers BLASBuffer = CreateBottomLevelAS({{m_objects[i].pVertexBuffer.Get( ), m_objects[i].uVertices}}, {{m_objects[i].pIndexBuffer.Get( ), m_objects[i].uIndices}});
		m_instances.push_back({BLASBuffer.pResult, m_objects[i].modelMatrix});
	}
	CreateTopLevelAS(m_instances);

	m_commandList->Close( );
	ID3D12CommandList* ppCommandLists[] = {m_commandList.Get( )};
	m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
	m_fenceValue++;
	m_commandQueue->Signal(m_fence.Get( ), m_fenceValue);

	m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
	WaitForSingleObject(m_fenceEvent, INFINITE);

	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get( ), m_pipelineState.Get( )));
}

ComPtr<ID3D12RootSignature> D3D12HelloTriangle::CreateRayGenSignature( ) {
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddHeapRangesParameter({{0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,0},
							   {1,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1},
							   {2,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,2},
							   {3,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,3},
							   {4,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,4},
							   {0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,5},
							   {0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_CBV,6}});
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 1);

	return rsc.Generate(m_device.Get( ), true);
}

ComPtr<ID3D12RootSignature> D3D12HelloTriangle::CreateMissSignature( ) {
	nv_helpers_dx12::RootSignatureGenerator rsc;
	return rsc.Generate(m_device.Get( ), true);
}

ComPtr<ID3D12RootSignature> D3D12HelloTriangle::CreateHitSignature( ) {
	nv_helpers_dx12::RootSignatureGenerator rsc;

	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 1);
	rsc.AddHeapRangesParameter({{2,1,0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5}});

	return rsc.Generate(m_device.Get( ), true);
}

void D3D12HelloTriangle::CreateRaytracingPipeline( ) {
	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(m_device.Get( ));

	m_rayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders/RayGen.hlsl");
	m_missLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders/Miss.hlsl");
	m_hitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders/Hit.hlsl");
	m_shadowLiblary = nv_helpers_dx12::CompileShaderLibrary(L"Shaders/ShadowRay.hlsl");

	pipeline.AddLibrary(m_rayGenLibrary.Get( ), {L"RayGen"});
	pipeline.AddLibrary(m_missLibrary.Get( ), {L"Miss"});
	pipeline.AddLibrary(m_hitLibrary.Get( ), {L"ObjectClosestHit"});
	pipeline.AddLibrary(m_shadowLiblary.Get( ), {L"ShadowClosestHit", L"ShadowMiss"});

	m_rayGenSignature = CreateRayGenSignature( );
	m_missSignature = CreateMissSignature( );
	m_hitSignature = CreateHitSignature( );
	m_shadowSignature = CreateHitSignature( );

	pipeline.AddHitGroup(L"HitGroup", L"ObjectClosestHit");
	pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit");

	pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get( ), {L"RayGen"});
	pipeline.AddRootSignatureAssociation(m_missSignature.Get( ), {L"Miss", L"ShadowMiss"});
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get( ), {L"HitGroup"});
	pipeline.AddRootSignatureAssociation(m_shadowSignature.Get( ), {L"ShadowHitGroup"});

	pipeline.SetMaxPayloadSize(7 * sizeof(float));
	pipeline.SetMaxAttributeSize(2 * sizeof(float));
	pipeline.SetMaxRecursionDepth(10);

	m_rtStateObject = pipeline.Generate( );
	ThrowIfFailed(m_rtStateObject->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps)));
}


void D3D12HelloTriangle::CreateRaytracingOutputBuffer( ) {
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	resDesc.Width = GetWidth( );
	resDesc.Height = GetHeight( );
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;

	ThrowIfFailed(m_device->CreateCommittedResource(
		&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_outputResource)
	));

	ThrowIfFailed(m_device->CreateCommittedResource(
		&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_outputImage)
	));
	
	ThrowIfFailed(m_device->CreateCommittedResource(
		&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_outputGradX)
	));

	ThrowIfFailed(m_device->CreateCommittedResource(
		&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_outputGradY)
	));
	
	ThrowIfFailed(m_device->CreateCommittedResource(
		&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_outputReconstruct)
	));
}

void D3D12HelloTriangle::CreateShaderResourceHeap( ) {
	m_srvUavHeap = nv_helpers_dx12::CreateDescriptorHeap(m_device.Get( ), 7, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart( );

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	// Output
	m_device->CreateUnorderedAccessView(m_outputResource.Get( ), nullptr, &uavDesc, srvHandle);
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Image
	m_device->CreateUnorderedAccessView(m_outputImage.Get( ), nullptr, &uavDesc, srvHandle);
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	// GradX
	m_device->CreateUnorderedAccessView(m_outputGradX.Get( ), nullptr, &uavDesc, srvHandle);
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Grad Y
	m_device->CreateUnorderedAccessView(m_outputGradY.Get( ), nullptr, &uavDesc, srvHandle);
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Reconstr
	m_device->CreateUnorderedAccessView(m_outputReconstruct.Get( ), nullptr, &uavDesc, srvHandle);
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = m_topLevelASBuffers.pResult->GetGPUVirtualAddress( );

	m_device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_cameraBuffer->GetGPUVirtualAddress( );
	cbvDesc.SizeInBytes = m_cameraBufferSize;

	m_device->CreateConstantBufferView(&cbvDesc, srvHandle);


}

void D3D12HelloTriangle::CreateShaderBindingTable( ) {
	m_sbtHelper.Reset( );
	D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart( );

	auto heapPointer = reinterpret_cast<UINT64*>(srvUavHeapHandle.ptr);

	m_sbtHelper.AddRayGenerationProgram(L"RayGen", {heapPointer, (void*) m_frameBuffer->GetGPUVirtualAddress()});
	m_sbtHelper.AddMissProgram(L"Miss", {});
	m_sbtHelper.AddMissProgram(L"ShadowMiss", {});

	for (auto object : m_objects) {
		m_sbtHelper.AddHitGroup(L"HitGroup", {(void*) object.pVertexBuffer->GetGPUVirtualAddress( ),
								(void*) object.pIndexBuffer->GetGPUVirtualAddress( ),(void*) object.constantBuffers[0]->GetGPUVirtualAddress( ),
								(void*) m_lights->GetGPUVirtualAddress( ), heapPointer});
		m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {(void*) object.pVertexBuffer->GetGPUVirtualAddress( ),
								(void*) object.pIndexBuffer->GetGPUVirtualAddress( ),(void*) object.constantBuffers[0]->GetGPUVirtualAddress( ),
								(void*) m_lights->GetGPUVirtualAddress( ), heapPointer});
	}

	UINT32 sbtSize = m_sbtHelper.ComputeSBTSize( );

	m_sbtStorage = nv_helpers_dx12::CreateBuffer(m_device.Get( ), sbtSize, D3D12_RESOURCE_FLAG_NONE,
												 D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	if (!m_sbtStorage) {
		throw std::logic_error("Could not allocate the shader binding table");
	}

	m_sbtHelper.Generate(m_sbtStorage.Get( ), m_rtStateObjectProps.Get( ));
}


void D3D12HelloTriangle::CreateConstBuffers( ) {
	UINT32 nbMatrix = 4; //V, P, Vinv, Pinv
	m_cameraBufferSize = nbMatrix * sizeof(XMMATRIX);
	m_frameBufferSize = sizeof(XMUINT4);

	m_frameBuffer = nv_helpers_dx12::CreateBuffer(
		m_device.Get( ), m_frameBufferSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps
	);

	m_cameraBuffer = nv_helpers_dx12::CreateBuffer(
		m_device.Get( ), m_cameraBufferSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps
	);
}

void D3D12HelloTriangle::UpdateCameraBuffer( ) {
	std::vector<XMMATRIX> matrices(4);
	matrices[0] = XMMatrixLookAtRH(Eye, At, Up);

	float fovAngleY = 75.0f * XM_PI / 180.0f;
	matrices[1] = XMMatrixPerspectiveFovRH(fovAngleY, m_aspectRatio, 0.1f, 1000.f);

	XMVECTOR det;
	matrices[2] = XMMatrixInverse(&det, matrices[0]);
	matrices[3] = XMMatrixInverse(&det, matrices[1]);

	UINT8* pData;
	ThrowIfFailed(m_cameraBuffer->Map(0, nullptr, (void**) &pData));
	memcpy(pData, matrices.data( ), m_cameraBufferSize);
	m_cameraBuffer->Unmap(0, nullptr);
}

void D3D12HelloTriangle::UpdateFrameCountBuffer(UINT32 value) {
	m_framesFromMove.x = value;

	UINT8* pData;
	ThrowIfFailed(m_frameBuffer->Map(0, nullptr, (void**) &pData));
	memcpy(pData, &m_framesFromMove, m_frameBufferSize);
	m_frameBuffer->Unmap(0, nullptr);
}





void D3D12HelloTriangle::CreateSphere( ) {
	auto sphere = m_objectCreator.CreateSphere(1.0f);
	CreateObject(sphere.Vertices, sphere.Indices, {{1, 1, 1, 1.0f}, {0}, 1});
}

void D3D12HelloTriangle::CreateSkyBox( ) {
	auto sky = m_objectCreator.CreateBox({8, 10, 8}, {1,1,1});
	CreateObject(sky.Vertices, sky.Indices, {{0.8, 0.8, 0.8, 1.0f}, {0}, 0}, XMMatrixTranslation(0, 0, 0));
}

void D3D12HelloTriangle::CreateTable( ) {
	auto tlLeg = m_objectCreator.CreateBox({0.5,3,0.5}, {1,1,1});
	CreateObject(tlLeg.Vertices, tlLeg.Indices, {{0.960, 0.949, 0.6, 1.0f}, {0}, 0}, XMMatrixTranslation(-2, 0, -2));

	auto trLeg = m_objectCreator.CreateBox({0.5,3,0.5}, {1,1,1});
	CreateObject(trLeg.Vertices, trLeg.Indices, {{0.960, 0.6, 0.933, 1.0f}, {0}, 0}, XMMatrixTranslation(2, 0, -2));

	auto brLeg = m_objectCreator.CreateBox({0.5,3,0.5}, {1,1,1});
	CreateObject(brLeg.Vertices, brLeg.Indices, {{0.6, 0.725, 0.960, 1.0f}, {0}, 0}, XMMatrixTranslation(-2, 0, 2));

	auto blLeg = m_objectCreator.CreateBox({0.5,3,0.5}, {1,1,1});
	CreateObject(blLeg.Vertices, blLeg.Indices, {{0.698, 0.960, 0.6, 1.0f}, {0}, 0}, XMMatrixTranslation(2, 0, 2));

	auto face = m_objectCreator.CreateBox({4.5,0.5,4.5}, {1,1,1});
	CreateObject(face.Vertices, face.Indices, {{0.960, 0.6, 0.717, 1.0f}, {0}, 1}, XMMatrixTranslation(0, -1.75, 0));
}

void D3D12HelloTriangle::CreateLight( ) {
	auto light = m_objectCreator.CreateBox({1, 0.1, 1}, {1,1,1});
	CreateObject(light.Vertices, light.Indices, {{1, 1, 1}, {10, 10, 10}, 3}, XMMatrixTranslation(0, 4.5, 0));

	CreateLightBuffer({{0, 4.5, 0, 1}, {1, 0.1, 1}, {10, 10, 10, 0}});
}

void D3D12HelloTriangle::CreateObject(std::vector<Vertex>& vertices, std::vector<UINT>& indices, Material material, XMMATRIX position) {
	VBObject object;

	CreateVB(object, vertices);
	CreateIB(object, indices);
	CreateMaterial(object, material);

	object.modelMatrix = position;
	m_objects.push_back(object);
}

void D3D12HelloTriangle::CreateVB(VBObject& object, std::vector<Vertex>& vertices) {
	object.uVertices = vertices.size( );
	const UINT bufferSize = static_cast<UINT>(vertices.size( )) * sizeof(Vertex);

	CD3DX12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferResource = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProperty, D3D12_HEAP_FLAG_NONE, &bufferResource,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&object.pVertexBuffer))
	);

	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(object.pVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, vertices.data( ), bufferSize);
	object.pVertexBuffer->Unmap(0, nullptr);

	object.sVertexBufferView.BufferLocation = object.pVertexBuffer->GetGPUVirtualAddress( );
	object.sVertexBufferView.StrideInBytes = sizeof(Vertex);
	object.sVertexBufferView.SizeInBytes = bufferSize;
}

void D3D12HelloTriangle::CreateIB(VBObject& object, std::vector<UINT>& indices) {
	object.uIndices = indices.size( );
	const UINT indexBufferSize = static_cast<UINT>(indices.size( )) * sizeof(UINT);

	CD3DX12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferResource = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProperty, D3D12_HEAP_FLAG_NONE, &bufferResource, //
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&object.pIndexBuffer)));

	// Copy the triangle data to the index buffer.
	UINT8* pIndexDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(object.pIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, indices.data( ), indexBufferSize);
	object.pIndexBuffer->Unmap(0, nullptr);

	// Initialize the index buffer view.
	object.sIndexBufferView.BufferLocation = object.pIndexBuffer->GetGPUVirtualAddress( );
	object.sIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	object.sIndexBufferView.SizeInBytes = indexBufferSize;
}

void D3D12HelloTriangle::CreateMaterial(VBObject& object, Material material) {
	auto m_constantBuffer = nv_helpers_dx12::CreateBuffer(
		m_device.Get( ), sizeof(Material), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	uint8_t* pData;
	ThrowIfFailed(m_constantBuffer->Map(0, nullptr, (void**) &pData));
	memcpy(pData, &material, sizeof(Material));
	m_constantBuffer->Unmap(0, nullptr);

	object.constantBuffers.push_back(m_constantBuffer);
}

void D3D12HelloTriangle::CreateLightBuffer(Light light) {
	m_lights = nv_helpers_dx12::CreateBuffer(
		m_device.Get( ), sizeof(Material), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	uint8_t* pData;
	ThrowIfFailed(m_lights->Map(0, nullptr, (void**) &pData));
	memcpy(pData, &light, sizeof(Light));
	m_lights->Unmap(0, nullptr);
}

void D3D12HelloTriangle::OnKeyDown(UINT8 key) {
	auto forward = XMVector3Normalize(At - Eye);
	auto side = XMVector3Normalize(XMVector3Cross(forward, Up));

	m_framesFromMove.x = 0;

	switch (key) {
	case 0x57: //W
		Eye = XMVectorAdd(Eye, forward);
		At = XMVectorAdd(At, forward);
		break;
	case 0x53:
		Eye = XMVectorSubtract(Eye, forward);
		At = XMVectorSubtract(At, forward);
		break;
	case 0x44:
		Eye = XMVectorAdd(Eye, side);
		At = XMVectorAdd(At, side);
		break;
	case 0x41:
		Eye = XMVectorSubtract(Eye, side);
		At = XMVectorSubtract(At, side);
		break;

	case VK_LEFT:
	{
		auto matrix = XMMatrixRotationAxis(Up, XM_PIDIV4 / 2);
		auto neward = XMVector3Transform(forward, matrix);
		At = Eye + neward;
		break;
	}
	case VK_RIGHT:
	{
		auto matrix = XMMatrixRotationAxis(Up, -XM_PIDIV4 / 2);
		auto neward = XMVector3Transform(forward, matrix);
		At = Eye + neward;
		break;
	}
	case VK_UP:
	{
		auto matrix = XMMatrixRotationAxis(side, XM_PIDIV4 / 2);
		auto neward = XMVector3Transform(forward, matrix);
		At = Eye + neward;
		Up = XMVector3Normalize(XMVector3Transform(Up, matrix));
		break;
	}
	case VK_DOWN:
	{
		auto matrix = XMMatrixRotationAxis(side, -XM_PIDIV4 / 2);
		auto neward = XMVector3Transform(forward, matrix);
		At = Eye + neward;
		Up = XMVector3Normalize(XMVector3Transform(Up, matrix));
		break;
	}
	}
}

void D3D12HelloTriangle::OnButtonDown(UINT32 lparam) {

}

void D3D12HelloTriangle::OnMouseMove(UINT8 wparam, UINT32 lparam) {

}
