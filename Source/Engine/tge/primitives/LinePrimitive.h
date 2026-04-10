#pragma once

#include <tge/render/RenderObject.h>


namespace Tga
{
	class Engine;
	class GraphicsEngine;
	struct LinePrimitive
	{
		Vector4f color;
		Vector3f fromPosition;
		Vector3f toPosition;
	};

	struct LineMultiPrimitive
	{
		Color *colors;
		Vector3f *fromPositions;
		Vector3f *toPositions;
		unsigned int count;
	};
}