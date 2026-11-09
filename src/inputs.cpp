#define GEODE_DEFINE_EVENT_EXPORTS

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include "../include/InputQueue.hpp"
#include "SIPlayerObject.hpp"
#include "SubtickInputs.hpp"

using namespace subtickinputs;
using namespace subtickinputs::fields;

namespace {
	struct TickDts {
		double m_real;	 // realtime, what processCommands gets
		float m_physics; // gametime, what PlayerObject::update gets
	};

	// where one tick is on realtime clock
	struct TickWindow {
		double m_start;
		double m_duration;

		double getRatioFromTimestamp(double timestamp) const {
			return (timestamp - m_start) / m_duration;
		}

		double getTimestampFromRatio(double ratio) const {
			return m_start + ratio * m_duration;
		}
	};

	// input timestamps are realtime, while m_timestamp only advances by gametime
	// which speedhack, timewarp and the rounding to whole ticks (leftover carried to the next frame) stretch or shrink
	struct FrameTiming {
		// m_timestamp on entry, vanilla sets it to the previous frame's clock read
		static inline double m_realStart = 0.0;
		static inline double m_realEnd = 0.0; // clock read at the start of this frame
		static inline int m_tickCount = 1;
		static inline int m_tickIndex = 0;

		// only set once getModifiedDelta runs, which vanilla skips while paused or not started
		static inline std::optional<TickDts> m_tickDts;

		// the dt processQueuedButtons gets is vanilla's real tick length cast to float, which loses precision
		// and isn't what the player physics use under time warp, so use the exact values from getModifiedDelta instead
		static TickDts getTickDts(float dt) {
			return m_tickDts.value_or(TickDts{dt, 60.0f * dt});
		}

		static TickWindow getTickWindow(GJBaseGameLayer* gameLayer, float dt) {
			bool hasRealSpan = m_tickDts && m_realEnd > m_realStart;
			if (!hasRealSpan) {
				return {gameLayer->m_timestamp, dt};
			}

			double tickDuration = (m_realEnd - m_realStart) / m_tickCount;
			double tickStartTime = m_realStart + m_tickIndex * tickDuration;
			return {tickStartTime, tickDuration};
		}
	};
} // namespace

static void processInputs(float dt, bool processEntireQueue);

class $modify(GJBaseGameLayer) {
	static void onModify(auto& self) {
		(void) self.setHookPriority("GJBaseGameLayer::processQueuedButtons", Priority::Replace);
	}

	// the main thing
	void processQueuedButtons(float dt, bool clearInputQueue) {
		if (useVanilla() || dt <= 0.0f) {
			GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
			return;
		}

		// the original isn't called after this, since it
		// could take inputs sip deferred to a later tick
		// also clearInputQueue (with COS enabled, as is the case) is true only on the last tick of the frame
		processInputs(dt, clearInputQueue);
	}

	void update(float dt) {
		FrameTiming::m_realStart = this->m_timestamp;
		FrameTiming::m_realEnd = utils::getInputTimestamp();
		FrameTiming::m_tickIndex = 0;
		FrameTiming::m_tickDts.reset();

		GJBaseGameLayer::update(dt);

		FrameTiming::m_tickDts.reset();
	}

	double getModifiedDelta(float dt) {
		double delta = GJBaseGameLayer::getModifiedDelta(dt);

		// same calculations vanilla does in update
		float timeWarp = this->m_gameState.m_timeWarp;
		float tickCount = std::round(static_cast<float>(delta * 60.0 / std::min(timeWarp, 1.0f) * 4.0));
		int ticks = std::max(1, static_cast<int>(tickCount));

		// applyTimeWarp also scales by 2.209 practice game speed setting while playing,
		// which vanilla's delta / m_timeWarp doesn't remove
		float gameSpeed = (std::abs(this->m_unk3380 - 1.0f) >= 0.01f && !this->m_unk7be) ? this->m_unk3380 : 1.0f;

		FrameTiming::m_tickCount = ticks;
		FrameTiming::m_tickDts =
			TickDts{delta / timeWarp / gameSpeed / ticks, static_cast<float>(delta * 60.0 / ticks)};

		return delta;
	}

	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
		FrameTiming::m_tickIndex++;
	}
};

