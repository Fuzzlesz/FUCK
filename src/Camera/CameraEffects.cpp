#include "Camera/CameraEffects.h"

#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <utility>

namespace
{
    struct Addresses
    {
        std::uint64_t sceneTime;
        std::uint64_t uvWriteSlot;
        std::uint64_t casterAttachArt;
        std::uint64_t attachInputUpdate;
    };

    // Address Library IDs. Fill in the SE equivalents once the addresses and layouts are verified.
    inline constexpr Addresses kAddressesSE{};
    inline constexpr Addresses kAddressesAE{ 400333, 391007, 34185, 32899 };

    using Nodes = std::vector<RE::NiPointer<RE::NiAVObject>>;
    RE::FormID visualActor = 0;
    float sceneTime = 0.0f;

    float* worldTime = nullptr;
    std::uint32_t* writeOther = nullptr;
    using AttachArt = void (*)(RE::ActorMagicCaster*);
    using UpdateArt = bool (*)(void*, void*);
    AttachArt attach = nullptr;
    UpdateArt animate = nullptr;

    void Resolve(Addresses addresses)
    {
        if (!addresses.sceneTime || !addresses.uvWriteSlot)
            return;

        const REL::Offset2ID db{};
        const auto resolve = [&](std::uint64_t id) -> std::uintptr_t {
            if (!id)
                return 0;
            for (const auto& entry : db) {
                if (entry.id == id)
                    return REL::Module::get().base() + entry.offset;
            }
            return 0;
        };
        worldTime = reinterpret_cast<float*>(resolve(addresses.sceneTime));
        writeOther = reinterpret_cast<std::uint32_t*>(resolve(addresses.uvWriteSlot));
        attach = reinterpret_cast<AttachArt>(resolve(addresses.casterAttachArt));
        animate = reinterpret_cast<UpdateArt>(resolve(addresses.attachInputUpdate));
    }

    // ActorMagicCaster::Update visual steps, called under the paused UV-slot override.
    void UpdateCasterArt(RE::Actor* a_actor, float a_dt) {
        for (const auto source : { RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand }) {
            auto* const caster = static_cast<RE::ActorMagicCaster*>(a_actor->GetMagicCaster(source));
            if (!caster) {
                continue;
            }
            auto* const raw   = reinterpret_cast<std::uint8_t*>(caster);
            const auto  flags = *reinterpret_cast<std::uint32_t*>(raw + 0xF8);
            const bool  pending = *reinterpret_cast<void**>(raw + 0xE0) != nullptr;
            if (pending && (flags & 0x8) == 0) {
                attach(caster);
                continue;
            }
            if (!pending) {
                alignas(8) std::uint8_t data[0x30]{};
                std::memcpy(data, &a_dt, sizeof a_dt);
                const std::uint32_t mask = 0x1000000;
                const std::uint16_t one  = 1;
                std::memcpy(data + 0x28, &mask, sizeof mask);
                std::memcpy(data + 0x2C, &one, sizeof one);
                data[0x2E] = 1;
                animate(raw + 0x48, data);
            }
        }
    }

    void CollectAnimatedWorn(RE::Actor* a_actor, Nodes& a_out) {
        const auto& biped = a_actor->GetBiped1(false);
        if (!biped) {
            return;
        }
        constexpr std::uint32_t kPropertyControllers = 0x40000;
        const auto walk = [&](auto&& a_self, RE::NiAVObject* a_object) -> void {
            if (!a_object) {
                return;
            }
            if (a_object->GetControllers()) {
                a_out.emplace_back(a_object);
                return;
            }
            if (a_object->AsGeometry()) {
                if (a_object->GetFlags().underlying() & kPropertyControllers) {
                    a_out.emplace_back(a_object);
                }
                return;
            }
            if (auto* const node = a_object->AsNode()) {
                for (auto& child : node->GetChildren()) {
                    a_self(a_self, child.get());
                }
            }
        };
        std::unordered_set<RE::NiAVObject*> seen;
        for (auto& object : biped->objects) {
            auto* const clone = object.partClone.get();
            if (clone && seen.insert(clone).second) {
                walk(walk, clone);
            }
        }
    }


    void CollectVisuals(RE::Actor* actor, Nodes& nodes)
    {
        const auto handle = actor->GetHandle();
        if (auto* lists = RE::ProcessLists::GetSingleton()) {
            RE::BSSpinLockGuard guard(lists->magicEffectsLock);
            for (const auto& temp : lists->magicEffects) {
                if (!temp)
                    continue;
                if (temp->GetType() == RE::TEMP_EFFECT_TYPE::kRefModel) {
                    auto* effect = static_cast<RE::ModelReferenceEffect*>(temp.get());
                    if (effect->target == handle && effect->artObject3D)
                        nodes.emplace_back(effect->artObject3D.get());
                } else if (temp->GetType() == RE::TEMP_EFFECT_TYPE::kRefShader) {
                    auto* effect = static_cast<RE::ShaderReferenceEffect*>(temp.get());
                    if (effect->target != handle)
                        continue;
                    if (effect->lastRootNode)
                        nodes.emplace_back(effect->lastRootNode.get());
                    for (auto& set : effect->particleShaders) {
                        auto* prop = set.particleShaderProp.get();
                        if (auto* geom = prop ? prop->particleShaderGeometry.get() : nullptr)
                            nodes.emplace_back(reinterpret_cast<RE::NiAVObject*>(geom));
                    }
                }
            }
        }
        CollectAnimatedWorn(actor, nodes);
        auto* root = actor->Get3D(false);
        for (const char* name : { "NPC L MagicNode [LMag]", "NPC R MagicNode [RMag]" }) {
            auto* hand = root ? root->GetObjectByName(name) : nullptr;
            if (auto* node = hand ? hand->AsNode() : nullptr) {
                for (const auto& child : node->GetChildren()) {
                    if (child && child->AsNode())
                        nodes.emplace_back(child.get());
                }
            }
        }
    }

