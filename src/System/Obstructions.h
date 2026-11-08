#pragma once

// Hides what stands between the camera and an actor, and brings it back after. Only its drawing is switched off (the app cull flag), so
// nothing about it changes in the world or the save. One owner hides at a time, a camera session's client or a plugin through the API,
// and another starting to hide brings back what the first hid
namespace Obstructions
{
	// Each frame, after the camera's placed. Checks a few times a second. The subject, their horse, whoever's framed with them and anyone
	// they hold always stay. upperBody when they're framed from the chest up, so only what's in front of that counts
	void Update(const void* a_owner, float a_interval, RE::Actor* a_subject, RE::Actor* a_alsoFramed, bool a_upperBody);

	// Brings back what the owner hid. Safe to call any time
	void Restore(const void* a_owner);

	// The owner for a plugin hiding through the API, the same for each call with its name
	const void* OwnerOf(const char* a_plugin);

	// Nothing with collision that blocks sight lies between the two points, in around's world, besides those given
	bool IsLineClear(RE::TESObjectREFR* a_around, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, std::span<const RE::TESObjectREFR* const> a_ignore);
}
