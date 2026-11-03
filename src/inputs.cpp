#define GEODE_DEFINE_EVENT_EXPORTS

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "../include/InputQueue.hpp"
#include "SIPlayerObject.hpp"
#include "SubtickInputs.hpp"

using namespace subtickinputs;
using namespace subtickinputs::fields;

static bool s_isLastTickOfFrame = true;

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
		GET_PLAYER_FIELD(static_cast<PlayerObject*>(this), m_updateJumpCalled) = true;
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

static float getRawDeltaVel(PlayerObject* player, float scaledDt) {
	if (player->m_isDart) return 0.0f;

	if (player->m_isRobot && player->m_maybeIsBoosted && player->m_jumpBuffered && !player->m_touchedPad &&
		player->m_accelerationOrSpeed < 1.5f) {
		return 0.0f;
	}

	bool jumpBuffered = player->m_jumpBuffered;
	bool fallingBugged = player->playerIsFallingBugged();
	bool isMini = player->m_vehicleSize != 1.0f;

	float rawBaseGravity = player->isInBasicMode() ? static_cast<float>(player->m_gravity) : 0.958199f;
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

	return player->flipMod() * gravPerTick;
}

static double getDeltaVel(PlayerObject* player, float scaledDt) {
	float dV = getRawDeltaVel(player, scaledDt);
	return vanillaRoundValue(player->m_yVelocity - dV) - player->m_yVelocity;
}

namespace {
	// everything processInputs tracks for one player over one tick
	struct PlayerTickState {
		PlayerObject* m_player;

		double m_adjustedYVel = 0.0;
		double m_finalVel = 0.0;
		double m_lastRatio = 0.0;

		// captured right before each input
		double m_preVel = 0.0;
		double m_preDv = 0.0;

		explicit PlayerTickState(PlayerObject* player) : m_player(player) {
			if (!m_player) return;

			// apply the velocity carried over from last tick before anything reads it
			double& yVelAdjustment = GET_PLAYER_FIELD(m_player, m_yVelAdjustment);
			m_player->m_yVelocity += yVelAdjustment;
			yVelAdjustment = 0.0;

			m_finalVel = m_player->m_yVelocity;
		}

		void advanceToInput(double ratio, float scaledDt) {
			m_preVel = m_player->m_yVelocity;
			m_preDv = getDeltaVel(m_player, scaledDt);

			m_finalVel -= getRawDeltaVel(m_player, scaledDt * (ratio - m_lastRatio));
			m_lastRatio = ratio;
		}

		void applyInputResult(double ratio, double rawRatio, float dt, float scaledDt, double inputSamplesPerTick) {
			// vanilla explicitly does updateJump(0) but only for certain gamemodes
			// the checks avoid double calling for those gamemodes
			if (!GET_PLAYER_FIELD(m_player, m_updateJumpCalled)) m_player->updateJump(0.0f);

			double postVel = m_player->m_yVelocity;
			double postDv = getDeltaVel(m_player, scaledDt);

			double preInputVel = m_preVel + m_preDv;
			double postInputVel = postVel + postDv;

			m_adjustedYVel += ratio * (preInputVel - postInputVel);

			if (postVel != m_preVel) m_finalVel = postVel;

			if (config::debugModeEnabled) {
				int playerNumber = m_player == PlayLayer::get()->m_player1 ? 1 : 2;
				log::debug(
					"p{} preVel: {}, postVel: {}, preDv: {}, postDv: {}, "
					"dt: {}, scaledDt: {}, rawRatio: {}, ratio: {}, inputSamplesPerTick: {}, "
					"adjustedYVel: {}, finalVel: {}",
					playerNumber, m_preVel, postVel, m_preDv, postDv, dt, scaledDt, rawRatio, ratio, inputSamplesPerTick,
					m_adjustedYVel, m_finalVel);
			}
		}

		void computeAdjustments(float scaledDt, double tps) {
			if (m_player->m_isDart) {
				// probably redundant since it's only dash orbs that can reach here
				double waveDt = 60.0 / tps * ((m_player->m_vehicleSize != 1.0f) ? 2.0 : 1.0);
				GET_PLAYER_FIELD(m_player, m_yDispAdjustment) = m_adjustedYVel * waveDt;
			} else {
				GET_PLAYER_FIELD(m_player, m_yDispAdjustment) = m_adjustedYVel * scaledDt;
			}

			m_finalVel -= getRawDeltaVel(m_player, scaledDt * (1.0 - m_lastRatio));
			double predictedVanillaVel = vanillaRoundValue(m_player->m_yVelocity - getRawDeltaVel(m_player, scaledDt));

			double& yVelAdjustment = GET_PLAYER_FIELD(m_player, m_yVelAdjustment);
			yVelAdjustment = vanillaRoundValue(m_finalVel) - predictedVanillaVel;

			bool logDebug = config::debugModeEnabled && yVelAdjustment != 0.0;
			if (logDebug) {
				log::debug("predicted vanilla velocity: {}", predictedVanillaVel);
				log::debug("subtick velocity: {}", vanillaRoundValue(m_finalVel));
				log::debug("m_yVelAdjustment: {}", yVelAdjustment);
			}
		}
	};
} // namespace

