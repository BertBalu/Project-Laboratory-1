#pragma once
#include "stdafx.h"
#include <vector>

using namespace DirectX;

struct Vertex {
	XMVECTOR Position;
	XMVECTOR Normal;
};

struct Object {
	std::vector<Vertex> Vertices;
	std::vector<UINT> Indices;
};

class ObjectCreator {
	Object m_object;

public:
	Object CreateBox(XMFLOAT3 dimensions, XMUINT3 parts = {20,20,20});
	Object CreateSphere(float radius);

	Object CreatePlane(XMFLOAT3 center, XMFLOAT2 size, XMUINT2 parts);

private:
	void CreatePlane(XMFLOAT3 topLeft, XMFLOAT3 rotation, XMFLOAT2 size, XMUINT2 parts = {20,20});
	void ClearObject( );
};

