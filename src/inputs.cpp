#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>

#include "SIPlayerObject.hpp"
#include "SubtickInputs.hpp"

using namespace subtickinputs;
using namespace subtickinputs::fields;

static bool s_isLastTickOfFrame = true;

static bool s_updateJumpCalledP1 = false;
static bool s_updateJumpCalledP2 = false;

static void processInputs(float dt);

class $modify(GJBaseGameLayer) {
	static void onModify(auto& self) {
		(void) self.setHookPriority("GJBaseGameLayer::processQueuedButtons", Priority::Replace);
	}

	// the main thing
	void processQueuedButtons(float dt, bool clearInputQueue) {
		if (!useVanilla()) {
			processInputs(dt);
		}

		GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
	}

	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
		s_isLastTickOfFrame = isLastTick;
		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
	}
};

class $modify(PlayerObject) {
	void updateJump(float dt) {
		auto* playLayer = PlayLayer::get();
		if (playLayer) {
			if (this == playLayer->m_player1)
				s_updateJumpCalledP1 = true;
			else if (this == playLayer->m_player2)
				s_updateJumpCalledP2 = true;
		}
		PlayerObject::updateJump(dt);
	}
};

// made this to emulate the rounding from setYVelocity
static double vanillaRoundValue(double value) {
	if (config::velocityUnroundingEnabled) {
		return value;
	} else {
		int integerPart = static_cast<int>(value);
		return std::round((value - integerPart) * 1000.0) / 1000.0 + integerPart;
	}
}

/// @param scaledDt the dt param passed to updateJump(float dt)
static float getGravPerTick(PlayerObject* player, float scaledDt) {
	if (player->m_isDart) return 0.0f;

	if (player->m_isOnGround) return 0.0f;

	if (player->m_isRobot && player->m_maybeIsBoosted && player->m_jumpBuffered &&
		!player->m_touchedPad && player->m_accelerationOrSpeed < 1.5f) {
		return 0.0f;
	}

	bool jumpBuffered = player->m_jumpBuffered;
	bool fallingBugged = player->playerIsFallingBugged();
	bool isMini = player->m_vehicleSize != 1.0f;

	float rawBaseGravity =
		player->isInBasicMode() ? static_cast<float>(player->m_gravity) : 0.958199f;
	float baseGravity = rawBaseGravity * player->m_gravityMod;
	float sizeFactor = isMini ? (player->isFlying() ? 0.85f : 0.8f) : 1.0f;

	float gravPerTick;

	if (player->m_isShip) {
		bool isFalling = player->m_yVelocity * player->flipMod() < 0.0;
		bool forceNegative = player->m_isAccelerating ? isFalling : jumpBuffered;

		float shipYVelFactor = 0.8f;
		if (forceNegative) shipYVelFactor = -1.0f;
		if (!jumpBuffered && !fallingBugged) shipYVelFactor = 1.2f;

		if (player->m_isPlatformer) baseGravity *= 0.8f;

		float effectiveBaseGrav = shipYVelFactor >= 0.0f ? baseGravity : rawBaseGravity;
		float gravCoeff = (jumpBuffered && fallingBugged) ? 0.5f : 0.4f;

		gravPerTick = scaledDt * effectiveBaseGrav * shipYVelFactor * gravCoeff / sizeFactor;
	} else if (player->m_isBird) {
		float ufoYVelFactor = fallingBugged ? 0.8f : 1.2f;
		gravPerTick = scaledDt * baseGravity * ufoYVelFactor * 0.5f / sizeFactor;
	} else if (player->m_isSwing) {
		float swingSizeFactor = isMini ? 0.6f : 0.4f;
		gravPerTick = scaledDt * baseGravity * swingSizeFactor;
	} else {
		float gravCoeff = 1.0f;
		if (player->m_isBall || player->m_isSpider)
			gravCoeff = 0.6f;
		else if (player->m_isRobot)
			gravCoeff = 0.9f;

		gravPerTick = scaledDt * baseGravity * gravCoeff;
	}

	float dV = player->flipMod() * gravPerTick;
	return vanillaRoundValue(player->m_yVelocity - dV) - player->m_yVelocity;
}

