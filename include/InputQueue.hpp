#pragma once

#include <Geode/loader/Dispatch.hpp>

#undef MY_MOD_ID

#define MY_MOD_ID "chizz.superb-input-precision"

namespace subtickinputs {

	struct PlayerButtonCommandWithRatio {
		PlayerButtonCommand input;
		double ratio;
	};

	inline geode::Result<void> setInputQueueWithRatios(
		std::vector<PlayerButtonCommandWithRatio> inputQueue, float dt)
		GEODE_EVENT_EXPORT(&setInputQueueWithRatios,
			(std::vector<PlayerButtonCommandWithRatio> inputQueue, float dt));

	inline geode::Result<std::vector<PlayerButtonCommandWithRatio>> getInputQueueWithRatios(
		float dt) GEODE_EVENT_EXPORT(&getInputQueueWithRatios, (float dt));
} // namespace subtickinputs