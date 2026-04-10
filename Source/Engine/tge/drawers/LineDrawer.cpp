#include "stdafx.h"
#include <tge/drawers/LineDrawer.h>
#include <tge/graphics/GraphicsEngine.h>
#include <tge/graphics/DX11.h>
#include <tge/render/RenderObject.h>
#include <tge/shaders/shader.h>
#include <tge/engine.h>
#include <tge/primitives/LinePrimitive.h>

using namespace Tga;
LineDrawer::LineDrawer(Engine& anEngine)
	: Shader(anEngine)
{}

LineDrawer::~LineDrawer() {}

bool LineDrawer::Init()
{
	Shader::Init();
	InitShaders();
	CreateBuffer();
	return true;
}

void LineDrawer::CreateBuffer()
{
	D3D11_BUFFER_DESC vertexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData;

	float theZ = 0.0f;
	myVertices[0].X = -0.5f;
	myVertices[0].Y = 0.5f;
	myVertices[0].Z = theZ;

	myVertices[1].X = -0.5f;
	myVertices[1].Y = -0.5f;
	myVertices[1].Z = theZ;

	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;       
	vertexBufferDesc.ByteWidth = sizeof(SimpleVertex) * 2000;        
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.pSysMem = myVertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;
	HRESULT hr = DX11::Device->CreateBuffer(&vertexBufferDesc, nullptr, myVertexBuffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		ERROR_PRINT("%s", "Buffer error");
		return;
	}

}

void Tga::LineDrawer::Draw(LineMultiPrimitive& aObject)
{
	assert(aObject.count <= 1000 && "A single multi Primivive can only draw 1000 lines"); //1000 is a magic number I dont know why it limits there. Maybe buffer size to graphics card.

	if (!myVertexBuffer)
	{
		return;
	}
	PrepareRender();
	DX11::Context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);

	D3D11_MAPPED_SUBRESOURCE mappedVertexResource;
	SimpleVertex* dataVertexPtr;
	HRESULT result = DX11::Context->Map(myVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVertexResource);
	if (FAILED(result))
	{
		INFO_PRINT("Error in rendering!");
		return;
	}

	GraphicsStateStack& graphicsStateStack = Tga::Engine::GetInstance()->GetGraphicsEngine().GetGraphicsStateStack();
	Matrix4x4f transform = graphicsStateStack.GetTransform();

	dataVertexPtr = (SimpleVertex*)mappedVertexResource.pData;
	int theCount = 0;
	for (unsigned int i = 0; i < aObject.count; i++)
	{
		Vector3f fromPos = Vector4f(aObject.fromPositions[i], 1.f) * transform;
		Vector3f toPos = Vector4f(aObject.toPositions[i], 1.f) * transform;

		dataVertexPtr[theCount].X = fromPos.x;
		dataVertexPtr[theCount].Y = fromPos.y;
		dataVertexPtr[theCount].Z = fromPos.z;

		dataVertexPtr[theCount + 1].X = toPos.x;
		dataVertexPtr[theCount + 1].Y = toPos.y;
		dataVertexPtr[theCount + 1].Z = toPos.z;

		dataVertexPtr[theCount].myColorA = aObject.colors[i].myA;
		dataVertexPtr[theCount].myColorR = aObject.colors[i].myR;
		dataVertexPtr[theCount].myColorG = aObject.colors[i].myG;
		dataVertexPtr[theCount].myColorB = aObject.colors[i].myB;

		dataVertexPtr[theCount+1].myColorA = aObject.colors[i].myA;
		dataVertexPtr[theCount+1].myColorR = aObject.colors[i].myR;
		dataVertexPtr[theCount+1].myColorG = aObject.colors[i].myG;
		dataVertexPtr[theCount+1].myColorB = aObject.colors[i].myB;
		theCount += 2;
	}

	
	DX11::Context->Unmap(myVertexBuffer.Get(), 0);


	unsigned int strides = sizeof(SimpleVertex);
	unsigned int offsets = 0;
	DX11::Context->IASetVertexBuffers(0, 1, myVertexBuffer.GetAddressOf(), &strides, &offsets);

	DX11::LogDrawCall();
	DX11::Context->Draw(aObject.count * 2, 0);
}

bool Tga::LineDrawer::InitShaders()
{
	CreateShaders(L"shaders/lineshader_VS.cso", L"shaders/lineshader_PS.cso");

	return true;
}

bool LineDrawer::CreateInputLayout(const std::string& aVS)
{
	// LAYOUT
	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];

	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "TEXCOORD";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;


	unsigned int numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	// Create the vertex input layout.
	HRESULT result = DX11::Device->CreateInputLayout(polygonLayout, numElements, aVS.data(), aVS.size(), myLayout.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		ERROR_PRINT("%s", "Layout error");
		return false;
	}
	return true;
}

void LineDrawer::SetShaderParameters(LinePrimitive& aObject)
{
	UpdateVertexes(aObject);

	unsigned int strides = sizeof(SimpleVertex);
	unsigned int offsets = 0;
	DX11::Context->IASetVertexBuffers(0, 1, myVertexBuffer.GetAddressOf(), &strides, &offsets);
}

void LineDrawer::UpdateVertexes(LinePrimitive& aObject)
{
	D3D11_MAPPED_SUBRESOURCE mappedVertexResource;
	SimpleVertex* dataVertexPtr;
	HRESULT result = DX11::Context->Map(myVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVertexResource);
	if(FAILED(result))
	{
		INFO_PRINT("Error in rendering!");
		return;
	}

	GraphicsStateStack& graphicsStateStack = Tga::Engine::GetInstance()->GetGraphicsEngine().GetGraphicsStateStack();
	Matrix4x4f transform = graphicsStateStack.GetTransform();

	Vector3f fromPos = Vector4f(aObject.fromPosition, 1.f) * transform;
	Vector3f toPos = Vector4f(aObject.toPosition, 1.f) * transform;

	dataVertexPtr = (SimpleVertex*)mappedVertexResource.pData;

	dataVertexPtr[0].X = fromPos.x;
	dataVertexPtr[0].Y = fromPos.y;
	dataVertexPtr[0].Z = fromPos.z;

	dataVertexPtr[1].X = toPos.x;
	dataVertexPtr[1].Y = toPos.y;
	dataVertexPtr[1].Z = toPos.z;

	dataVertexPtr[0].myColorA = aObject.color.w;
	dataVertexPtr[0].myColorR = aObject.color.x;
	dataVertexPtr[0].myColorG = aObject.color.y;
	dataVertexPtr[0].myColorB = aObject.color.z;

	dataVertexPtr[1].myColorA = aObject.color.w;
	dataVertexPtr[1].myColorR = aObject.color.x;
	dataVertexPtr[1].myColorG = aObject.color.y;
	dataVertexPtr[1].myColorB = aObject.color.z;

	DX11::Context->Unmap(myVertexBuffer.Get(), 0);
}


void LineDrawer::Draw(LinePrimitive& aObject)
{
	PrepareRender();
	DX11::Context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);

	SetShaderParameters(aObject);
	DX11::LogDrawCall();
	DX11::Context->Draw(2, 0);
}