static void processInputs(float dt) {
	auto* playLayer = PlayLayer::get();
	if (!playLayer) return;

	if (dt <= 0.0f) return;

	auto& inputQueue = playLayer->m_queuedButtons;

	double tps = 1.0 / dt;
	double inputSamplesPerTick = config::inputHz / tps;
	float scaledDt = 60.0f * dt * 0.9f;

	PlayerObject* p1 = playLayer->m_player1;
	PlayerObject* p2 = playLayer->m_player2;
	if (p1 && !p1->isVanillaPlayer()) {
		p1 = nullptr;
	}
	if (p2 && (!playLayer->m_gameState.m_isDualMode || !p2->isVanillaPlayer())) {
		p2 = nullptr;
	}

	PlayerTickState playerStates[2] = {PlayerTickState(p1), PlayerTickState(p2)};

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

		double rawRatio = (currentTime - tickStartTime) / tickDuration;
		double ratio = std::clamp(rawRatio, 0.0, 1.0);

		if (!config::instantInputsEnabled) {
			if (inputSamplesPerTick > 1.0) {
				ratio = std::floor(ratio * inputSamplesPerTick) / inputSamplesPerTick;
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
			// this is processed in PlayerObject::update in wavetick.cpp
			continue;
		}

		for (auto& state : playerStates) {
			if (state.m_player) {
				state.advanceToInput(ratio, scaledDt);
				GET_PLAYER_FIELD(state.m_player, m_updateJumpCalled) = false;
			}
		}

		playLayer->handleButton(input.m_isPush, static_cast<int>(input.m_button), !input.m_isPlayer2);

		for (auto& state : playerStates) {
			if (state.m_player) state.applyInputResult(ratio, rawRatio, dt, scaledDt, inputSamplesPerTick);
		}

		// the handleButton + updateJump method to "dispatch" an input could be wrong
		// i'll let the players figure that one out 😛
	}

	for (auto& state : playerStates) {
		if (state.m_player) state.computeAdjustments(scaledDt, tps);
	}

	inputQueue.erase(inputQueue.begin(), inputQueue.begin() + processedCount);
}

namespace subtickinputs {
	Result<std::vector<PlayerButtonCommandWithRatio>> getInputQueueWithRatios(float dt) {
		auto* playLayer = PlayLayer::get();
		if (!playLayer) return Err("playlayer is null");

		double tps = 1.0 / dt;
		double inputSamplesPerTick = config::inputHz / tps;

		double tickDuration = dt;
		double tickStartTime = playLayer->m_timestamp;
		double tickEndTime = tickStartTime + tickDuration;

		std::vector<PlayerButtonCommandWithRatio> result;

		for (auto& input : playLayer->m_queuedButtons) {
			double currentTime = input.m_timestamp;

			double ratio = (currentTime - tickStartTime) / tickDuration;
			ratio = std::clamp(ratio, 0.0, 1.0);

			if (!config::instantInputsEnabled) {
				if (inputSamplesPerTick > 1.0) {
					ratio = std::floor(ratio * inputSamplesPerTick) / inputSamplesPerTick;
				} else {
					ratio = 0.0;
				}
			}

			result.push_back({input, ratio});
		}

		return Ok(std::move(result));
	}

	Result<void> setInputQueueWithRatios(std::vector<PlayerButtonCommandWithRatio> inputQueue, float dt) {
		auto* playLayer = PlayLayer::get();
		if (!playLayer) return Err("playlayer is null");

		// no way this works 😂
		playLayer->m_queuedButtons.clear();
		for (auto& inputWithRatio : inputQueue) {
			auto& inputWithoutRatio = inputWithRatio.input;
			inputWithoutRatio.m_timestamp = playLayer->m_timestamp + inputWithRatio.ratio * dt;
			playLayer->m_queuedButtons.push_back(inputWithoutRatio);
		}
		return Ok();
	}
} // namespace subtickinputs