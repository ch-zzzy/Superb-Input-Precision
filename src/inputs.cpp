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
	// what vanilla used for this frame, captured when getModifiedDelta runs
	struct FrameInfo {
		double m_realStart; // m_timestamp, vanilla sets it to the previous frame's clock read
		double m_realEnd;	// clock read at the start of this frame
		double m_delta;		// what getModifiedDelta returned
		float m_timeWarp;
		float m_timeScale;	 // what cocos scaled this frame's dt by, timeWarp * game speed
		int m_tickIndex = 0; // ticks already run this frame (or this re-run), not since the level started
		std::optional<double> m_lastTickEnd; // where the last tick ended, carried into a timewarp re-run
	};

	// one tick's timing, for placing inputs within it and for the player physics
	struct TickInfo {
		double m_realDt;   // realtime tick length, what input hz is relative to
		float m_physicsDt; // gametime, what PlayerObject::update gets
		double m_start;	   // where the tick starts on the input timestamp clock
		double m_end;	   // where it ends on that clock

		double getDuration() const {
			return m_end - m_start;
		}

		double getRatioFromTimestamp(double timestamp) const {
			return (timestamp - m_start) / getDuration();
		}

		double getTimestampFromRatio(double ratio) const {
			return m_start + ratio * getDuration();
		}
	};

	// input timestamps are realtime, while m_timestamp is a mix of realtime and gametime,
	// set to the previous frame's clock read then advanced by each tick's gametime
	// which speedhack, timewarp, and the rounding to whole ticks stretch or shrink
	// so the frame's real span is captured before that and split across its ticks instead
	class FrameTiming {
		public:
		static void beginFrame() {
			// vanilla re-runs update for the leftover ticks when timewarp changes mid frame
			// by which point m_timestamp is already this frame's clock read
			// so the outer frame is kept to carry its leftover span
			m_outerFrame = m_inFrameUpdate ? m_frameInfo : std::nullopt;
			m_frameInfo.reset();
			m_inFrameUpdate = true;
		}

		static void endFrame() {
			m_inFrameUpdate = false;
			m_frameInfo.reset();
			m_outerFrame.reset();
		}

		static void setFrameInfo(FrameInfo frameInfo) {
			if (!m_inFrameUpdate) return;

			bool isRerun = m_outerFrame.has_value() && m_outerFrame->m_lastTickEnd.has_value();
			if (isRerun) {
				frameInfo.m_realStart = *m_outerFrame->m_lastTickEnd;
				frameInfo.m_realEnd = m_outerFrame->m_realEnd;
			}

			m_frameInfo = frameInfo;
		}

		static void advanceTick(float tickDt) {
			if (!m_frameInfo.has_value()) return;

			// remembered for a timewarp re-run, see beginFrame
			auto tickInfo = getTickInfoFromFrame(*m_frameInfo, tickDt);
			m_frameInfo->m_lastTickEnd = tickInfo.m_end;

			m_frameInfo->m_tickIndex++;
		}

		static TickInfo getTickInfo(PlayLayer* playLayer, float tickDt) {
			if (!m_frameInfo.has_value()) {
				return {
					.m_realDt = tickDt,
					.m_physicsDt = 60.0f * tickDt,
					.m_start = playLayer->m_timestamp,
					.m_end = playLayer->m_timestamp + tickDt,
				};
			}

			return getTickInfoFromFrame(*m_frameInfo, tickDt);
		}

		private:
		// only set once getModifiedDelta runs inside update, which vanilla skips while paused or not started
		static inline std::optional<FrameInfo> m_frameInfo;
		static inline std::optional<FrameInfo> m_outerFrame; // the frame a timewarp re-run continues from
		static inline bool m_inFrameUpdate = false;

		// tickDt is vanilla's frameDelta / timeWarp / tickCount cast to float
		// which loses precision, still includes game speed, and isn't what the player physics use under timewarp
		// so vanilla's exact calculations are used when the tick count is known
		static TickInfo getTickInfoFromFrame(const FrameInfo& frameInfo, float tickDt) {
			std::optional<int> tickCount = getFrameTickCount(frameInfo, tickDt);
			
			float timeScale = frameInfo.m_timeScale > 0.0f ? frameInfo.m_timeScale : frameInfo.m_timeWarp;

			double realDt;
			float physicsDt;
			if (tickCount.has_value()) {
				double frameDelta = frameInfo.m_delta;
				realDt = frameDelta / timeScale / *tickCount;
				physicsDt = static_cast<float>(frameDelta * 60.0 / *tickCount);
			} else {
				// approximation from dt alone, the physics dt can be off by a float rounding step
				realDt = tickDt * frameInfo.m_timeWarp / timeScale;
				physicsDt = static_cast<float>(tickDt * frameInfo.m_timeWarp * 60.0);
			}

			double frameSpan = frameInfo.m_realEnd - frameInfo.m_realStart;
			double tickDuration = (frameSpan > 0.0 && tickCount.has_value()) ? frameSpan / *tickCount : realDt;
			double tickStart = frameInfo.m_realStart + frameInfo.m_tickIndex * tickDuration;

			return {
				.m_realDt = realDt,
				.m_physicsDt = physicsDt,
				.m_start = tickStart,
				.m_end = tickStart + tickDuration,
			};
		}

		// vanilla doesn't store its tick count and tps bypasses change it
		// so it's worked out from the dt processCommands gets, which vanilla computes as delta / timeWarp / ticks
		// only trusted if it reproduces that dt exactly, otherwise the delta sip saw isn't the one vanilla used
		static std::optional<int> getFrameTickCount(const FrameInfo& frameInfo, float tickDt) {
			double realFrameDelta = frameInfo.m_delta / frameInfo.m_timeWarp;
			int tickCountThisFrame = std::max(1, static_cast<int>(std::round(realFrameDelta / tickDt)));

			bool tickCountMatches = static_cast<float>(realFrameDelta / tickCountThisFrame) == tickDt;

			// can't think of anything that would actually cause this to happen
			if (!tickCountMatches) return std::nullopt;

			return tickCountThisFrame;
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
		FrameTiming::beginFrame();
		GJBaseGameLayer::update(dt);
		FrameTiming::endFrame();
	}

	double getModifiedDelta(float dt) {
		double delta = GJBaseGameLayer::getModifiedDelta(dt);

		FrameTiming::setFrameInfo({
			.m_realStart = this->m_timestamp,
			.m_realEnd = utils::getInputTimestamp(),
			.m_delta = delta,
			.m_timeWarp = this->m_gameState.m_timeWarp,
			.m_timeScale = CCDirector::sharedDirector()->getScheduler()->getTimeScale(),
		});

		return delta;
	}

	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
		FrameTiming::advanceTick(dt);
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
	// recreation of the y movement from PlayerObject::update, updateJump and setYVelocity
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

		// the y velocity added by force blocks in PlayerObject::update
		static double getStateForceDelta(PlayerObject* player, float scaledDt) {
			if (player->m_stateForce <= 0) return 0.0;

			float modeForceScale = 1.0f;
			if (player->m_isShip) {
				modeForceScale = 0.47f;
			} else if (player->m_isBird) {
				modeForceScale = 0.58f;
			} else if (player->m_isSwing) {
				modeForceScale = 0.4f;
			} else if (player->m_isBall || player->m_isSpider) {
				modeForceScale = 0.6f;
			} else if (player->m_isRobot) {
				modeForceScale = 0.9f;
			}

			if (player->m_vehicleSize != 1.0f) {
				if (player->m_isShip) {
					modeForceScale = 0.5875f;
				} else if (player->m_isBird) {
					modeForceScale = 0.72499996f;
				} else if (player->m_isSwing) {
					modeForceScale = 0.61538464f;
				}
			}

			return static_cast<double>(scaledDt * player->m_stateForceVector.y * modeForceScale);
		}

		// the y displacement PlayerObject::update would apply this tick, given the player's current state
		// does NOT account for like collisions and stuff as those happen after PlayerObject::update
		static double getYDisplacement(PlayerObject* player, float scaledDt, float physicsDt) {
			if (player->m_isDead || player->m_isLocked) return 0.0;

			double xOffset = player->getCurrentXVelocity() * physicsDt;

			if (player->m_isDashing) {
				double dashStep = player->m_isPlatformer ? scaledDt : xOffset;
				return player->m_dashY * dashStep;
			}

			if (player->m_isDart) {
				int bufferMod = player->m_jumpBuffered ? 1 : -1;
				double yOffset = std::abs(xOffset) * player->flipMod() * bufferMod;
				return player->m_vehicleSize != 1.0f ? yOffset * 2.0 : yOffset;
			}

			double yVel = roundVelocity(player->m_yVelocity - getRawDeltaVel(player, scaledDt));
			yVel = roundVelocity(yVel + getStateForceDelta(player, scaledDt));
			return yVel * scaledDt;
		}

		// the position along the axis that update applies the y displacement to
		static float getMovementAxisPos(PlayerObject* player) {
			return player->m_isSideways ? player->getPositionX() : player->getPositionY();
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
		float m_physicsDt; // what PlayerObject::update receives
		float m_scaledDt;  // what PlayerObject::update passes to updateJump

		double m_adjustedYDisp = 0.0;
		double m_accurateVel = 0.0; // y velocity at m_lastRatio, as if inputs happened mid-tick
		double m_lastRatio = 0.0;

		// captured right before each input
		double m_preVel = 0.0;
		double m_preDisp = 0.0; // what a full tick of displacement would have been before the input
		float m_prePos = 0.0f;

		PlayerTickState(PlayerObject* player, const TickInfo& tickInfo)
			: m_player(player), m_physicsDt(tickInfo.m_physicsDt), m_scaledDt(tickInfo.m_physicsDt * 0.9f) {
			if (!m_player) return;

			// apply the velocity correction carried over from last tick before anything reads it
			// unless something else changed the velocity since the prediction was calculated
			double& yVelAdjustment = GET_PLAYER_FIELD(m_player, m_yVelAdjustment);
			if (yVelAdjustment != 0.0) {
				constexpr double TOLERANCE = 1e-6;
				double predictedYVel = GET_PLAYER_FIELD(m_player, m_predictedYVel);
				double actualYVel = m_player->m_yVelocity;

				bool matchesPrediction = std::abs(actualYVel - predictedYVel) < TOLERANCE;
				if (matchesPrediction) m_player->m_yVelocity += yVelAdjustment;

				if (config::debugModeEnabled) {
					log::debug("{} m_yVelAdjustment: {}, predicted velocity: {}, actual velocity: {}",
						matchesPrediction ? "applied" : "dropped", yVelAdjustment, predictedYVel, actualYVel);
				}

				yVelAdjustment = 0.0;
			}

			m_accurateVel = m_player->m_yVelocity;
		}

		void advanceToInput(double ratio) {
			m_preVel = m_player->m_yVelocity;
			m_preDisp = Physics::getYDisplacement(m_player, m_scaledDt, m_physicsDt);
			m_prePos = Physics::getMovementAxisPos(m_player);

			m_accurateVel -= Physics::getRawDeltaVel(m_player, m_scaledDt * (ratio - m_lastRatio));
			m_lastRatio = ratio;
		}

		void applyInputResult(double ratio, double rawRatio, double inputSamplesPerTick) {
			// vanilla explicitly does updateJump(0) but only for certain gamemodes
			// the checks avoid double calling for those gamemodes
			if (!GET_PLAYER_FIELD(m_player, m_updateJumpCalled)) m_player->updateJump(0.0f);

			double postVel = m_player->m_yVelocity;
			double postDisp = Physics::getYDisplacement(m_player, m_scaledDt, m_physicsDt);

			// a teleport overwrites the position, so the movement from before the input doesn't count
			bool teleported = Physics::getMovementAxisPos(m_player) != m_prePos;
			double preDisp = teleported ? 0.0 : m_preDisp;

			// both pre and postDisp represent displacement over a full tick, before and after applying the input
			m_adjustedYDisp += ratio * (preDisp - postDisp);

			if (postVel != m_preVel) m_accurateVel = postVel;

			if (config::debugModeEnabled) {
				int playerNumber = m_player->isPlayer1() ? 1 : 2;
				log::debug(
					"p{} preVel: {}, postVel: {}, preDisp: {}, postDisp: {}, teleported: {}, "
					"physicsDt: {}, scaledDt: {}, rawRatio: {}, ratio: {}, inputSamplesPerTick: {}, "
					"adjustedYDisp: {}, accurateVel: {}",
					playerNumber, m_preVel, postVel, m_preDisp, postDisp, teleported, m_physicsDt, m_scaledDt, rawRatio,
					ratio, inputSamplesPerTick, m_adjustedYDisp, m_accurateVel);
			}
		}

		void computeAdjustments() {
			GET_PLAYER_FIELD(m_player, m_yDispAdjustment) = m_adjustedYDisp;

			m_accurateVel -= Physics::getRawDeltaVel(m_player, m_scaledDt * (1.0 - m_lastRatio));
			double predictedVanillaVel =
				Physics::roundVelocity(m_player->m_yVelocity - Physics::getRawDeltaVel(m_player, m_scaledDt));

			double& yVelAdjustment = GET_PLAYER_FIELD(m_player, m_yVelAdjustment);
			yVelAdjustment = Physics::roundVelocity(m_accurateVel) - predictedVanillaVel;
			GET_PLAYER_FIELD(m_player, m_predictedYVel) = predictedVanillaVel;

			bool logDebug = config::debugModeEnabled && yVelAdjustment != 0.0;
			if (logDebug) {
				log::debug("predicted vanilla velocity: {}", predictedVanillaVel);
				log::debug("accurate velocity: {}", Physics::roundVelocity(m_accurateVel));
				log::debug("m_yVelAdjustment: {}", yVelAdjustment);
			}
		}
	};
} // namespace

