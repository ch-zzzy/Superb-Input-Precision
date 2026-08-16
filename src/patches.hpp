#pragma once

#include "SubtickInputs.hpp"
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <vector>

struct PatchGroup {
	std::vector<Patch*> appliedPatches;

	void init(std::initializer_list<std::tuple<ptrdiff_t, size_t, uint8_t>> entries) {
		for (auto& [offset, size, newValue] : entries) {
			ByteVector potentialPatch(size, newValue);

			auto result =
				Mod::get()->patch(reinterpret_cast<void*>(base::get() + offset), potentialPatch);

			if (result.isOk()) {
				auto* patch = result.unwrap();
				patch->setAutoEnable(false);
				(void) patch->disable();
				appliedPatches.push_back(patch);
			} else {
				log::error(
					"Failed to create patch at offset 0x{:X}: {}", offset, result.unwrapErr());
			}
		}
	}

	void toggle(bool enable) {
		for (auto* patch : appliedPatches) {
			(void) patch->toggle(enable);
		}
	}
};