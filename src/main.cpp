#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <chizz.subtick-inputs-api/include/SubtickInputs.hpp>

using namespace geode::prelude;
using namespace subtickinputs::prelude;

static bool s_modEnabled = true;

class $modify(GJBaseGameLayer) {
	static void onModify(auto& self) {
		(void) self.setHookPriority(
			"GJBaseGameLayer::processQueuedButtons", Priority::Replace);
	}

	void processQueuedButtons(float dt, bool clearInputQueue) {
		if (s_modEnabled && !useVanilla()) {
			// im forseeing issues if any other mod actually uses the api since the queue gets cleared every call...
			// but that's a problem for future chizz
			processInputs(dt);
		}

		// practically a no-op if processInputs was called
		GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
	}
};

$on_mod(Loaded) {
	auto* mod = Mod::get();
	auto& config = Config::get();

	// Input Hz
	config.setInputHz(mod->getSettingValue<float>("input-hz"));

	listenForSettingChanges<float>(
		"input-hz", [](float val) { Config::get().setInputHz(val); });

	listenForInputHzChanges(
		[mod](float val) { mod->setSettingValue<float>("input-hz", val); });

	// Instantaneous inputs
	config.setInstantInputsEnabled(
		mod->getSettingValue<bool>("instant-inputs"));

	listenForSettingChanges<bool>("instant-inputs",
		[](bool val) { Config::get().setInstantInputsEnabled(val); });

	listenForInstantInputsChanges(
		[mod](bool val) { mod->setSettingValue<bool>("instant-inputs", val); });

	// Velocity unrounding
	config.setVelocityUnroundingEnabled(
		mod->getSettingValue<bool>("velocity-unrounding"));

	listenForSettingChanges<bool>("velocity-unrounding",
		[](bool val) { Config::get().setVelocityUnroundingEnabled(val); });

	listenForVelocityUnroundingChanges([mod](bool val) {
		mod->setSettingValue<bool>("velocity-unrounding", val);
	});

	// soft toggle
	s_modEnabled = !mod->getSettingValue<bool>("mod-disabled");

	listenForSettingChanges<bool>(
		"mod-disabled", +[](bool val) { s_modEnabled = !val; });
}