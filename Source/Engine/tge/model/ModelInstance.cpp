#include "stdafx.h"
#include <tge/model/ModelInstance.h>
#include <tge/model/Model.h>
#include <tge/shaders/ModelShader.h>

using namespace Tga;

void ModelInstance::Init(std::shared_ptr<Model> aModel)
{
	myModel = aModel;
}

std::shared_ptr<Model> ModelInstance::GetModel() const
{
	return myModel;
}

void ModelInstance::SetTransform(const Transform& someTransform)
{
	myTransform = someTransform;
}

void ModelInstance::SetRotation(Rotator someRotation)
{
	// Really should unroll rotations as well somewhere
	// so we can use -180 to 180 instead of 0 to 360.
	myTransform.SetRotation(someRotation);
}

void ModelInstance::SetLocation(Vector3f someLocation)
{
	myTransform.SetPosition(someLocation);
}

void ModelInstance::SetScale(Vector3f someScale)
{
	myTransform.SetScale(someScale);
}

void ModelInstance::Render(const ModelShader& shader) const
{
	const std::vector<Model::MeshData>& meshData = myModel->GetMeshDataList();

	for (int j = 0; j < meshData.size(); j++)
	{
		shader.Render(myTextures[j], meshData[j], myTransform.GetMatrix());
	}
}

void ModelInstance::Render(const ModelShader& shader, int aMeshIndex) const
{
	const std::vector<Model::MeshData>& meshData = myModel->GetMeshDataList();

	assert(aMeshIndex < meshData.size());
	if (aMeshIndex < meshData.size())
	{
		shader.Render(myTextures[aMeshIndex], meshData[aMeshIndex], myTransform.GetMatrix());
	}
}
