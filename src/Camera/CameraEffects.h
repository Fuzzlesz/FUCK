#pragma once

namespace CameraEffects
{
    void Initialize();
    void Advance(RE::Actor* a_actor, float a_interval, bool a_menuPaused);
    void Reset();
}