static void processInputs(float dt) {
	auto* playLayer = PlayLayer::get();
	if (!playLayer) return;

	if (dt <= 0.0f) return;

	auto& inputQueue = playLayer->m_queuedButtons;

	double tps = 1.0 / dt;
	double inputChecksPerTick = config::inputHz / tps;
	float scaledDt = 60.0f * dt * 0.9f;

	PlayerObject* p1 = playLayer->m_player1;
	PlayerObject* p2 = playLayer->m_player2;
	if (p1 && !p1->isVanillaPlayer()) {
		p1 = nullptr;
	}
	if (p2 && (!playLayer->m_gameState.m_isDualMode || !p2->isVanillaPlayer())) {
		p2 = nullptr;
	}

	double adjustedYVel1 = 0.0;
	double adjustedYVel2 = 0.0;

	double tickDuration = dt;
	double tickStartTime = playLayer->m_timestamp;
	double tickEndTime = tickStartTime + tickDuration;

	bool processEntireQueue = s_isLastTickOfFrame;

	size_t processedCount = 0;

	for (auto& input : inputQueue) {
		if (!processEntireQueue && input.m_timestamp >= tickEndTime) {
			break;
		}
		processedCount++;

		PlayerObject* player = input.m_isPlayer2 ? p2 : p1;

		double currentTime = input.m_timestamp;

		double ratio = (currentTime - tickStartTime) / tickDuration;
		ratio = std::clamp(ratio, 0.0, 1.0);

		if (!config::instantInputsEnabled) {
			if (inputChecksPerTick > 1.0) {
				ratio = std::floor(ratio * inputChecksPerTick) / inputChecksPerTick;
			} else {
				ratio = 0.0;
			}
		}

		if (player && player->m_isDart && !player->m_isDashing) {
			GET_PLAYER_FIELD(player, m_pendingWaveInputs)
				.push_back({
					ratio,
					input.m_isPush,
					static_cast<int>(input.m_button),
				});
			// this is processed in PlayerObject::update in hooks.cpp
			continue;
		}

		double preVel1 = p1 ? p1->m_yVelocity : 0.0;
		float preDv1 = p1 ? getGravPerTick(p1, scaledDt) : 0.0;

		double preVel2 = p2 ? p2->m_yVelocity : 0.0;
		float preDv2 = p2 ? getGravPerTick(p2, scaledDt) : 0.0;

		s_updateJumpCalledP1 = false;
		s_updateJumpCalledP2 = false;

		playLayer->handleButton(
			input.m_isPush, static_cast<int>(input.m_button), !input.m_isPlayer2);

		if (p1) {
			// vanilla explicitly does updateJump(0) but only for certain gamemodes
			// the checks avoid double calling for those gamemodes
			if (!s_updateJumpCalledP1) p1->updateJump(0.0f);

			double postVel = p1->m_yVelocity;
			float postDv = getGravPerTick(p1, scaledDt);

			adjustedYVel1 += ratio * ((preVel1 - postVel) + (preDv1 - postDv));

			if (config::debugModeEnabled) {
				log::debug(
					"p1 preVel: {}, postVel: {}, preDv: {}, postDv: {}, dt: {}, scaledDt: {}, "
					"ratio: {}, "
					"adjustedYVel: {}",
					preVel1, postVel, preDv1, postDv, dt, scaledDt, ratio, adjustedYVel1);
			}
		}

		if (p2) {
			if (!s_updateJumpCalledP2) p2->updateJump(0.0f);

			double postVel = p2->m_yVelocity;
			float postDv = getGravPerTick(p2, scaledDt);

			adjustedYVel2 += ratio * ((preVel2 - postVel) + (preDv2 - postDv));

			if (config::debugModeEnabled) {
				log::debug(
					"p2 preVel: {}, postVel: {}, preDv: {}, postDv: {}, dt: {}, scaledDt: {}, "
					"ratio: {}, "
					"adjustedYVel: {}",
					preVel2, postVel, preDv2, postDv, dt, scaledDt, ratio, adjustedYVel2);
			}
		}
		// the handleButton + updateJump method to "dispatch" an input could be wrong
		// i'll let the players figure that one out 😛
	}

	for (PlayerObject* player : {p1, p2}) {
		if (!player) continue;
		auto& adjustedYVel = (player == p1) ? adjustedYVel1 : adjustedYVel2;

		if (player->m_isDart) {
			double waveDt = 60.0 / tps * ((player->m_vehicleSize != 1.0f) ? 2.0 : 1.0);
			GET_PLAYER_FIELD(player, m_yDispAdjustment) = adjustedYVel * waveDt;
		} else {
			GET_PLAYER_FIELD(player, m_yDispAdjustment) = adjustedYVel * scaledDt;
		}
	}

	inputQueue.erase(inputQueue.begin(), inputQueue.begin() + processedCount);
}