#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace subtickinputs {

	bool useVanilla();
	void toggleVelocityUnroundingPatches(bool enable);

	namespace config {
		inline float inputHz = 240.0f;
		inline bool instantInputsEnabled = false;
		inline bool velocityUnroundingEnabled = false;
		inline bool modEnabled = false;
		inline bool debugModeEnabled = false;
	} // namespace config

} // namespace subtickinputs
