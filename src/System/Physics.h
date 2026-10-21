#pragma once

// Keeps FSMP's cloth and hair moving while the game is paused, for a camera session with kRunPhysics.
// FSMP steps its physics from a hook on a call in the game's frame update, and holds still while the game is paused.
// FUCK hooks the same call on both sides of FSMP's hook: the inner hook makes the game look unpaused once the game's
// own function has run, so FSMP's step sees it, and the outer hook puts everything back before the frame goes on.
namespace Physics
{
	// At kPostLoad, so it's inside FSMP's hook, which FSMP adds at kPostPostLoad
	void InstallInner();
	// At kDataLoaded, so it's outside FSMP's hook
	void InstallOuter();
}
