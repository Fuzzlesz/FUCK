#pragma once

namespace Freeze
{
	// Holds actors still for plugins, e.g. to pose them.
	// Each actor stays frozen while any plugin holds it, so one plugin's release can't let go of another's
	class Manager final : public REX::Singleton<Manager>
	{
	public:
		// a_holdPhysics: FSMP and CBPC stay paused with the game while it's held, as they can't be held for one actor alone
		void SetFrozen(const char* a_pluginName, RE::Actor* a_actor, bool a_frozen, bool a_holdPhysics = true);
		bool IsFrozen(RE::Actor* a_actor) const;
		bool HoldsPhysics() const;

		// Keeps frozen faces' eyes open and still, as the game would otherwise go on blinking
		void OnFrame();

		// Lets everything go before a save loads, so no actor is left frozen by a plugin that didn't get to release it
		void ReleaseAll();

	private:
		static void Apply(RE::Actor* a_actor, bool a_frozen);
		static void HoldEyes(RE::Actor* a_actor);

		struct Hold
		{
			RE::ActorHandle       actor;
			StringMap<bool> plugins;  // the plugins holding it, and whether each holds physics, so it's let go once none is left
		};

		Map<std::uint32_t, Hold> _frozen;  // by the actor's handle
	};
}
