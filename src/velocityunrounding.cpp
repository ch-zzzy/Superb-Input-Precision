#include "SIPlayerObject.hpp"
#include "SubtickInputs.hpp"
#include "patches.hpp"

using namespace subtickinputs;

static PatchGroup s_velocityUnroundingNops;

// clang-format off
void subtickinputs::toggleVelocityUnroundingPatches(bool enable) {
	if (s_velocityUnroundingNops.appliedPatches.empty()) {
		s_velocityUnroundingNops.init({

			#ifdef GEODE_IS_WINDOWS
			{0x4EA5BC, 0x24, 0x90}, // updateJump
			{0x31B1CE, 0x32, 0x90}, // checkCollisions
			{0x4EBDF8, 0x38, 0x90}, // postCollision
			{0x4F146A, 0x3F, 0x90}, // collidedWithObjectInternal
			{0x4FE334, 0x26, 0x90}, // boostPlayer
			{0x501726, 0x32, 0x90}, // loadFromCheckpoint
			#endif

		});
	}

	s_velocityUnroundingNops.toggle(enable);
}
// clang-format on

void SIPlayerObject::setYVelocity(double velocity, int type) {
	if (config::velocityUnroundingEnabled) {
		this->m_yVelocity = velocity;
		return;
	}
	PlayerObject::setYVelocity(velocity, type);
}