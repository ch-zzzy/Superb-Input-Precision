#pragma once

#include <Geode/modify/PlayerObject.hpp>

#include "SubtickInputs.hpp"

class $modify(SIPlayerObject, PlayerObject) {
	struct Fields {
		bool m_updateJumpCalled = false;
		double m_yDispAdjustment = 0.0;
		double m_yVelAdjustment = 0.0;
		double m_predictedYVel = 0.0; // what m_yVelocity *should* be when m_yVelAdjustment gets applied
		bool m_didSplit = false;
		bool m_skipNextUpdate = false; // already moved during the other player's update this tick
		float m_remainingRotationDelta = 0.0f;
		CCPoint m_tickStartPosition;
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
