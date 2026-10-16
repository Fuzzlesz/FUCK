#include "System/Freeze.h"

namespace Freeze
{
	void Manager::SetFrozen(const char* a_pluginName, RE::Actor* a_actor, bool a_frozen)
	{
		if (!a_actor)
			return;

		const std::string plugin = a_pluginName ? a_pluginName : "";
		const auto        handle = a_actor->GetHandle();
		const auto        id     = handle.native_handle();

		if (a_frozen) {
			auto& hold = _frozen[id];
			if (hold.plugins.empty()) {
				hold.actor = handle;
				Apply(a_actor, true);
			}
			hold.plugins.insert(plugin);
			return;
		}

		const auto it = _frozen.find(id);
		if (it == _frozen.end() || !it->second.plugins.erase(plugin) || !it->second.plugins.empty())
			return;

		_frozen.erase(it);
		Apply(a_actor, false);
	}

	bool Manager::IsFrozen(RE::Actor* a_actor) const
	{
		return a_actor && _frozen.contains(a_actor->GetHandle().native_handle());
	}

	void Manager::OnFrame()
	{
		// Actors unloaded since are dropped, as there's nothing left to hold
		std::erase_if(_frozen, [](const auto& a_entry) {
			const auto actor = a_entry.second.actor.get();
			if (!actor)
				return true;

			HoldEyes(actor.get());
			return false;
		});
	}

	void Manager::ReleaseAll()
	{
		for (const auto& [id, hold] : _frozen) {
			if (const auto actor = hold.actor.get()) {
				Apply(actor.get(), false);
			}
		}
		_frozen.clear();
	}

	// Stops the animation graph and AI and keeps the actor from being pushed, which holds them whether or not the game is paused
	void Manager::Apply(RE::Actor* a_actor, bool a_frozen)
	{
		auto& runtime = a_actor->GetActorRuntimeData();

		if (a_frozen) {
			if (const auto process = runtime.currentProcess) {
				process->ClearMuzzleFlashes();
			}

			runtime.boolFlags.reset(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

			if (const auto controller = a_actor->GetCharController()) {
				controller->flags.set(RE::CHARACTER_FLAGS::kNotPushable);
				controller->flags.reset(RE::CHARACTER_FLAGS::kRecordHits);
				controller->flags.reset(RE::CHARACTER_FLAGS::kHitFlags);
			}

			a_actor->EnableAI(false);
			a_actor->StopMoving(1.0f);
			HoldEyes(a_actor);
			return;
		}

		runtime.boolFlags.set(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

		if (const auto controller = a_actor->GetCharController()) {
			controller->flags.reset(RE::CHARACTER_FLAGS::kNotPushable);
			controller->flags.set(RE::CHARACTER_FLAGS::kRecordHits);
			controller->flags.set(RE::CHARACTER_FLAGS::kHitFlags);
		}

		a_actor->EnableAI(true);

		// Zero timers so blinking and glancing pick up again straight away
		if (const auto animData = a_actor->GetFaceGenAnimationData()) {
			animData->eyesOffsetTimer   = 0.0f;
			animData->eyesBlinkingTimer = 0.0f;
		}
	}

	// Eyes open and looking ahead, with the blink and glance timers held off
	void Manager::HoldEyes(RE::Actor* a_actor)
	{
		const auto animData = a_actor->GetFaceGenAnimationData();
		if (!animData)
			return;

		animData->eyesHeadingOffset = 0.0f;
		animData->eyesPitchOffset   = 0.0f;
		animData->eyesOffsetTimer   = FLT_MAX;
		animData->eyesBlinkingTimer = FLT_MAX;
		animData->eyesBlinkingStage = RE::BSFaceGenAnimationData::EyesBlinkingStage::BlinkDelay;

		animData->modifierKeyFrame.values[RE::BSFaceGenKeyframeMultiple::Modifier::BlinkLeft]  = 0.0f;
		animData->modifierKeyFrame.values[RE::BSFaceGenKeyframeMultiple::Modifier::BlinkRight] = 0.0f;
	}
}
