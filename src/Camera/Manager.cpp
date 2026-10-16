#include "Camera/Manager.h"

#include "FUCK-Man.h"
#include "ImGui/Widgets.h"
#include "System/Input.h"
#include "System/InputMap.h"
#include "System/Settings.h"

#define SMOOTHCAM_API_COMMONLIB
#include "Camera/SmoothCamAPI.h"

namespace
{
	// The free camera's own update and pose queries, which a kFree session moves through
	struct FreeCameraUpdate
	{
		static void thunk(RE::FreeCameraState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
		{
			func(a_this, a_nextState);
			MANAGER(Camera)->OnFreeCameraUpdate(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            idx{ 0x03 };
	};

	struct FreeCameraGetRotation
	{
		static void thunk(RE::FreeCameraState* a_this, RE::NiQuaternion& a_rotation)
		{
			func(a_this, a_rotation);
			MANAGER(Camera)->OverrideFreeRotation(a_this, a_rotation);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            idx{ 0x04 };
	};

	struct FreeCameraGetTranslation
	{
		static void thunk(RE::FreeCameraState* a_this, RE::NiPoint3& a_translation)
		{
			func(a_this, a_translation);
			MANAGER(Camera)->OverrideFreeTranslation(a_this, a_translation);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            idx{ 0x05 };
	};

	// The third-person camera's update, which places the camera itself every frame while time runs
	struct ThirdPersonUpdate
	{
		static void thunk(RE::ThirdPersonState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
		{
			func(a_this, a_nextState);
			MANAGER(Camera)->OnThirdPersonUpdate(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            idx{ 0x03 };
	};

	SmoothCamAPI::IVSmoothCam2* g_smoothCam = nullptr;

	// The camera settings and their INI keys. Defaults come from Config, and only values that differ are written
	const FUCK::INI::Fields<Camera::Config> kConfigFields{ "Camera", {
		{ "KBMPanSpeed",    &Camera::Config::kbmPanSpeed     },
		{ "KBMRotSpeed",    &Camera::Config::kbmRotSpeed     },
		{ "KBMRollSpeed",   &Camera::Config::kbmRollSpeed    },
		{ "KBMFovSpeed",    &Camera::Config::kbmZoomFovSpeed },
		{ "GPPanSpeed",     &Camera::Config::gpPanSpeed      },
		{ "GPZoomSpeed",    &Camera::Config::gpZoomSpeed     },
		{ "GPRotSpeed",     &Camera::Config::gpRotSpeed      },
		{ "GPRollSpeed",    &Camera::Config::gpRollSpeed     },
		{ "GPFovSpeed",     &Camera::Config::gpFovSpeed      },
		{ "GPCharRotSpeed", &Camera::Config::gpCharRotSpeed  },
		{ "GPDeadzone",     &Camera::Config::gpDeadzone      },
		{ "Cinematic",      &Camera::Config::cinematic       },
	} };

	std::string GetConfigPath()
	{
		return ::Settings::Core.GetConfigDirectory() + "camera.ini";
	}

	std::string_view GetName(const Camera::Session& a_session)
	{
		return a_session.context.empty() ? "(unnamed)" : a_session.context;
	}

	// KBM/GP speeds use different units, so each maps to a shared 1-20 display with the default at 10
	bool SpeedSlider(const char* a_label, float* a_value, float a_default)
	{
		return FUCK::ScaledSliderFloat(a_label, a_value, a_default / 10.0f, a_default * 2.0f, 10.0f / a_default);
	}
}

namespace Camera
{
	void Manager::InstallHooks()
	{
		stl::write_vfunc<RE::FreeCameraState, FreeCameraUpdate>();
		stl::write_vfunc<RE::FreeCameraState, FreeCameraGetRotation>();
		stl::write_vfunc<RE::FreeCameraState, FreeCameraGetTranslation>();

		logger::info("Installed Camera Hooks");
	}

	void Manager::InstallLateHooks()
	{
		// Once every plugin has loaded, so this wraps any other mod's hook on the same update (e.g. SmoothCam's) and always runs last
		stl::write_vfunc<RE::ThirdPersonState, ThirdPersonUpdate>();

		logger::info("Installed Third-Person Camera Hook");
	}

	void Manager::RegisterSmoothCam()
	{
		bool registered = SmoothCamAPI::RegisterInterfaceLoaderCallback(SKSE::GetMessagingInterface(), [](void* a_interface, SmoothCamAPI::InterfaceVersion a_version) {
			if (a_version == SmoothCamAPI::InterfaceVersion::V2 || a_version == SmoothCamAPI::InterfaceVersion::V3) {
				g_smoothCam = reinterpret_cast<SmoothCamAPI::IVSmoothCam2*>(a_interface);
				logger::info("Camera: obtained SmoothCam's API");
			} else {
				logger::warn("Camera: SmoothCam returned an API version FUCK doesn't support");
			}
		});

		if (!registered) {
			logger::warn("Camera: couldn't listen for SmoothCam's API");
		}
	}

	void Manager::RequestSmoothCam()
	{
		// Fails quietly when SmoothCam isn't installed
		if (!SmoothCamAPI::RequestInterface(SKSE::GetMessagingInterface(), SmoothCamAPI::InterfaceVersion::V2)) {
			logger::info("Camera: SmoothCam not found");
		}
	}

	// ==================================================
	// Settings
	// ==================================================

	void Manager::LoadSettings()
	{
		::Settings::LoadINI(GetConfigPath().c_str(), [this](CSimpleIniA& ini) { kConfigFields.Load(ini, _config); });
	}

	void Manager::SaveSettings() const
	{
		::Settings::LoadINI(GetConfigPath().c_str(), [this](CSimpleIniA& ini) { kConfigFields.Save(ini, _config); }, true);
	}

	// ==================================================
	// Sessions
	// ==================================================

	bool Manager::BeginSession(const FUCK::CameraSessionOptions* a_options)
	{
		if (!a_options || a_options->size == 0)
			return false;

		// Read only as much as the caller's version of the struct holds, so newer options keep their defaults
		FUCK::CameraSessionOptions opts;
		std::memcpy(&opts, a_options, std::min<std::size_t>(a_options->size, sizeof(opts)));

		if (!opts.client)
			return false;

		// The same client beginning again starts over, without being told it was taken over
		if (auto existing = Find(opts.client)) {
			if (existing == _current) {
				EndCurrent(false);
			} else {
				Remove(*existing);
			}
		}

		if (_current) {
			if (_current->flags & FUCK::CameraSessionFlags::kResumable) {
				PauseCurrent();
			} else {
				EndCurrent(true);
			}
		}

		auto session        = std::make_unique<Session>(*this);
		session->client     = opts.client;
		session->context    = opts.context ? opts.context : "";
		session->base       = opts.base;
		session->flags      = opts.flags;
		session->hasSubject = opts.subject != nullptr;
		session->subject    = session->hasSubject ? opts.subject->GetHandle() : RE::ActorHandle{};

		auto camera = RE::PlayerCamera::GetSingleton();
		if (camera && camera->currentState) {
			session->startingState = camera->currentState->id;
		}

		_sessions.push_back(std::move(session));
		_current = _sessions.back().get();

		EnterBase(*_current);

		// The client has set the camera up by now, so this is the pose the third-person base holds
		if (_current->base == FUCK::CameraBase::kThirdPerson) {
			if (auto node = camera ? camera->cameraRoot.get() : nullptr) {
				_current->fixedTranslate = node->local.translate;
				_current->fixedRotate    = node->local.rotate;
				_current->hasFixedBase   = true;
			}
		}

		_current->presets.Open(_current->context, _current->base == FUCK::CameraBase::kFree);
		SetUpStart(opts);
		UpdateSmoothCam();

		logger::info("Camera session began: {}", GetName(*_current));
		return true;
	}

	void Manager::EndSession(FUCK::ICameraClient* a_client)
	{
		auto session = Find(a_client);
		if (!session)
			return;

		logger::info("Camera session ended: {}", GetName(*session));

		if (session == _current) {
			EndCurrent(false);
		} else {
			Remove(*session);
		}

		TryResume();
		UpdateSmoothCam();
	}

	bool Manager::IsPaused(const FUCK::ICameraClient* a_client) const
	{
		auto session = Find(a_client);
		return session && session->paused;
	}

	Session* Manager::Find(const FUCK::ICameraClient* a_client) const
	{
		if (!a_client)
			return nullptr;

		auto it = std::ranges::find(_sessions, a_client, [](const auto& a_session) { return a_session->client; });
		return it != _sessions.end() ? it->get() : nullptr;
	}

	void Manager::EndCurrent(bool a_takenOver)
	{
		if (!_current)
			return;

		auto client = _current->client;

		RememberCamera();
		LeaveBase(*_current);

		// The current session is always the newest
		_sessions.pop_back();
		_current = nullptr;

		// Lifts the session's time freeze now, in case nothing FUCK draws is open to do it
		FUCKMan::GetSingleton()->RefreshGameState();

		if (a_takenOver) {
			client->OnCameraTakenOver();
		}
	}

	void Manager::PauseCurrent()
	{
		auto& session = *_current;

		// Remembered now, as the newer session owns the base camera until this one resumes
		RememberCamera();

		RevertNode();
		session.core.Pause();
		session.freeFinal.valid = false;
		session.paused          = true;
		_current                = nullptr;

		logger::info("Camera session paused: {}", GetName(session));
		session.client->OnCameraPaused();
	}

	void Manager::Remove(Session& a_session)
	{
		auto it = std::ranges::find(_sessions, &a_session, [](const auto& a_ptr) { return a_ptr.get(); });
		if (it == _sessions.end())
			return;

		// What it set up for the game is still in use by the sessions above it, so the next one up takes it over rather than it being undone
		auto above = std::next(it) != _sessions.end() ? std::next(it)->get() : nullptr;
		if (above) {
			if (a_session.enteredFreeCamera && !above->enteredFreeCamera) {
				above->enteredFreeCamera   = true;
				above->previousCameraState = a_session.previousCameraState;
			}
			above->disabledSaving |= a_session.disabledSaving;

			a_session.enteredFreeCamera = false;
			a_session.disabledSaving    = false;
		}

		LeaveBase(a_session);
		_sessions.erase(it);

		FUCKMan::GetSingleton()->RefreshGameState();
	}

	void Manager::TryResume()
	{
		if (_current || _sessions.empty())
			return;

		auto& session = *_sessions.back();
		if (!IsBaseReady(session))
			return;

		session.paused = false;
		_current       = &session;

		// Another session may have turned the subject in the meantime
		if (session.subjectYaw) {
			SetSubjectYaw(*session.subjectYaw);
		}

		FUCKMan::GetSingleton()->RefreshGameState();

		logger::info("Camera session resumed: {}", GetName(session));
		session.client->OnCameraResumed();
	}

	bool Manager::IsBaseReady(const Session& a_session) const
	{
		auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->currentState)
			return false;

		bool inFree = camera->currentState->id == RE::CameraState::kFree;
		if (a_session.base == FUCK::CameraBase::kFree)
			return inFree;

		// An offset session over a menu's camera waits for a free-camera session above it to leave the free camera
		return !inFree || a_session.startingState == RE::CameraState::kFree;
	}

	void Manager::EnterBase(Session& a_session)
	{
		if (a_session.base == FUCK::CameraBase::kFree) {
			if (auto camera = RE::PlayerCamera::GetSingleton()) {
				a_session.previousCameraState = camera->currentState ? camera->currentState->id : RE::CameraState::kThirdPerson;
				a_session.enteredFreeCamera   = a_session.previousCameraState != RE::CameraState::kFree;
				if (a_session.enteredFreeCamera) {
					camera->ToggleFreeCameraMode(false);
				}
			}
		}

		// Leave it alone if it's already set, e.g. by character creation, so ending the session can't clear it
		if (a_session.flags & FUCK::CameraSessionFlags::kDisableSaving) {
			if (auto player = RE::PlayerCharacter::GetSingleton()) {
				auto& flags = player->GetPlayerRuntimeData().byCharGenFlag;
				if (!flags.any(RE::PlayerCharacter::ByCharGenFlag::kDisableSaving)) {
					flags.set(RE::PlayerCharacter::ByCharGenFlag::kDisableSaving);
					a_session.disabledSaving = true;
				}
			}
		}

		a_session.core.SetZoomAcceleration(a_session.flags & FUCK::CameraSessionFlags::kZoomAcceleration);
		a_session.core.SetKeyboardEnabled(!(a_session.flags & FUCK::CameraSessionFlags::kNoKeyboard));

		FUCKMan::GetSingleton()->RefreshGameState();
	}

	void Manager::LeaveBase(Session& a_session)
	{
		if (&a_session == _current) {
			RevertNode();
		}

		// A paused session already gave the FOV back, and it may be another session's by now
		if (!a_session.paused) {
			a_session.core.Reset();
		}

		if (a_session.enteredFreeCamera) {
			auto camera = RE::PlayerCamera::GetSingleton();
			if (camera && camera->IsInFreeCameraMode()) {
				camera->ToggleFreeCameraMode(false);
			}
			a_session.enteredFreeCamera = false;
		}
		a_session.freeBase.valid  = false;
		a_session.freeFinal.valid = false;

		if (a_session.disabledSaving) {
			if (auto player = RE::PlayerCharacter::GetSingleton()) {
				player->GetPlayerRuntimeData().byCharGenFlag.reset(RE::PlayerCharacter::ByCharGenFlag::kDisableSaving);
			}
			a_session.disabledSaving = false;
		}

		a_session.presets.Close();
	}

	void Manager::SetUpStart(const FUCK::CameraSessionOptions& a_options)
	{
		auto& session = *_current;

		// The subject first, as remembered facing and body framing are measured from where it stands
		if (session.flags & FUCK::CameraSessionFlags::kFaceCamera) {
			RE::NiPoint3  baseTranslate;
			RE::NiMatrix3 baseRotate;
			if (GetBasePose(baseTranslate, baseRotate)) {
				SetSubjectYaw(Maths::YawOf(baseRotate) + std::numbers::pi_v<float>);
			}
		}

		if (!IsOffsetBase(session.base))
			return;

		// Where the last session in this context left the camera, else the starting framing. Either lands at once rather than easing in
		const auto& last = session.presets.GetLast();
		if ((session.flags & FUCK::CameraSessionFlags::kRememberCamera) && last) {
			ApplyOffsets(*last);
			session.core.Settle();
		} else if (a_options.startFramed) {
			session.core.SetBodyFraming(true, a_options.framingRect, a_options.framingMargin);
			session.core.Settle();
		}
	}

	void Manager::RememberCamera()
	{
		if (!_current || !IsOffsetBase(_current->base) || !(_current->flags & FUCK::CameraSessionFlags::kRememberCamera))
			return;

		Preset preset;
		if (CapturePreset(false, preset) && preset.offset) {
			_current->presets.StoreLast(*preset.offset);
		}
	}

	void Manager::UpdateSmoothCam()
	{
		// Held while any session over the third-person camera is open, paused ones included, so SmoothCam can't take it back between them
		bool wanted = std::ranges::any_of(_sessions, [](const auto& a_session) { return a_session->base == FUCK::CameraBase::kThirdPerson; });
		if (wanted == _holdsSmoothCam || !g_smoothCam)
			return;

		auto handle = SKSE::GetPluginHandle();

		if (wanted) {
			if (!g_smoothCam->IsCameraEnabled())
				return;

			auto result = g_smoothCam->RequestCameraControl(handle);
			if (result == SmoothCamAPI::APIResult::OK || result == SmoothCamAPI::APIResult::AlreadyGiven) {
				// As Show Player in Inventory does, so SmoothCam's interpolation carries on smoothly once it's given back
				g_smoothCam->RequestInterpolatorUpdates(handle, true);
				_holdsSmoothCam = true;
				logger::info("Camera: took control from SmoothCam");
			} else {
				logger::warn("Camera: SmoothCam refused control ({})", std::to_underlying(result));
			}
		} else {
			g_smoothCam->ReleaseCameraControl(handle);
			_holdsSmoothCam = false;
			logger::info("Camera: gave control back to SmoothCam");
		}
	}

	void Manager::RunControls(float a_interval)
	{
		auto client = _current->client;

		FUCK::CameraControlState state;

		_isUpdating     = true;
		_updateInterval = a_interval;
		client->OnCameraUpdate(state, a_interval);
		_isUpdating     = false;

		// The client may have ended its session from inside the callback
		if (!IsActive(client))
			return;

		// Kept for the input hook, which decides between frames whether the controls are in use
		_inputAllowed  = state.inputAllowed;
		_mouseLocked   = state.mouseLocked;
		_cameraMoves   = state.cameraMoves;

		UpdatePresetHotkeys(state.inputAllowed);
		_current->core.Update(state, _config, a_interval);
	}

	bool Manager::IsBlockingInput() const
	{
		if (!_current)
			return false;

		if (_current->flags & FUCK::CameraSessionFlags::kBlockControls)
			return true;

		if (!_inputAllowed)
			return false;

		namespace Keys = Input::Keymap;

		// Blocked while the camera's controls are held, so a menu underneath doesn't act on the same keys. ImGui also only gets the mouse while input is blocked.
		// A held mouse button or RB blocks on its own, and Ctrl and Shift are then just modifiers. Right-drag and RB turn the subject, so they still block with cameraMoves off
		bool mouse = !_mouseLocked && (FUCK::IsInputDown(Keys::kMouse_Right) || (_cameraMoves && FUCK::IsInputDown(Keys::kMouse_Middle)));
		if (mouse || FUCK::IsInputDown(Keys::kGP_RB))
			return true;

		if (!_cameraMoves)
			return false;

		// Ctrl only when used with the camera's keys or the wheel, so other mods' Ctrl shortcuts still reach the game
		if (!FUCK::IsModifierPressed(FUCK::Modifier::kCtrl))
			return false;

		if (Input::Manager::GetSingleton()->IsWheelInBatch())
			return true;

		// Ctrl+1-9 too when they load presets
		if (_current->flags & FUCK::CameraSessionFlags::kPresetHotkeys) {
			for (int number = 0; number < 9; ++number) {
				if (FUCK::IsInputDown(static_cast<std::uint32_t>(KEY::kNum1) + number))
					return true;
			}
		}

		if (_current->flags & FUCK::CameraSessionFlags::kNoKeyboard)
			return false;

		constexpr std::array cameraKeys{ Keys::kKB_W, Keys::kKB_A, Keys::kKB_S, Keys::kKB_D, Keys::kKB_Q, Keys::kKB_E, Keys::kKB_R, Keys::kKB_F };
		return std::ranges::any_of(cameraKeys, [](std::uint32_t a_key) { return FUCK::IsInputDown(a_key); });
	}

	void Manager::Move(const FUCK::CameraMove* a_move, FUCK::InputDevice a_device, float a_mult)
	{
		if (!_isUpdating || !a_move || !_current)
			return;

		_current->core.Move(*a_move, _config, a_device == FUCK::InputDevice::kGamepad, a_mult, _updateInterval);
	}

	void Manager::Reset(FUCK::ICameraClient* a_client)
	{
		if (IsActive(a_client)) {
			RevertNode();
			_current->core.Reset();
		}
	}

	bool Manager::IsModified(const FUCK::ICameraClient* a_client) const
	{
		return IsActive(a_client) && _current->core.IsModified();
	}

	void Manager::SetBodyFraming(FUCK::ICameraClient* a_client, bool a_framed, const FUCK::CameraScreenRect* a_rect, float a_margin)
	{
		if (IsActive(a_client) && a_rect) {
			_current->core.SetBodyFraming(a_framed, *a_rect, a_margin);
		}
	}

	bool Manager::IsBodyFramed(const FUCK::ICameraClient* a_client) const
	{
		return IsActive(a_client) && _current->core.IsBodyFramed();
	}

	bool Manager::GetOffsets(const FUCK::ICameraClient* a_client, FUCK::CameraOffsets* a_offsets) const
	{
		if (!a_offsets || !IsActive(a_client) || !IsOffsetBase(_current->base))
			return false;

		auto targets         = _current->core.GetTargetOffsets();
		a_offsets->translate = targets.translate;
		a_offsets->orbit     = targets.orbit;
		a_offsets->pitch     = targets.pitch;
		a_offsets->roll      = targets.roll;
		a_offsets->fov       = targets.fov;
		return true;
	}

	bool Manager::SetOffsets(FUCK::ICameraClient* a_client, const FUCK::CameraOffsets* a_offsets)
	{
		if (!a_offsets || !IsActive(a_client) || !IsOffsetBase(_current->base))
			return false;

		Offsets targets;
		targets.translate = a_offsets->translate;
		targets.orbit     = a_offsets->orbit;
		targets.pitch     = a_offsets->pitch;
		targets.roll      = a_offsets->roll;
		targets.fov       = a_offsets->fov;
		_current->core.SetTargetOffsets(targets);
		return true;
	}

	void Manager::OnFrame()
	{
		// Set again by DrawPresets() if the list is still open over an entry this frame
		_presetHovered = false;

		// A session waiting on its camera to come back, e.g. for the free camera to be left
		TryResume();

		if (!_current)
			return;

		if (float wheel = FUCK::GetMouseWheel(); wheel != 0.0f) {
			_current->core.AccumulateScroll(wheel);
		}
	}

	// ==================================================
	// Offset base
	// ==================================================

	void Manager::Revert(FUCK::ICameraClient* a_client)
	{
		if (IsActive(a_client) && IsOffsetBase(_current->base)) {
			RevertNode();
		}
	}

	void Manager::Update(FUCK::ICameraClient* a_client, float a_interval)
	{
		if (!IsActive(a_client) || !IsOffsetBase(_current->base))
			return;

		RunControls(a_interval);
		if (IsActive(a_client)) {
			ApplyNode();
		}
	}

	void Manager::ApplyNode()
	{
		auto camera = RE::PlayerCamera::GetSingleton();
		auto node   = camera ? camera->cameraRoot.get() : nullptr;
		if (!node)
			return;

		// Composed from where the game left the camera, so a client that skipped RevertCamera() can't stack this frame on the last
		RevertNode();

		auto& session = *_current;

		// The third-person camera follows the subject's heading, which turning changes, so it's held where it was as the session began
		const auto& baseTranslate = session.hasFixedBase ? session.fixedTranslate : node->local.translate;
		const auto& baseRotate    = session.hasFixedBase ? session.fixedRotate : node->local.rotate;

		// Without offsets there's nothing to apply, unless the base is held, as the game's camera may have moved from it
		RE::NiPoint3  translate;
		RE::NiMatrix3 rotate;
		if (!session.core.Compose(baseTranslate, baseRotate, translate, rotate) && !session.hasFixedBase)
			return;

		session.nodeOriginalTranslate = node->local.translate;
		session.nodeOriginalRotate    = node->local.rotate;

		node->local.translate         = translate;
		node->local.rotate            = rotate;
		session.nodeAppliedTranslate  = translate;
		session.nodeAppliedRotate     = rotate;
		session.nodeModified          = true;

		// The whole branch, so the camera under the root moves too even when nothing else updates the scene, e.g. in a paused menu
		RE::NiUpdateData ctx;
		node->Update(ctx);
	}

	void Manager::RevertNode()
	{
		if (!_current || !_current->nodeModified)
			return;

		auto& session = *_current;

		// Only if the node still holds what FUCK applied, as the game may have placed it again since
		auto camera = RE::PlayerCamera::GetSingleton();
		if (auto node = camera ? camera->cameraRoot.get() : nullptr) {
			if (node->local.translate == session.nodeAppliedTranslate && node->local.rotate == session.nodeAppliedRotate) {
				node->local.translate = session.nodeOriginalTranslate;
				node->local.rotate    = session.nodeOriginalRotate;

				RE::NiUpdateData ctx;
				node->Update(ctx);
			}
		}
		session.nodeModified = false;
	}

	void Manager::OnThirdPersonUpdate(RE::ThirdPersonState* a_state)
	{
		if (!IsThirdPerson())
			return;

		auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || camera->currentState.get() != a_state)
			return;

		// The game has just placed the camera, replacing anything FUCK applied, so this is the new base
		_current->nodeModified = false;
		ApplyNode();
	}

	// ==================================================
	// Free-camera base
	// ==================================================

	RE::FreeCameraState* Manager::GetFreeState() const
	{
		auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->currentState || camera->currentState->id != RE::CameraState::kFree)
			return nullptr;

		return static_cast<RE::FreeCameraState*>(camera->currentState.get());
	}

	bool Manager::IsFreeState(const RE::FreeCameraState* a_state) const
	{
		return a_state && IsFree() && GetFreeState() == a_state;
	}

	void Manager::OnFreeCameraUpdate(RE::FreeCameraState* a_state)
	{
		if (!IsFreeState(a_state)) {
			if (_current) {
				_current->freeFinal.valid = false;
			}
			return;
		}

		auto& session = *_current;

		// The pose the free camera would show on its own, before FUCK's moves
		FreeCameraGetTranslation::func(a_state, session.freeBase.translate);
		FreeCameraGetRotation::func(a_state, session.freeBase.quat);
		session.freeBase.rotate = session.freeBase.quat.ToRotation();
		session.freeBase.valid  = true;

		// Real time, as the session may have frozen the game's
		auto  timer    = RE::BSTimer::GetSingleton();
		float interval = timer ? timer->realTimeDelta : 0.0f;

		RunControls(interval);
		if (!IsFreeState(a_state)) {
			session.freeFinal.valid = false;
			return;
		}

		session.core.Compose(session.freeBase.translate, session.freeBase.rotate, session.freeFinal.translate, session.freeFinal.rotate);
		session.freeFinal.quat  = RE::NiQuaternion(session.freeFinal.rotate);
		session.freeFinal.valid = true;
	}

	void Manager::OverrideFreeRotation(const RE::FreeCameraState* a_state, RE::NiQuaternion& a_rotation) const
	{
		if (IsFreeState(a_state) && _current->freeFinal.valid) {
			a_rotation = _current->freeFinal.quat;
		}
	}

	void Manager::OverrideFreeTranslation(const RE::FreeCameraState* a_state, RE::NiPoint3& a_translation) const
	{
		if (!IsFreeState(a_state))
			return;

		if (_current->freeFinal.valid) {
			a_translation = _current->freeFinal.translate;
		}
		_current->client->OnCameraFinalPosition(a_translation);
	}

	bool Manager::GetPose(const FUCK::ICameraClient* a_client, FUCK::CameraPose* a_pose) const
	{
		if (!a_pose || !IsActive(a_client) || _current->base != FUCK::CameraBase::kFree)
			return false;

		const auto& pose = _current->freeFinal.valid ? _current->freeFinal : _current->freeBase;
		if (!pose.valid)
			return false;

		a_pose->position = pose.translate;
		Maths::AnglesFromRotation(pose.rotate, a_pose->pitch, a_pose->yaw, a_pose->roll);

		auto camera = RE::PlayerCamera::GetSingleton();
		a_pose->fov = camera ? camera->GetRuntimeData2().worldFOV : 0.0f;
		return true;
	}

	bool Manager::SetPose(FUCK::ICameraClient* a_client, const FUCK::CameraPose* a_pose)
	{
		if (!a_pose || !IsActive(a_client) || _current->base != FUCK::CameraBase::kFree)
			return false;

		auto state = GetFreeState();
		if (!state)
			return false;

		// The free camera holds position, pitch and yaw itself, while roll and FOV are FUCK's
		state->translation = a_pose->position;
		state->rotation.x  = a_pose->pitch;
		state->rotation.y  = a_pose->yaw;
		_current->core.SnapTo(a_pose->roll, a_pose->fov);

		// Shown as set until the next update recalculates the final pose
		_current->freeFinal.valid = false;
		return true;
	}

	// ==================================================
	// Base and subject
	// ==================================================

	RE::Actor* Manager::GetSubject() const
	{
		if (_current && _current->hasSubject) {
			return _current->subject.get().get();
		}
		return RE::PlayerCharacter::GetSingleton();
	}

	bool Manager::GetBasePose(RE::NiPoint3& a_translate, RE::NiMatrix3& a_rotate)
	{
		if (!_current)
			return false;

		if (_current->base == FUCK::CameraBase::kFree) {
			if (!_current->freeBase.valid)
				return false;
			a_translate = _current->freeBase.translate;
			a_rotate    = _current->freeBase.rotate;
			return true;
		}

		if (_current->hasFixedBase) {
			a_translate = _current->fixedTranslate;
			a_rotate    = _current->fixedRotate;
			return true;
		}

		// Once the offsets are applied the node holds them, so the base is the transform they replaced
		if (_current->nodeModified) {
			a_translate = _current->nodeOriginalTranslate;
			a_rotate    = _current->nodeOriginalRotate;
			return true;
		}

		auto camera = RE::PlayerCamera::GetSingleton();
		auto node   = camera ? camera->cameraRoot.get() : nullptr;
		if (!node)
			return false;

		a_translate = node->local.translate;
		a_rotate    = node->local.rotate;
		return true;
	}

	std::optional<RE::NiPoint3> Manager::GetPivot()
	{
		if (_current && (_current->flags & FUCK::CameraSessionFlags::kPivotAtCamera)) {
			RE::NiPoint3  translate;
			RE::NiMatrix3 rotate;
			if (GetBasePose(translate, rotate))
				return translate;
		}

		auto subject = GetSubject();
		if (!subject)
			return std::nullopt;

		if (auto head = subject->GetNodeByName("NPC Head [Head]"))
			return head->world.translate;

		return subject->GetPosition();
	}

	bool Manager::GetBodyPoints(RE::NiPoint3& a_centre, RE::NiPoint3& a_base)
	{
		auto subject = GetSubject();
		if (!subject)
			return false;

		a_base = subject->GetPosition();

		if (auto pelvis = subject->GetNodeByName("NPC Pelvis [Pelv]")) {
			a_centre = pelvis->world.translate;
		} else if (auto head = subject->GetNodeByName("NPC Head [Head]")) {
			a_centre = head->world.translate + (a_base - head->world.translate) * 0.5f;
		} else {
			a_centre = a_base;
		}
		return true;
	}

	bool Manager::IsScrollBlocked()
	{
		return _current && _current->client->IsCameraScrollBlocked();
	}

	void Manager::RotateSubject(float a_amount)
	{
		auto subject = GetSubject();
		if (!subject || !subject->Is3DLoaded())
			return;

		if (auto root = subject->Get3D(false)) {
			RE::NiMatrix3 deltaMat;
			deltaMat.SetEulerAnglesXYZ(0.0f, 0.0f, -a_amount);

			if (TurnsHeading()) {
				SetSubjectYaw(Maths::YawOf(deltaMat * root->local.rotate));
				return;
			}

			// Turns the 3D rather than the actor's heading, so head tracking keeps its target instead of following the camera
			root->local.rotate = deltaMat * root->local.rotate;

			// The whole branch, so the body follows even when its animation isn't updating, e.g. in a paused menu
			RE::NiUpdateData ctx;
			root->Update(ctx);
		}
	}

	void Manager::SetSubjectYaw(float a_yaw)
	{
		auto subject = GetSubject();
		auto root    = subject ? subject->Get3D(false) : nullptr;
		if (!root)
			return;

		// The heading matches YawOf(), and moving the 3D to it also undoes any earlier 3D turn
		if (TurnsHeading()) {
			subject->data.angle.z = a_yaw;
			subject->Update3DPosition(true);
			_current->subjectYaw = a_yaw;
			return;
		}

		// Turned around world Z by the difference, keeping any tilt. A positive yaw turns the opposite way to MakeYaw
		float delta = a_yaw - Maths::YawOf(root->local.rotate);

		root->local.rotate = Maths::MakeYaw(-delta) * root->local.rotate;

		RE::NiUpdateData ctx;
		root->Update(ctx);
	}

	void Manager::MoveSubject(const SubjectTransform& a_transform)
	{
		auto subject     = GetSubject();
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!subject || !dataHandler || a_transform.cellPlugin.empty())
			return;

		// Only within the loaded area, as Photo Mode does, since moving across cells needs a load
		auto cell = dataHandler->LookupForm<RE::TESObjectCELL>(a_transform.cellLocalID, a_transform.cellPlugin);
		if (!cell || !cell->IsAttached())
			return;

		subject->SetPosition(RE::NiPoint3{ a_transform.position[0], a_transform.position[1], a_transform.position[2] }, true);
		subject->SetAngle(RE::NiPoint3{ a_transform.angle[0], a_transform.angle[1], a_transform.angle[2] });

		if (a_transform.facingYaw) {
			SetSubjectYaw(*a_transform.facingYaw);
		}
	}

	// ==================================================
	// Presets
	// ==================================================

	bool Manager::SavePreset(FUCK::ICameraClient* a_client, const char* a_name, bool a_withSubject)
	{
		if (!IsActive(a_client) || !_current->presets.IsOpen())
			return false;

		auto& session = *_current;

		Preset preset;
		if (!CapturePreset(a_withSubject, preset))
			return false;

		if (a_name && a_name[0] != '\0') {
			preset.name = a_name;
		} else {
			// A missing translation comes back as its key, which would make a poor preset name
			std::string_view base = "$FUCK_Camera_PresetDefaultName"_T;
			preset.name           = session.presets.MakeUniqueName(base.starts_with('$') ? "Preset" : base);
		}

		session.selectedPreset = preset.name;
		session.presets.Store(std::move(preset));
		return true;
	}

	bool Manager::CapturePreset(bool a_withSubject, Preset& a_preset)
	{
		auto& session = *_current;
		auto  subject = GetSubject();

		if (IsOffsetBase(session.base)) {
			// Where the camera is heading, so a save mid-ease keeps the intended view
			OffsetPreset offset;
			auto         targets = session.core.GetTargetOffsets();
			offset.offset        = { targets.translate.x, targets.translate.y, targets.translate.z };
			offset.orbit         = targets.orbit;
			offset.pitch         = targets.pitch;
			offset.roll          = targets.roll;
			offset.fov           = targets.fov;

			FUCK::CameraScreenRect rect;
			offset.framed = session.core.GetBodyFraming(rect, offset.framingMargin);
			if (offset.framed) {
				offset.framingRect = { rect.centreX, rect.centreY, rect.halfHeight };
			}

			// Relative to the base camera, as that's placed around the subject and moves with it between cells
			RE::NiPoint3  baseTranslate;
			RE::NiMatrix3 baseRotate;
			auto          root = subject ? subject->Get3D(false) : nullptr;
			if (root && GetBasePose(baseTranslate, baseRotate)) {
				offset.facingYaw = Maths::YawOf(root->local.rotate) - Maths::YawOf(baseRotate);
			}

			a_preset.offset = offset;
			return true;
		}

		FUCK::CameraPose pose;
		if (!GetPose(session.client, &pose))
			return false;

		FreePreset free;
		free.position = { pose.position.x, pose.position.y, pose.position.z };
		free.pitch    = pose.pitch;
		free.yaw      = pose.yaw;
		free.roll     = pose.roll;
		free.fov      = pose.fov;

		if (a_withSubject && subject) {
			SubjectTransform transform;
			auto             position = subject->GetPosition();
			auto             angle    = subject->GetAngle();
			transform.position        = { position.x, position.y, position.z };
			transform.angle           = { angle.x, angle.y, angle.z };

			// Turning moves the 3D rather than the heading, so keep its facing too
			if (auto root = subject->Get3D(false)) {
				transform.facingYaw = Maths::YawOf(root->local.rotate);
			}

			if (auto cell = subject->GetParentCell()) {
				if (auto file = cell->GetFile(0)) {
					transform.cellPlugin  = file->GetFilename();
					transform.cellLocalID = cell->GetLocalFormID();
				}
			}
			free.subject = transform;
		}

		a_preset.free = free;
		return true;
	}

	bool Manager::LoadPreset(FUCK::ICameraClient* a_client, const char* a_name)
	{
		if (!IsActive(a_client) || !a_name)
			return false;

		auto preset = _current->presets.Find(a_name);
		return preset && ApplyPreset(*preset);
	}

	bool Manager::ApplyPreset(const Preset& a_preset)
	{
		auto& session = *_current;

		if (IsOffsetBase(session.base) && a_preset.offset) {
			ApplyOffsets(*a_preset.offset);
		} else if (session.base == FUCK::CameraBase::kFree && a_preset.free) {
			const auto& free = *a_preset.free;

			if (free.subject) {
				MoveSubject(*free.subject);
			}

			FUCK::CameraPose pose;
			pose.position = { free.position[0], free.position[1], free.position[2] };
			pose.pitch    = free.pitch;
			pose.yaw      = free.yaw;
			pose.roll     = free.roll;
			pose.fov      = free.fov;
			if (!SetPose(session.client, &pose))
				return false;
		} else {
			// Saved from a session on the other kind of base under the same context, so it can't be loaded here
			return false;
		}

		session.selectedPreset = a_preset.name;
		return true;
	}

	void Manager::ApplyOffsets(const OffsetPreset& a_offset)
	{
		auto& core = _current->core;

		// Framing first, as changing it eases out the offsets
		FUCK::CameraScreenRect rect{ a_offset.framingRect[0], a_offset.framingRect[1], a_offset.framingRect[2] };
		core.SetBodyFraming(a_offset.framed, rect, a_offset.framingMargin);

		Offsets targets;
		targets.translate = { a_offset.offset[0], a_offset.offset[1], a_offset.offset[2] };
		targets.orbit     = a_offset.orbit;
		targets.pitch     = a_offset.pitch;
		targets.roll      = a_offset.roll;
		targets.fov       = a_offset.fov;
		core.SetTargetOffsets(targets);

		RE::NiPoint3  baseTranslate;
		RE::NiMatrix3 baseRotate;
		if (a_offset.facingYaw && GetBasePose(baseTranslate, baseRotate)) {
			SetSubjectYaw(Maths::YawOf(baseRotate) + *a_offset.facingYaw);
		}
	}

	bool Manager::DeletePreset(FUCK::ICameraClient* a_client, const char* a_name)
	{
		if (!IsActive(a_client) || !a_name || !_current->presets.Remove(a_name))
			return false;

		if (_current->selectedPreset == a_name) {
			_current->selectedPreset.clear();
		}
		return true;
	}

	int Manager::GetPresetCount(const FUCK::ICameraClient* a_client) const
	{
		return IsActive(a_client) ? static_cast<int>(_current->presets.GetAll().size()) : 0;
	}

	const char* Manager::GetPresetName(const FUCK::ICameraClient* a_client, int a_index) const
	{
		if (!IsActive(a_client))
			return nullptr;

		const auto& all = _current->presets.GetAll();
		if (a_index < 0 || a_index >= static_cast<int>(all.size()))
			return nullptr;

		return all[a_index].name.c_str();
	}

	void Manager::UpdatePresetHotkeys(bool a_allowed)
	{
		auto& session = *_current;
		bool  enabled = (session.flags & FUCK::CameraSessionFlags::kPresetHotkeys) && session.presets.IsOpen();
		bool  ctrl    = FUCK::IsModifierPressed(FUCK::Modifier::kCtrl);

		// Skipped while typing, or while the list is open over an entry, where the same keys assign instead
		bool canLoad = a_allowed && enabled && !_presetHovered && !ImGui::GetIO().WantTextInput;

		for (int number = 1; number <= 9; ++number) {
			bool down = ctrl && FUCK::IsInputDown(static_cast<std::uint32_t>(KEY::kNum1) + number - 1);
			if (down && !session.presetKeysDown[number] && canLoad) {
				if (auto preset = session.presets.FindByHotkey(number)) {
					ApplyPreset(*preset);
				}
			}
			session.presetKeysDown[number] = down;
		}
	}

	bool Manager::DrawPresets(FUCK::ICameraClient* a_client, float a_width)
	{
		if (!IsActive(a_client) || !_current->presets.IsOpen())
			return false;

		auto& session = *_current;

		float width   = a_width > 0.0f ? a_width : FUCK::GetContentRegionAvail().x;
		bool  loaded  = false;
		auto  spacing = FUCK::GetStyleVarVec(ImGuiStyleVar_ItemSpacing).x;
		auto  padding = FUCK::GetStyleVarVec(ImGuiStyleVar_FramePadding).x * 2.0f;

		// Styled buttons come out wider than their text and padding, so lay out from each one's width as last drawn
		auto buttonWidth = [&](const char* a_label, float a_measured) {
			return a_measured > 0.0f ? a_measured : FUCK::CalcTextSize(a_label).x + padding;
		};

		FUCK::PushID("FUCKCameraPresets");

		const auto& all = session.presets.GetAll();
		if (all.empty()) {
			FUCK::TextDisabled("$FUCK_Camera_PresetNone"_T);
		} else {
			std::vector<std::string>   names;
			std::vector<std::uint32_t> badges;
			int                        current = -1;

			names.reserve(all.size());
			badges.reserve(all.size());
			for (const auto& preset : all) {
				if (preset.name == session.selectedPreset)
					current = static_cast<int>(names.size());

				names.push_back(preset.name);
				badges.push_back(preset.hotkey > 0 ? static_cast<std::uint32_t>(KEY::kNum1) + preset.hotkey - 1 : 0);
			}

			const char* deleteLabel = "$FUCK_Camera_PresetDelete"_T;
			float       deleteW     = buttonWidth(deleteLabel, _deleteButtonW);

			int hovered = -1;
			FUCK::SetNextItemWidth(std::max(width - deleteW - spacing, 1.0f));
			if (ImGui::ComboWithFilter("##Presets", &current, names, -1, nullptr, nullptr, badges, &hovered) && current >= 0) {
				loaded = ApplyPreset(all[current]);
			}

			bool hotkeys = session.flags & FUCK::CameraSessionFlags::kPresetHotkeys;
			if (hotkeys && hovered < 0 && FUCK::IsItemHovered(0)) {
				FUCK::SetTooltip("$FUCK_Camera_PresetHotkeysTT"_T);
			}

			// Ctrl+1-9 on the entry under the mouse gives it that shortcut, and Ctrl+0 clears it
			_presetHovered = hotkeys && hovered >= 0;
			if (_presetHovered && ImGui::GetIO().KeyCtrl) {
				for (int number = 0; number <= 9; ++number) {
					if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_0 + number), false)) {
						session.presets.SetHotkey(names[hovered], number);
					}
				}
			}

			FUCK::SameLine(0.0f, spacing);
			FUCK::BeginDisabled(current < 0);
			bool deletePressed = FUCK::Button(deleteLabel);
			_deleteButtonW     = FUCK::GetItemRectSize().x;
			FUCK::EndDisabled();

			if (deletePressed && current >= 0) {
				DeletePreset(a_client, names[current].c_str());
			}
		}

		FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));

