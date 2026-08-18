#include <Geode/modify/PlayLayer.hpp>
#include <string>

#include "SubtickInputs.hpp"

using namespace subtickinputs;

template <typename T>
static void settingInit(T& settingVar, const std::string& settingKey, auto listenerCallback) {
	settingVar = Mod::get()->getSettingValue<T>(settingKey);
	listenForSettingChanges<T>(settingKey, listenerCallback);
}

$on_mod(Loaded) {
	settingInit(config::modEnabled, "mod-enabled", [](bool val) {
		config::modEnabled = val;

		auto* playLayer = PlayLayer::get();
		if (playLayer) {
			if (!config::modEnabled) {
				auto* gameManager = GameManager::get();
				playLayer->m_clickBetweenSteps = gameManager->getGameVariable("0177");
				playLayer->m_clickOnSteps = gameManager->getGameVariable("0176");
			} else {
				playLayer->m_clickBetweenSteps = false;

				/* 
					vanilla always expects all inputs to be processed on the first tick (aka step)
					of any given frame when cbs and cos are both disabled
					enabling cos alone causes the first tick to defer inputs to the second tick of the frame if needed
					and so on up until the final tick of the frame, at which point all remaining inputs are processed
					(honestly there is zero reason robtop should have even made cos a setting able to be disabled but 🤷‍♂️)
					took a while to figure out since i mainly tested on 240fps where there is only ever one tick per frame
				*/
				playLayer->m_clickOnSteps = true;
			}
		}
	});

	settingInit(config::inputHz, "input-hz", [](float val) { config::inputHz = val; });

	settingInit(config::instantInputsEnabled, "instant-inputs",
		[](bool val) { config::instantInputsEnabled = val; });

	settingInit(
		config::debugModeEnabled, "debug-mode", [](bool val) { config::debugModeEnabled = val; });

	// might remove this someday for compatiblity and just check if velocity unrounding mod is enabled
	settingInit(config::velocityUnroundingEnabled, "velocity-unrounding", [](bool val) {
		config::velocityUnroundingEnabled = val;
		toggleVelocityUnroundingPatches(val);
	});
}

class $modify(PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

		if (config::modEnabled) {
			this->m_clickBetweenSteps = false;
			this->m_clickOnSteps = true;
		}

		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		if (config::modEnabled) {
			this->m_clickBetweenSteps = false;
			this->m_clickOnSteps = true;
		}
	}
};