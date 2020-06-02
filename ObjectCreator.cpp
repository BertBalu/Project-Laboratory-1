#include "ObjectCreator.h"

Object ObjectCreator::CreateBox(XMFLOAT3 dimensions, XMUINT3 parts) {
	ClearObject( );

	XMFLOAT3 half = {dimensions.x / 2, dimensions.y / 2, dimensions.z / 2};

	CreatePlane({0, 0,  half.z}, {0,   0, 0}, {dimensions.x, dimensions.y}, {parts.x, parts.y}); // Szembe
	CreatePlane({0, 0, -half.z}, {0, 180, 0}, {dimensions.x, dimensions.y}, {parts.x, parts.y}); // Hátra

	CreatePlane({half.x, 0, 0}, {90,  0,  90}, {dimensions.y, dimensions.z}, {parts.y, parts.z}); // Jobb
	CreatePlane({-half.x, 0, 0}, {-90, 0,  90}, {dimensions.y, dimensions.z}, {parts.y, parts.z}); // Bal

	CreatePlane({0, -half.y, 0}, {90, 0, 0}, {dimensions.z, dimensions.x}, {parts.z, parts.x}); // Le
	CreatePlane({0,  half.y, 0}, {270, 0, 0}, {dimensions.z, dimensions.x}, {parts.z, parts.x}); // Fel

	return m_object;
}

Object ObjectCreator::CreateSphere(float radius) {
	ClearObject( );
	CreateBox({1,1,1}, {40,40,40});

	for (size_t i = 0; i < m_object.Vertices.size( ); i++) {
		auto vec = XMVector3Normalize(m_object.Vertices[i].Position);
		auto pos = vec * radius;

		m_object.Vertices[i].Position = pos;
		m_object.Vertices[i].Normal = vec;
	}

	return m_object;
}

Object ObjectCreator::CreatePlane(XMFLOAT3 center, XMFLOAT2 size, XMUINT2 parts) {
	ClearObject( );
	XMFLOAT3 topLeft = {center.x - size.x, center.y - size.y, center.z};
	CreatePlane(topLeft, {0, 0, 0}, size, parts);
	return m_object;
}

void ObjectCreator::CreatePlane(XMFLOAT3 topLeft, XMFLOAT3 rotation, XMFLOAT2 size, XMUINT2 parts) {
	XMMATRIX rotationMatrix = XMMatrixRotationX(XMConvertToRadians(rotation.x)) * XMMatrixRotationY(XMConvertToRadians(rotation.y)) * XMMatrixRotationZ(XMConvertToRadians(rotation.z));
	XMMATRIX translateMatrix = XMMatrixTranslation(topLeft.x, topLeft.y, topLeft.z);
	XMMATRIX scaleMatrix = XMMatrixScaling(size.x, size.y, 1);
	XMMATRIX M = scaleMatrix * rotationMatrix * translateMatrix;

	float colStep = 1.0f / parts.x;
	float rowStep = 1.0f / parts.y;

	int startPos = m_object.Vertices.size( );
	for (int col = 0; col <= parts.x; col++) {
		for (int row = 0; row <= parts.y; row++) {
			XMVECTOR vector = {-.5f + colStep * col, -.5f + rowStep * row, 0, 1};
			XMVECTOR normalVector = {0,0,1,1};

			vector = XMVector3Transform(vector, M);
			normalVector = XMVector3Transform(normalVector, rotationMatrix);

			m_object.Vertices.push_back({vector, normalVector});
		}
	}

	for (int col = 0; col < parts.x; col++) {
		for (int row = 0; row < parts.y; row++) {
			m_object.Indices.push_back(startPos + col * (parts.y + 1) + row);
			m_object.Indices.push_back(startPos + (col + 1) * (parts.y + 1) + row);
			m_object.Indices.push_back(startPos + col * (parts.y + 1) + row + 1);
			m_object.Indices.push_back(startPos + col * (parts.y + 1) + row + 1);
			m_object.Indices.push_back(startPos + (col + 1) * (parts.y + 1) + row);
			m_object.Indices.push_back(startPos + (col + 1) * (parts.y + 1) + row + 1);
		}
	}
}

void ObjectCreator::ClearObject( ) {
	m_object.Vertices.clear( );
	m_object.Indices.clear( );
}