class $modify(PlayLayer) {
	bool init(GJGameLevel* level, bool dontCreateObjects) {
		if (!PlayLayer::init(level, dontCreateObjects)) return false;

		if (config::modEnabled) config::applyClickSettings(this);
		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		if (config::modEnabled) config::applyClickSettings(this);

		s_pendingSplitInputs.clear();

		for (PlayerObject* player : {this->m_player1, this->m_player2}) {
			if (!player) continue;

			GET_PLAYER_FIELD(player, m_yVelAdjustment) = 0.0;
			GET_PLAYER_FIELD(player, m_yDispAdjustment) = 0.0;
			GET_PLAYER_FIELD(player, m_skipNextUpdate) = false;
			GET_PLAYER_FIELD(player, m_didSplit) = false;
		}
	}
};

class $modify(PlayerObject) {
	void updateJump(float dt) {
		GET_PLAYER_FIELD(static_cast<PlayerObject*>(this), m_updateJumpCalled) = true;
		PlayerObject::updateJump(dt);
	}
};

namespace {
	// recreation of the y velocity changes from PlayerObject::updateJump and setYVelocity
	struct Physics {
		// emulates the rounding from setYVelocity
		static double roundVelocity(double value) {
			if (config::velocityUnroundingEnabled) {
				return value;
			} else {
				int integerPart = static_cast<int>(value);
				return std::round((value - integerPart) * 1000.0) / 1000.0 + integerPart;
			}
		}

		// the gravity from PlayerObject::updateJump
		static float getRawDeltaVel(PlayerObject* player, float scaledDt) {
			if (player->m_isDart) return 0.0f;

			if (player->m_isRobot && player->m_isRising && player->m_jumpBuffered && !player->m_touchedPad &&
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
				bool isFalling = player->playerIsFalling(0.0f);
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
			return roundVelocity(player->m_yVelocity - dV) - player->m_yVelocity;
		}
	};
} // namespace

static double quantizeRatio(double ratio, double inputSamplesPerTick) {
	if (config::instantInputsEnabled) return ratio;
	if (inputSamplesPerTick <= 1.0) return 0.0;
	return std::floor(ratio * inputSamplesPerTick) / inputSamplesPerTick;
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
			m_preDv = Physics::getDeltaVel(m_player, scaledDt);

			m_finalVel -= Physics::getRawDeltaVel(m_player, scaledDt * (ratio - m_lastRatio));
			m_lastRatio = ratio;
		}

		void applyInputResult(double ratio, double rawRatio, float dt, float scaledDt, double inputSamplesPerTick) {
			// vanilla explicitly does updateJump(0) but only for certain gamemodes
			// the checks avoid double calling for those gamemodes
			if (!GET_PLAYER_FIELD(m_player, m_updateJumpCalled)) m_player->updateJump(0.0f);

			double postVel = m_player->m_yVelocity;
			double postDv = Physics::getDeltaVel(m_player, scaledDt);

			double preInputVel = m_preVel + m_preDv;
			double postInputVel = postVel + postDv;

			m_adjustedYVel += ratio * (preInputVel - postInputVel);

			if (postVel != m_preVel) m_finalVel = postVel;

			if (config::debugModeEnabled) {
				int playerNumber = m_player->isPlayer1() ? 1 : 2;
				log::debug(
					"p{} preVel: {}, postVel: {}, preDv: {}, postDv: {}, "
					"dt: {}, scaledDt: {}, rawRatio: {}, ratio: {}, inputSamplesPerTick: {}, "
					"adjustedYVel: {}, finalVel: {}",
					playerNumber, m_preVel, postVel, m_preDv, postDv, dt, scaledDt, rawRatio, ratio,
					inputSamplesPerTick, m_adjustedYVel, m_finalVel);
			}
		}

		void computeAdjustments(float scaledDt, float physicsTickDt) {
			if (m_player->m_isDart) {
				// probably redundant since it's only dash orbs that can reach here
				double waveDt = physicsTickDt * ((m_player->m_vehicleSize != 1.0f) ? 2.0 : 1.0);
				GET_PLAYER_FIELD(m_player, m_yDispAdjustment) = m_adjustedYVel * waveDt;
			} else {
				GET_PLAYER_FIELD(m_player, m_yDispAdjustment) = m_adjustedYVel * scaledDt;
			}

			m_finalVel -= Physics::getRawDeltaVel(m_player, scaledDt * (1.0 - m_lastRatio));
			double predictedVanillaVel =
				Physics::roundVelocity(m_player->m_yVelocity - Physics::getRawDeltaVel(m_player, scaledDt));

			double& yVelAdjustment = GET_PLAYER_FIELD(m_player, m_yVelAdjustment);
			yVelAdjustment = Physics::roundVelocity(m_finalVel) - predictedVanillaVel;

			bool logDebug = config::debugModeEnabled && yVelAdjustment != 0.0;
			if (logDebug) {
				log::debug("predicted vanilla velocity: {}", predictedVanillaVel);
				log::debug("subtick velocity: {}", Physics::roundVelocity(m_finalVel));
				log::debug("m_yVelAdjustment: {}", yVelAdjustment);
			}
		}
	};
} // namespace

