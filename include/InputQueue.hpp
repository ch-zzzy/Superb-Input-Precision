#pragma once

#include <Geode/binding/PlayerButtonCommand.hpp>
#include <Geode/loader/Dispatch.hpp>
#include <vector>

#undef MY_MOD_ID

#define MY_MOD_ID "chizz.superb-input-precision"

namespace subtickinputs {

	struct PlayerButtonCommandWithRatio {
		PlayerButtonCommand input;
		double ratio;
	};

	GEODE_EVENT_EXPORT(geode::Result<void>, setInputQueueWithRatios,
		(std::vector<PlayerButtonCommandWithRatio> inputQueue, float dt), (std::move(inputQueue), dt));

	GEODE_EVENT_EXPORT(
		geode::Result<std::vector<PlayerButtonCommandWithRatio>>, getInputQueueWithRatios, (float dt), (dt));
} // namespace subtickinputs