static void processInputs(float dt, bool processEntireQueue) {
	auto* playLayer = PlayLayer::get();
	if (!playLayer) return;

	auto& inputQueue = playLayer->m_queuedButtons;

	auto tickInfo = FrameTiming::getTickInfo(playLayer, dt);
	double inputSamplesPerTick = config::inputHz * tickInfo.m_realDt; // realDt since input hz is realtime

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

	PlayerTickState playerStates[2] = {
		PlayerTickState(p1, tickInfo),
		PlayerTickState(p2, tickInfo),
	};

	size_t processedCount = 0;

	for (const auto& input : inputQueue) {
		double rawRatio = tickInfo.getRatioFromTimestamp(input.m_timestamp);

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
				state.advanceToInput(ratio);
				GET_PLAYER_FIELD(state.m_player, m_updateJumpCalled) = false;
			}
		}

		playLayer->handleButton(input.m_isPush, static_cast<int>(input.m_button), !input.m_isPlayer2);

		for (auto& state : playerStates) {
			if (state.m_player) state.applyInputResult(ratio, rawRatio, inputSamplesPerTick);
		}
	}

	for (auto& state : playerStates) {
		if (state.m_player) state.computeAdjustments();
	}

	inputQueue.erase(inputQueue.begin(), inputQueue.begin() + processedCount);
}

namespace subtickinputs::api {
	Result<std::vector<PlayerButtonCommandWithRatio>> getInputQueueWithRatios(float dt) {
		auto* playLayer = PlayLayer::get();
		if (!playLayer) return Err("playlayer is null");

		auto tickInfo = FrameTiming::getTickInfo(playLayer, dt);
		double inputSamplesPerTick = config::inputHz * tickInfo.m_realDt;

		std::vector<PlayerButtonCommandWithRatio> result;

		for (const auto& input : playLayer->m_queuedButtons) {
			double ratio = tickInfo.getRatioFromTimestamp(input.m_timestamp);

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

		auto tickInfo = FrameTiming::getTickInfo(playLayer, dt);

		// no way this works 😂
		playLayer->m_queuedButtons.clear();
		for (auto& inputWithRatio : inputQueue) {
			auto& inputWithoutRatio = inputWithRatio.input;
			inputWithoutRatio.m_timestamp = tickInfo.getTimestampFromRatio(inputWithRatio.ratio);
			playLayer->m_queuedButtons.push_back(inputWithoutRatio);
		}

		return Ok();
	}
} // namespace subtickinputs::api
