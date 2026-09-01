#include <algorithm>
#include <limits>

#include "SIPlayerObject.hpp"
#include "SubtickInputs.hpp"

using namespace subtickinputs;
using namespace subtickinputs::fields;

// split like cbf for wave
// doesn't cause gravity issues since wave velocity is always constant
void SIPlayerObject::update(float dt) {
	if (useVanilla()) {
		GET_PLAYER_FIELD(this, m_didWaveSplit) = false;
		PlayerObject::update(dt);
		return;
	}

	auto& pendingWaveInputs = GET_PLAYER_FIELD(this, m_pendingWaveInputs);

	if (pendingWaveInputs.empty() || !this->m_isDart || this->m_isDashing) {
		pendingWaveInputs.clear();
		GET_PLAYER_FIELD(this, m_didWaveSplit) = false;
		processPlayerTick(dt);
		return;
	}

	PlayLayer* playLayer = PlayLayer::get();
	double lastRatio = 0.0;

	CCPoint preTickPosition = this->getPosition();
	bool firstLoop = true;
	bool startedOnGround = this->m_isOnGround;

	constexpr double EPSILON = std::numeric_limits<float>::min();

	for (auto& input : pendingWaveInputs) {
		double segment = std::clamp(input.m_ratio - lastRatio, EPSILON, 1.0);

		PlayerObject::update(segment * dt);

		if (config::debugModeEnabled) {
			log::debug("processing wave input: ratio={}, segment={}, isPush={}", input.m_ratio,
				segment, input.m_isPush);
		}

		if (firstLoop && ((this->m_yVelocity < 0) ^ this->m_isUpsideDown)) {
			this->m_isOnGround = startedOnGround;
		}

		playLayer->checkCollisions(this, 0.0f, true);
		PlayerObject::updateRotation(segment * dt);
		this->resetCollisionLog(false);

		playLayer->handleButton(input.m_isPush, input.m_button, this->isPlayer1());

		lastRatio = std::max(lastRatio, input.m_ratio);
		firstLoop = false;
	}
	pendingWaveInputs.clear();

	float remainingDelta = static_cast<float>((1.0 - lastRatio) * dt);
	PlayerObject::update(remainingDelta);

	GET_PLAYER_FIELD(this, m_rotationDelta) = remainingDelta;
	GET_PLAYER_FIELD(this, m_preTickPosition) = preTickPosition;
	GET_PLAYER_FIELD(this, m_didWaveSplit) = true;
}

void SIPlayerObject::updateRotation(float dt) {
	bool& didWaveSplit = GET_PLAYER_FIELD(this, m_didWaveSplit);

	if (!didWaveSplit) {
		PlayerObject::updateRotation(dt);
		return;
	}

	didWaveSplit = false;

	PlayerObject::updateRotation(GET_PLAYER_FIELD(this, m_rotationDelta));
	this->m_lastPosition = GET_PLAYER_FIELD(this, m_preTickPosition);
}