    // An art's refraction layer refracts a screen the menu's render doesn't fill
    std::vector<RE::NiPointer<RE::NiAVObject>> g_hazeHidden;

    void HideHaze(RE::Actor* a_actor) {
        auto* const lists = RE::ProcessLists::GetSingleton();
        if (!lists) {
            return;
        }
        Nodes arts;
        {
            const auto          handle = a_actor->GetHandle();
            RE::BSSpinLockGuard guard(lists->magicEffectsLock);
            for (auto& ptr : lists->magicEffects) {
                auto* const temp = ptr.get();
                if (temp && temp->GetType() == RE::TEMP_EFFECT_TYPE::kRefModel) {
                    auto* const m = static_cast<RE::ModelReferenceEffect*>(temp);
                    if (m->target == handle && m->artObject3D) {
                        arts.emplace_back(m->artObject3D.get());
                    }
                }
            }
        }
        const auto walk = [&](auto&& a_self, RE::NiAVObject* a_object) -> void {
            if (!a_object) {
                return;
            }
            if (auto* const geom = a_object->AsGeometry()) {
                auto* const shade = geom->GetGeometryRuntimeData().shaderProperty.get();
                if (!shade) {
                    return;
                }
                const bool refract = (shade->flags.underlying() &
                                      static_cast<std::uint64_t>(RE::BSShaderProperty::EShaderPropertyFlag::kRefraction)) != 0;
                if (refract && !geom->GetAppCulled()) {
                    geom->SetAppCulled(true);
                    g_hazeHidden.emplace_back(geom);
                }
                return;
            }
            if (auto* const node = a_object->AsNode()) {
                for (auto& child : node->GetChildren()) {
                    a_self(a_self, child.get());
                }
            }
        };
        for (const auto& art : arts) {
            walk(walk, art.get());
        }
    }

    void ShowHaze() {
        for (auto& geom : g_hazeHidden) {
            if (geom) {
                geom->SetAppCulled(false);
            }
        }
        g_hazeHidden.clear();
    }

    // A weapon equipped over a spell leaves the spell's art on the magic node. The caster's currentSpell is set only
    // mid-cast, so the hand's equipped object says what it holds
    std::vector<RE::NiPointer<RE::NiAVObject>> g_staleHidden;

    void HideStaleHandArt(RE::Actor* a_actor) {
        for (const auto source : { RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand }) {
            const bool  left   = source == RE::MagicSystem::CastingSource::kLeftHand;
            auto* const caster = static_cast<RE::ActorMagicCaster*>(a_actor->GetMagicCaster(source));
            auto* const held   = a_actor->GetEquippedObject(left);
            const bool  heldWeapon = held && !held->Is(RE::FormType::Spell) && !held->Is(RE::FormType::Scroll);
            if (!caster || !caster->magicNode || !heldWeapon) {
                continue;
            }
            if (auto* const node = caster->magicNode->AsNode()) {
                for (auto& child : node->GetChildren()) {
                    if (child && !child->GetAppCulled()) {
                        child->SetAppCulled(true);
                        g_staleHidden.emplace_back(child);
                    }
                }
            }
        }
    }

    void ShowStaleHandArt() {
        for (auto& o : g_staleHidden) {
            if (o) {
                o->SetAppCulled(false);
            }
        }
        g_staleHidden.clear();
    }

}

namespace CameraEffects
{
    void Initialize()
    {
        if (REL::Module::IsSE())
            Resolve(kAddressesSE);
        else if (REL::Module::IsAE())
            Resolve(kAddressesAE);
    }

    void Advance(RE::Actor* actor, float dt, bool menuPaused)
    {
        if (!worldTime || !writeOther || !actor || !actor->Get3D(false)) {
            Reset();
            return;
        }
        if (visualActor != actor->GetFormID()) {
            ShowHaze();
            ShowStaleHandArt();
            sceneTime = *worldTime;
            visualActor = actor->GetFormID();
        }
        const float step = std::clamp(dt, 0.0f, 0.05f);
        sceneTime += step;
        Nodes nodes;
        CollectVisuals(actor, nodes);
        std::unordered_set<RE::NiAVObject*> roots;
        for (const auto& node : nodes)
            roots.insert(node.get());
        RE::NiUpdateData data{};
        data.time = sceneTime;
        data.flags = RE::NiUpdateData::Flag::kDirty;
        struct VisibleUVSlot {
            std::uint32_t& slot;
            std::uint32_t saved;
            ~VisibleUVSlot() { slot = saved; }
        };
        {
            const VisibleUVSlot restore{ *writeOther, std::exchange(*writeOther, 0u) };
            for (auto* node : roots) {
                auto* ancestor = node->parent;
                while (ancestor && !roots.contains(ancestor))
                    ancestor = ancestor->parent;
                if (!ancestor)
                    node->Update(data);
            }
            if (attach && animate && step > 0.0f)
                UpdateCasterArt(actor, step);
        }
        ShowStaleHandArt();
        if (menuPaused) {
            HideHaze(actor);
            HideStaleHandArt(actor);
        } else {
            ShowHaze();
        }
    }

    void Reset()
    {
        ShowHaze();
        ShowStaleHandArt();
        visualActor = 0;
    }

}
