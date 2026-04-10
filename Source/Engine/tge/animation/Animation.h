#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <tge/math/Transform.h>

namespace Tga
{
	struct Animation
	{
		struct Frame
		{
			std::unordered_map<std::string, Transform> LocalTransforms;
		};

		std::vector<Frame> Frames;

		// The animation length in frames.
		unsigned int Length;

		// The number of framer per second
		float FramesPerSecond;

		// The animation duration in seconds.
		float Duration;

		std::wstring Name;
	};
}

