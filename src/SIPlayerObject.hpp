#pragma once

#include <Geode/modify/PlayerObject.hpp>

#include "SubtickInputs.hpp"

namespace subtickinputs::fields {
	struct PendingWaveInput {
		double m_ratio;
		bool m_isPush;
		int m_button;
	};
} // namespace subtickinputs::fields

class $modify(SIPlayerObject, PlayerObject) {
	struct Fields {
		double m_yDispAdjustment = 0.0;
		double m_yVelAdjustment = 0.0;
		std::vector<subtickinputs::fields::PendingWaveInput> m_pendingWaveInputs;
		bool m_didWaveSplit = false;
		float m_rotationDelta = 0.0f;
		CCPoint m_preTickPosition;
	};

	static void onModify(auto& self) {
		(void) self.setHookPriority("PlayerObject::update", Priority::Replace);
	}

	void update(float dt);
	void updateRotation(float dt);
	void setYVelocity(double velocity, int type);
	void processPlayerTick(float dt);
};

namespace subtickinputs::fields {

	// clang-format off
	#define GET_PLAYER_FIELD(playerObj, fieldName) \
	(static_cast<SIPlayerObject*>(playerObj)->m_fields->fieldName)
	// clang-format on

} // namespace subtickinputs::fields