static void processInputs(float dt, bool processEntireQueue) {
	auto* playLayer = PlayLayer::get();
	if (!playLayer) return;

	auto& inputQueue = playLayer->m_queuedButtons;

	auto tickDts = FrameTiming::getTickDts(dt);
	double inputSamplesPerTick = config::inputHz * tickDts.m_real; // realtime since input hz is real time
	float scaledDt = tickDts.m_physics * 0.9f;					   // what PlayerObject::update passes to updateJump
	auto tickWindow = FrameTiming::getTickWindow(playLayer, dt);

	bool isDual = playLayer->m_gameState.m_isDualMode;

	// if at least one player is a wave, every input this tick is split (see split_tick.cpp),
	// otherwise every input goes through the velocity/displacement adjustments below
	bool splitThisTick = isSplittableWave(playLayer->m_player1) || (isDual && isSplittableWave(playLayer->m_player2));

	auto* p1 = playLayer->m_player1;
	auto* p2 = playLayer->m_player2;
	if (p1 && !p1->isVanillaPlayer()) {
		p1 = nullptr;
	}
	if (p2 && (!isDual || !p2->isVanillaPlayer())) {
		p2 = nullptr;
	}

	PlayerTickState playerStates[2] = {PlayerTickState(p1), PlayerTickState(p2)};

	size_t processedCount = 0;

	for (auto& input : inputQueue) {
		double rawRatio = tickWindow.getRatioFromTimestamp(input.m_timestamp);

		// past the end of this tick, so it belongs to a later one
		if (!processEntireQueue && rawRatio >= 1.0) break;

		processedCount++;

		double ratio = quantizeRatio(std::clamp(rawRatio, 0.0, 1.0), inputSamplesPerTick);

		if (splitThisTick && ratio > 0.0) {
			// this is processed in SIPlayerObject::update in split_tick.cpp
			s_pendingSplitInputs.push_back({input, ratio});
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
	}

	for (auto& state : playerStates) {
		if (state.m_player) state.computeAdjustments(scaledDt, tickDts.m_physics);
	}

	inputQueue.erase(inputQueue.begin(), inputQueue.begin() + processedCount);
}

namespace subtickinputs::api {
	Result<std::vector<PlayerButtonCommandWithRatio>> getInputQueueWithRatios(float dt) {
		auto* playLayer = PlayLayer::get();
		if (!playLayer) return Err("playlayer is null");

		double inputSamplesPerTick = config::inputHz * FrameTiming::getTickDts(dt).m_real;

		auto tickWindow = FrameTiming::getTickWindow(playLayer, dt);

		std::vector<PlayerButtonCommandWithRatio> result;

		for (auto& input : playLayer->m_queuedButtons) {
			double ratio = tickWindow.getRatioFromTimestamp(input.m_timestamp);

			// only inputs inside this tick get quantized
			bool isInThisTick = ratio >= 0.0 && ratio < 1.0;
			if (isInThisTick) ratio = quantizeRatio(ratio, inputSamplesPerTick);

			result.push_back({input, ratio});
		}

		return Ok(std::move(result));
	}

	Result<void> setInputQueueWithRatios(std::vector<PlayerButtonCommandWithRatio> inputQueue, float dt) {
		auto* playLayer = PlayLayer::get();
		if (!playLayer) return Err("playlayer is null");

		auto tickWindow = FrameTiming::getTickWindow(playLayer, dt);

		// no way this works 😂
		playLayer->m_queuedButtons.clear();
		for (auto& inputWithRatio : inputQueue) {
			auto& inputWithoutRatio = inputWithRatio.input;
			inputWithoutRatio.m_timestamp = tickWindow.getTimestampFromRatio(inputWithRatio.ratio);
			playLayer->m_queuedButtons.push_back(inputWithoutRatio);
		}

		return Ok();
	}
} // namespace subtickinputs::api