		const char* saveLabel = "$FUCK_Camera_PresetSave"_T;
		float       saveW     = buttonWidth(saveLabel, _saveButtonW);

		FUCK::SetNextItemWidth(std::max(width - saveW - spacing, 1.0f));
		bool save = FUCK::InputText("##PresetName", session.presetName, sizeof(session.presetName), ImGuiInputTextFlags_EnterReturnsTrue);
		FUCK::SameLine(0.0f, spacing);
		save |= FUCK::Button(saveLabel);
		_saveButtonW = FUCK::GetItemRectSize().x;

		if (save && SavePreset(a_client, session.presetName, session.saveSubject)) {
			session.presetName[0] = '\0';
		}

		if (session.base == FUCK::CameraBase::kFree) {
			FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(2.0f)));
			FUCK::Checkbox("$FUCK_Camera_PresetSubject"_T, &session.saveSubject, true, true);
		}

		FUCK::PopID();
		return loaded;
	}

	// ==================================================
	// Applets
	// ==================================================

	bool Manager::DrawSettings(FUCK::CameraSettingsFlags a_flags)
	{
		constexpr int TAB_SET_SELECTED = 2;  // ImGuiTabItemFlags_SetSelected

		bool changed   = false;
		bool isGamepad = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;

		const Config def;

		if (a_flags & FUCK::CameraSettingsFlags::kSpeeds) {
			// Open on the tab for whichever device is in use
			bool selectTab = a_flags & FUCK::CameraSettingsFlags::kSelectDeviceTab;
			int  flagsKBM  = (selectTab && !isGamepad) ? TAB_SET_SELECTED : 0;
			int  flagsGP   = (selectTab && isGamepad) ? TAB_SET_SELECTED : 0;

			ImVec2 tabRowPos   = FUCK::GetCursorPos();
			float  tabRowWidth = FUCK::GetContentRegionAvail().x;

			if (FUCK::BeginTabBar("FUCKCameraSettingsTabs", 0)) {
				// Keyboard & Mouse Tab
				if (!isGamepad) {
					if (FUCK::BeginTabItem("$FUCK_Camera_TabKBM"_T, flagsKBM)) {
						FUCK::PushID("KBM");
						FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

						changed |= SpeedSlider("$FUCK_Camera_SpeedPan"_T,     &_config.kbmPanSpeed,     def.kbmPanSpeed);
						changed |= SpeedSlider("$FUCK_Camera_SpeedOrbit"_T,   &_config.kbmRotSpeed,     def.kbmRotSpeed);
						changed |= SpeedSlider("$FUCK_Camera_SpeedRoll"_T,    &_config.kbmRollSpeed,    def.kbmRollSpeed);
						changed |= SpeedSlider("$FUCK_Camera_SpeedFovZoom"_T, &_config.kbmZoomFovSpeed, def.kbmZoomFovSpeed);

						FUCK::PopID();
						FUCK::EndTabItem();
					}
				}

				// Gamepad Tab
				if (FUCK::BeginTabItem("$FUCK_Camera_TabGP"_T, flagsGP)) {
					FUCK::PushID("GP");
					FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));

					changed |= SpeedSlider("$FUCK_Camera_SpeedPan"_T,       &_config.gpPanSpeed,     def.gpPanSpeed);
					changed |= SpeedSlider("$FUCK_Camera_SpeedZoom"_T,      &_config.gpZoomSpeed,    def.gpZoomSpeed);
					changed |= SpeedSlider("$FUCK_Camera_SpeedOrbit"_T,     &_config.gpRotSpeed,     def.gpRotSpeed);
					changed |= SpeedSlider("$FUCK_Camera_SpeedRoll"_T,      &_config.gpRollSpeed,    def.gpRollSpeed);
					changed |= SpeedSlider("$FUCK_Camera_SpeedFov"_T,       &_config.gpFovSpeed,     def.gpFovSpeed);
					changed |= SpeedSlider("$FUCK_Camera_SpeedCharacter"_T, &_config.gpCharRotSpeed, def.gpCharRotSpeed);

					changed |= FUCK::ScaledSliderFloat("$FUCK_Camera_Deadzone"_T, &_config.gpDeadzone, 0.0f, 0.5f, 40.0f);

					FUCK::PopID();
					FUCK::EndTabItem();
				}
				FUCK::EndTabBar();
			}

			if (_config.HasCustomSpeeds()) {
				static float s_resetWidth = 0.0f;

				ImVec2 afterTabs  = FUCK::GetCursorPos();
				float  resetWidth = s_resetWidth > 0.0f ? s_resetWidth : FUCK::CalcTextSize("$FUCK_Settings_Reset"_T).x + FUCK::GetStyleVarVec(ImGuiStyleVar_FramePadding).x * 2.0f;

				FUCK::SetCursorPos(ImVec2(tabRowPos.x + tabRowWidth - resetWidth - FUCK::Scale(30.0f), tabRowPos.y - FUCK::Scale(3.0f)));

				FUCK::PushID("FUCKCameraSpeedsReset");
				bool resetPressed = FUCK::Button("$FUCK_Settings_Reset"_T);
				FUCK::PopID();
				s_resetWidth = FUCK::GetItemRectSize().x;

				if (resetPressed) {
					// Only the speeds, as the cinematic toggle sits outside the tabs
					bool cinematic    = _config.cinematic;
					_config           = def;
					_config.cinematic = cinematic;
					changed           = true;
				}

				FUCK::SetCursorPos(afterTabs);
			}

			FUCK::Dummy(ImVec2(0.0f, FUCK::Scale(4.0f)));
		}

		changed |= FUCK::Checkbox("$FUCK_Camera_Cinematic"_T, &_config.cinematic, true, true);

		if (changed) {
			SaveSettings();
		}

		return changed;
	}

	void Manager::DrawHelpMarker(FUCK::CameraSessionFlags a_flags, const char* a_extraKBM, const char* a_extraGP)
	{
		// Goes by the caller's flags rather than the running session, as settings pages draw this with no session at all
		bool isGamepad = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
		bool keyboard  = !(a_flags & FUCK::CameraSessionFlags::kNoKeyboard);

		const char* controls = isGamepad ? "$FUCK_Camera_Help_GP"_T : keyboard ? "$FUCK_Camera_Help_KBM"_T : "$FUCK_Camera_Help_Mouse"_T;
		const char* extra     = isGamepad ? a_extraGP : a_extraKBM;

		if (!extra || extra[0] == '\0') {
			FUCK::HelpMarker(controls);
			return;
		}

		std::string text = controls;
		text += "\n\n";
		text += extra;
		FUCK::HelpMarker(text.c_str());
	}
}
