#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace subtickinputs {

	struct PendingSplitInput {
		PlayerButtonCommand m_command;
		double m_ratio;
	};

	inline std::vector<PendingSplitInput> s_pendingSplitInputs;

	inline bool isSplittableWave(PlayerObject* player) {
		return player && player->m_isDart && !player->m_isDashing;
	}

	bool useVanilla();
	void toggleVelocityUnroundingPatches(bool enable);

	namespace config {
		inline double inputHz = 240.0;
		inline bool instantInputsEnabled = false;
		inline bool velocityUnroundingEnabled = false;
		inline bool modEnabled = false;
		inline bool debugModeEnabled = false;

		void applyClickSettings(PlayLayer* playLayer);
	} // namespace config

} // namespace subtickinputs
