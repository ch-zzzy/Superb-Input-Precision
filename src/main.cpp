#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <chizz.subtick-inputs-api/include/SubtickInputs.hpp>

using namespace geode::prelude;
using namespace subtickinputs::prelude;

static bool s_modEnabled = true;

class $modify(PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		bool result = PlayLayer::init(level, useReplay, dontCreateObjects);
		if (!result) return false;

		if (s_modEnabled) {
			this->m_clickBetweenSteps = false;
			this->m_clickOnSteps = false;
		}

		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		if (s_modEnabled) {
			this->m_clickBetweenSteps = false;
			this->m_clickOnSteps = false;
		}
	}
};

class $modify(GJBaseGameLayer) {
	static void onModify(auto& self) {
		(void) self.setHookPriority(
			"GJBaseGameLayer::processQueuedButtons", Priority::VeryEarly);
	}

	void processQueuedButtons(float dt, bool clearInputQueue) {
		if (!s_modEnabled || useVanillaPhysics()) {
			GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
			return;
		}

		processInputs(this->m_player1, dt);
		if (this->m_gameState.m_isDualMode && this->m_player2) {
			processInputs(this->m_player2, dt);
		}

		this->m_queuedButtons.clear();
		GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
	}
};

$on_mod(Loaded) {
	auto mod = Mod::get();
	auto& config = Config::get();

	// Input Hz
	config.setInputHz(mod->getSettingValue<float>("input-hz"));
	listenForSettingChanges<float>(
		"input-hz", +[](float val) { Config::get().setInputHz(val); });

	// Instantaneous inputs
	config.setInstantInputsEnabled(
		mod->getSettingValue<bool>("instant-inputs"));
	listenForSettingChanges<bool>(
		"instant-inputs",
		+[](bool val) { Config::get().setInstantInputsEnabled(val); });

	// Velocity unrounding
	config.setVelocityUnroundingEnabled(
		mod->getSettingValue<bool>("velocity-unrounding"));
	listenForSettingChanges<bool>(
		"velocity-unrounding",
		+[](bool val) { Config::get().setVelocityUnroundingEnabled(val); });

	// CBF+ toggle
	s_modEnabled = !mod->getSettingValue<bool>("mod-disabled");
	listenForSettingChanges<bool>(
		"mod-disabled", +[](bool val) {
			s_modEnabled = !val;
			PlayLayer* playLayer = PlayLayer::get();
			// clang-format off
			if (playLayer) {
				if (val) {
					// copied from legacy cbf
					auto* gameManager = GameManager::sharedState();
					playLayer->m_clickBetweenSteps = gameManager->getGameVariable("0177");
					playLayer->m_clickOnSteps = gameManager->getGameVariable("0176");
				} else {
					playLayer->m_clickBetweenSteps = false;
					playLayer->m_clickOnSteps = false;
				}
			}
			// clang-format on
		});
}