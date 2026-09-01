// NOLINTBEGIN
#include <algorithm>
#include <cmath>
#include <string>

#include "SIPlayerObject.hpp"
#include "SubtickInputs.hpp"

using namespace subtickinputs;
using namespace subtickinputs::fields;

static std::string getFrameForStreak(ShipStreak type, float time);

// modification at line: 155
void SIPlayerObject::processPlayerTick(float dt) {
	auto dtModified = dt * 0.9f;

	if (m_flashTime >= 0) {
		double elapsed = (m_totalTime - m_flashTime) - m_flashDelay;
		double duration = m_flashDuration;

		if (elapsed < duration) {
			float t = elapsed / duration;

			auto mainColor =
				GameToolbox::multipliedColorValue(m_flashMainColor, m_originalMainColor, t);
			auto secondColor =
				GameToolbox::multipliedColorValue(m_flashSecondColor, m_originalSecondColor, t);

			setColor(mainColor);
			setSecondColor(secondColor);
		} else {
			m_flashTime = -1;

			setColor(m_originalMainColor);
			setSecondColor(m_originalSecondColor);
		}
	}

	m_yVelocity = std::clamp(m_yVelocity, -1000.0, 1000.0);

	if (m_isPlatformer) {
		m_platformerXVelocity = std::clamp(m_platformerXVelocity, -1000.0, 1000.0);
	}

	if (m_isDead) return;

	auto pos = getPosition();

	float movedDist = m_lastPosition.getDistance(pos);
	m_lastPosition = pos;

	m_yVelocityRelated3 = 0.f;

	if (!m_isLocked) {
		updateJump(dtModified);

		if (m_stateForce > 0) {
			float modeForceScale = 1.f;

			if (m_isShip) {
				modeForceScale = 0.47f;
			} else if (m_isBird) {
				modeForceScale = 0.58f;
			} else if (m_isSwing) {
				modeForceScale = 0.4f;
			} else if (m_isBall || m_isSpider) {
				modeForceScale = 0.6f;
			} else if (m_isRobot) {
				modeForceScale = 0.9f;
			}

			if (m_vehicleSize != 1.f) {
				if (m_isShip) {
					modeForceScale = 0.5875f;
				} else if (m_isBird) {
					modeForceScale = 0.72499996f;
				} else if (m_isSwing) {
					modeForceScale = 0.61538464f;
				}
			}

			auto stateForceYDelta =
				static_cast<double>(dtModified * m_stateForceVector.y * modeForceScale);
			addToYVelocity(stateForceYDelta, 0);

			if (stateForceYDelta != 0) {
				m_isAccelerating = true;
			}

			bool shouldAffectAcceleration = (!m_isUpsideDown && stateForceYDelta > 0) ||
				(m_isUpsideDown && stateForceYDelta < 0);

			if (shouldAffectAcceleration) {
				m_accelerationOrSpeed = (stateForceYDelta / 12.94f) * 1.5 + m_accelerationOrSpeed;
			}

			if (m_isPlatformer) {
				auto stateForceX = m_stateForceVector.x;
				m_platformerXVelocity =
					static_cast<double>(stateForceX * dtModified) + m_platformerXVelocity;

				if (stateForceX != 0) {
					m_affectedByForces = true;
				}

				bool canAutoRotate =
					isInNormalMode() && !m_isRotating && std::abs(stateForceX) > 0.1;

				if (canAutoRotate) {
					runNormalRotation(true, 1);
				}
			}
		}

		bool isDashing = m_isDashing;
		if (isDashing) {
			m_yVelocity = 0.0;
		}

		double scaledDt = dtModified;
		double yOffset = scaledDt * m_yVelocity;

		if (m_isPlatformer) {
			updateMove(dtModified);
			isDashing = m_isDashing;
		}

		double xOffset = getCurrentXVelocity();
		xOffset = xOffset * dt;

		double reverseDecayStep = xOffset * 0.02;

		if (isDashing) {
			double dashY = m_dashY;

			if (!m_isPlatformer) {
				yOffset = dashY * xOffset;
			} else {
				double dashX = m_dashX;
				xOffset = scaledDt * dashX;
				yOffset = dashY * scaledDt;
				m_platformerXVelocity = dashX;
				m_yVelocity = dashY;
			}
		} else if (m_isDart) {
			int bufferSign = m_jumpBuffered ? 1 : -1;

			yOffset = std::abs(xOffset) * flipMod() * bufferSign;
			if (m_vehicleSize != 1) {
				yOffset = yOffset + yOffset;
			}
		}

		// m_yVelocityRelated3 = yOffset;

		double& yDispAdjustment = GET_PLAYER_FIELD(this, m_yDispAdjustment);
		bool logDebug = config::debugModeEnabled && yDispAdjustment != 0.0;

		if (logDebug) {
			log::debug("vanilla ypos displacement (double): {}", yOffset);
			log::debug("true vanilla ypos displacement (float): {}", static_cast<float>(yOffset));
			log::debug("m_yDispAdjustment: {}", yDispAdjustment);
		}

		m_yVelocityRelated3 = yOffset += yDispAdjustment;

		if (logDebug) {
			log::debug("total ypos displacement: {}", m_yVelocityRelated3);
		}

		yDispAdjustment = 0.0;

		//

		if (m_isGoingLeft && !m_isPlatformer) {
			xOffset = xOffset * -1;
		}

		xOffset = xOffset + m_maybeReverseAcceleration;

		double finalYOffset = yOffset;
		if (m_isSideways) {
			finalYOffset = xOffset;
			xOffset = yOffset;
		}

		auto frameOffset = CCPoint{static_cast<float>(xOffset), static_cast<float>(finalYOffset)};
		auto position = getPosition();
		auto offsetPosition = position + frameOffset;
		setPosition(offsetPosition);

		m_maybeReverseAcceleration = 0;
		double reverseSpeed = m_maybeReverseSpeed;

		if (reverseSpeed != 0) {
			double reverseStep = std::abs(reverseSpeed);

			if (reverseDecayStep <= std::abs(reverseSpeed)) {
				reverseStep = reverseDecayStep;
			}
			if (reverseSpeed <= 0) {
				reverseStep = -reverseStep;
			}

			m_maybeReverseAcceleration = reverseStep;
			m_maybeReverseSpeed = reverseSpeed - reverseStep;
		}
	}

	if (m_isShip) {
		if (!m_jumpBuffered || levelFlipping() || m_isHidden) {
			if (m_hasShipParticles) {
				m_shipClickParticles->stopSystem();
				m_hasShipParticles = false;
			}
		} else {
			if (!m_hasShipParticles) {
				m_shipClickParticles->resumeSystem();
				m_hasShipParticles = true;
			}
		}
	} else if (!m_isBird && !m_isSwing && !m_isDart) {
		if ((!m_isOnGround2 || levelFlipping() || m_isLocked || m_isHidden) ||
			(m_isPlatformer && std::abs(m_platformerXVelocity) <= 2.5)) {
			if (m_hasGroundParticles) {
				auto action = getActionByTag(3);
				if (!action) {
					auto call = CCCallFunc::create(
						this, callfunc_selector(PlayerObject::deactivateParticle));
					auto delay = CCDelayTime::create(0.06);
					action = CCSequence::create(delay, call, nullptr);
					runAction(action);
				}
			}
		} else {
			if (!m_hasGroundParticles) {
				m_playerGroundParticles->resumeSystem();
			}
			m_hasGroundParticles = true;
			stopActionByTag(3);
		}
	}

	bool active = !m_isLocked && !m_isHidden;
	bool moving = !m_isPlatformer || m_holdingLeft || m_holdingRight || m_platformerMovingLeft ||
		m_platformerMovingRight;

	if (!m_isDart && (isFlying()) && m_isOnGround2 && m_yVelocity * flipMod() > -1 && active &&
		moving) {
		m_vehicleGroundParticles->resumeSystem();
	} else {
		m_vehicleGroundParticles->stopSystem();
	}

	m_waveTrail->m_pulseSize = (m_audioScale - 0.1) * 2.1 + 0.4;

	if (m_playEffects) {
		if (!m_isGoingLeft) {
			auto gameManager = GameManager::get();
			m_waveTrail->clearBehindXPos(gameManager->m_playLayer->m_gameState.m_cameraPosition2.x);
		} else {
			auto winSize = CCDirector::get()->getWinSize();
			auto gameManager = GameManager::get();

			float cameraX = gameManager->m_playLayer->m_gameState.m_cameraPosition2.x;
			float cameraZoom = gameManager->m_playLayer->m_gameState.m_cameraZoom;
			float thresholdX = cameraX + (winSize.width / cameraZoom);

			m_waveTrail->clearAboveXPos(thresholdX);
		}
	}

	if (m_robotFire) {
		bool shouldDisable = !m_isRobot || !m_jumpBuffered || m_touchedPad || !m_maybeIsBoosted ||
			m_accelerationOrSpeed <= 0.27f || m_accelerationOrSpeed >= 1.5f;
		bool isVisible = m_robotFire->isVisible();

		if (shouldDisable) {
			if (isVisible && !m_robotFire->getActionByTag(9)) {
				m_robotFire->stopAllActions();

				auto scaleDown = CCScaleTo::create(0.15f, 0.05f, 0.1f);
				auto hide = CCHide::create();

				auto seq = CCSequence::create(scaleDown, hide, nullptr);
				seq->setTag(9);

				m_robotFire->runAction(seq);
				m_robotBurstParticles->stopSystem();
			}
		} else {
			if (!isVisible) {
				m_robotFire->stopAllActions();
				m_robotFire->update(0.1f);
				m_robotFire->setVisible(true);

				auto a1 = CCScaleTo::create(0.05f, 1.44f, 0.9f);
				auto a2 = CCScaleTo::create(0.05f, 0.72f, 1.35f);
				auto a3 = CCScaleTo::create(0.05f, 1.8f, 0.9f);
				auto a4 = CCScaleTo::create(0.05f, 0.9f, 1.35f);

				auto seq = CCSequence::create(a1, a2, a3, a4, nullptr);

				m_robotFire->runAction(seq);
				m_robotBurstParticles->resumeSystem();
			}
		}
	}

	bool shouldDisableBurst = !m_isRobot || !m_jumpBuffered || m_touchedPad || m_isHidden ||
		(!m_maybeIsBoosted && m_accelerationOrSpeed <= 0.1f) || (m_accelerationOrSpeed >= 1.5f);
	bool isRunning = m_robotBurstParticles->isActive();

	if (shouldDisableBurst) {
		if (isRunning) {
			m_robotBurstParticles->stopSystem();
		}
	} else {
		if (!isRunning) {
			m_robotBurstParticles->resumeSystem();
		}
	}

	m_maybeSpriteRelated = false;

	if (m_ghostTrail) {
		CCPoint ghostPos;

		if (m_isRobot) {
			auto head = m_robotSprite->m_headSprite;
			auto headPos = head->getPosition();
			float yOffset = headPos.y * 0.4f + 7.f;

			ghostPos = CCPoint{0.f, yOffset};
		} else if (m_isSpider) {
			auto head = m_spiderSprite->m_headSprite;
			auto headPos = head->getPosition();
			float yOffset = headPos.y * 0.4f + 2.f;

			ghostPos = CCPoint{0.f, yOffset};
		} else {
			ghostPos = CCPoint{0.f, 0.f};
		}

		m_ghostTrail->m_position = ghostPos;
	}

	if (m_isDashing) {
		updateDashAnimation();
	}

	if (m_isSwing) {
		float rotation = getRotation();

		float angle1 = (45.f - rotation) - 180.f;
		m_swingBurstParticles1->setAngle(angle1 + (m_isGoingLeft ? 90 : 0));

		float angle2 = (-rotation - 45.f) - 180.f;
		m_swingBurstParticles2->setAngle(angle2 + (m_isGoingLeft ? -90 : 0));
	}

	updateJumpVariables();
	updateStateVariables();

	if (m_shipStreak) {
		auto textureName = getFrameForStreak(m_shipStreakType, m_totalTime);
		auto texture = CCTextureCache::get()->addImage(textureName.c_str(), false);

		m_shipStreak->setTexture(texture);
	}

	if (m_isPlatformer && m_isDashing && m_dashRing && m_dashRing->m_maxDuration > 0 &&
		m_dashRing->m_maxDuration < m_totalTime - m_dashStartTime) {
		stopDashing();
		m_jumpBuffered = false;
	}
}

static std::string getFrameForStreak(ShipStreak type, float time) {
	float frameRate;
	int frameCount;

	switch (type) {
		case ShipStreak::ShipFire2:
			frameRate = 0.03125f;
			frameCount = 9;
			break;
		case ShipStreak::ShipFire3:
			frameRate = 0.03125f;
			frameCount = 10;
			break;
		case ShipStreak::ShipFire4:
			frameRate = 0.0416667f;
			frameCount = 6;
			break;
		case ShipStreak::ShipFire5:
			frameRate = 0.05f;
			frameCount = 16;
			break;
		case ShipStreak::ShipFire6:
			frameRate = 0.0416667f;
			frameCount = 5;
			break;
		default:
			frameRate = 0.03125f;
			frameCount = 1;
			break;
	}

	int frameIndex = (static_cast<int>(std::floor(time / frameRate)) % frameCount) + 1;

	std::string result = "shipfire";

	if (static_cast<int>(type) < 10) result += "0";

	result += std::to_string(static_cast<int>(type));
	result += "_";

	if (frameIndex < 10) {
		result += "00";
	} else if (frameIndex < 100) {
		result += "0";
	}

	result += std::to_string(frameIndex);
	result += ".png";

	return result;
}
// NOLINTEND