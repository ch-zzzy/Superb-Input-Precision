#include <algorithm>
#include <limits>
#include <vector>

#include "SIPlayerObject.hpp"
#include "SubtickInputs.hpp"

using namespace subtickinputs;
using namespace subtickinputs::fields;

// on ticks where any player is a wave, inputs are split like cbf instead of adjusted (decided in inputs.cpp)

// true while players are currently in a split tick,
// so the nested update/updateRotation calls go straight to vanilla with the fractional deltas
static bool s_isSplitting = false;

namespace {
	struct SplitPlayer {
		PlayerObject* m_player;
		cocos2d::CCPoint m_tickStartPosition;
		bool m_startedOnGround;
		float m_totalYStep = 0.0f; // sum of m_diagonalYStep over every segment
	};
} // namespace

static void splitTick(PlayLayer* playLayer, std::vector<SplitPlayer>& splitPlayers, float dt);

void SIPlayerObject::update(float dt) {
	if (s_isSplitting) {
		PlayerObject::update(dt); // actively splitting, segment dt, not a full tick
		return;
	}

	// true if player 2 already had its split from player 1's update this tick
	bool& alreadyUpdated = GET_PLAYER_FIELD(this, m_skipNextUpdate);
	if (alreadyUpdated) {
		alreadyUpdated = false;
		return;
	}

	if (useVanilla()) {
		GET_PLAYER_FIELD(this, m_didSplit) = false;
		PlayerObject::update(dt); // completely vanilla, full dt, full tick
		return;
	}

	PlayLayer* playLayer = PlayLayer::get();
	if (s_pendingSplitInputs.empty() || !playLayer) {
		GET_PLAYER_FIELD(this, m_didSplit) = false;
		processPlayerTick(dt); // full tick, with adjustments
		return;
	}

	std::vector<SplitPlayer> splitPlayers;
	splitPlayers.push_back({this, getPosition(), m_isOnGround});

	PlayerObject* player2 = playLayer->m_player2;
	bool isDual = playLayer->m_gameState.m_isDualMode;
	bool splitOtherPlayer = player2 && isPlayer1() && isDual;
	if (splitOtherPlayer) {
		splitPlayers.push_back({player2, player2->getPosition(), player2->m_isOnGround});
		GET_PLAYER_FIELD(player2, m_skipNextUpdate) = true;
	}

	splitTick(playLayer, splitPlayers, dt); // start the tick split
}

// both players are moved up to each input's moment together regardless of which player the input belongs to
// then handleButton sends the input to the appropriate player(s)
static void splitTick(PlayLayer* playLayer, std::vector<SplitPlayer>& splitPlayers, float dt) {
	constexpr double EPSILON = std::numeric_limits<float>::min();

	double lastRatio = 0.0;
	bool firstLoop = true;

	s_isSplitting = true;

	for (const auto& pending : s_pendingSplitInputs) {
		const auto& input = pending.m_command;
		double segment = std::clamp(pending.m_ratio - lastRatio, EPSILON, 1.0);
		float segmentDt = static_cast<float>(segment * dt);

		for (auto& splitPlayer : splitPlayers) {
			PlayerObject* player = splitPlayer.m_player;

			if (player->isPlayer2() && !playLayer->m_gameState.m_isDualMode) continue;

			player->update(segmentDt);
			splitPlayer.m_totalYStep += player->m_diagonalYStep;

			// copied from cbf ((player->m_yVelocity < 0) ^ player->m_isUpsideDown)
			if (firstLoop && player->playerIsFalling(0.0f)) {
				player->m_isOnGround = splitPlayer.m_startedOnGround;
			}

			float collisionDt = (player->m_isOnSlope && !player->m_isDart) ? dt : 0.0f; // also copied from cbf
			playLayer->checkCollisions(player, collisionDt, true);
			player->updateRotation(segmentDt);
			player->resetCollisionLog(false);
		}

		if (config::debugModeEnabled) {
			log::debug("processing split input: ratio={}, segment={}, isPush={}, isPlayer2={}, splitPlayers={}",
				pending.m_ratio, segment, input.m_isPush, input.m_isPlayer2, splitPlayers.size());
		}

		playLayer->handleButton(input.m_isPush, static_cast<int>(input.m_button), !input.m_isPlayer2);

		lastRatio = std::max(lastRatio, pending.m_ratio);
		firstLoop = false;
	}
	s_pendingSplitInputs.clear();

	float remainingDelta = static_cast<float>((1.0 - lastRatio) * dt);
	for (auto& splitPlayer : splitPlayers) {
		PlayerObject* player = splitPlayer.m_player;

		if (player->isPlayer2() && !playLayer->m_gameState.m_isDualMode) {
			GET_PLAYER_FIELD(player, m_skipNextUpdate) = false;
			continue;
		}

		player->update(remainingDelta);
		splitPlayer.m_totalYStep += player->m_diagonalYStep;

		GET_PLAYER_FIELD(player, m_remainingRotationDelta) = remainingDelta;
		GET_PLAYER_FIELD(player, m_tickStartPosition) = splitPlayer.m_tickStartPosition;

		// each update() call overwrote m_diagonalYStep with only its own segment's y movement
		// but gd expects it to be the whole tick's y movement
		player->m_diagonalYStep = splitPlayer.m_totalYStep;

		GET_PLAYER_FIELD(player, m_didSplit) = true;
	}

	s_isSplitting = false;
}

void SIPlayerObject::updateRotation(float dt) {
	if (s_isSplitting) {
		PlayerObject::updateRotation(dt);
		return;
	}

	bool& didSplit = GET_PLAYER_FIELD(this, m_didSplit);

	if (!didSplit) {
		PlayerObject::updateRotation(dt);
		return;
	}

	didSplit = false;

	PlayerObject::updateRotation(GET_PLAYER_FIELD(this, m_remainingRotationDelta));

	// gd reads (position - m_lastPosition) next tick as how far the player moved, so it has to cover the whole tick
	// it's restored here and not at the end of the split since checkCollisions uses the last segment's value first
	m_lastPosition = GET_PLAYER_FIELD(this, m_tickStartPosition);
}
