#pragma once

// Keeps body and cloth physics moving while the game is paused, for a camera session with kRunPhysics.
//
// FSMP steps its physics from a hook on a call in the game's frame update, and holds still while the game is paused.
// FUCK hooks the same call on both sides of FSMP's hook: the inner hook makes the game look unpaused once the game's
// own function has run, so FSMP's step sees it, and the outer hook puts everything back before the frame goes on.
//
// CBPC runs in a paused game only while it believes RaceMenu is open, which it learns from menu open and close events.
// FUCK tells CBPC's own event sink that RaceMenu opened, and that it closed again after. Nothing else gets those events.
namespace Physics
{
	// Finds FSMP and CBPC, and hooks inside FSMP's hook, which FSMP adds at kPostPostLoad
	void OnPostLoad();
	// Hooks outside FSMP's hook
	void OnDataLoaded();
	// Each frame: starts or stops CBPC as a session asks for physics in a paused game
	void OnFrame();
}
