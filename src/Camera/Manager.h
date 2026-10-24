#pragma once

#include "Camera/Core.h"
#include "Camera/Presets.h"

namespace Camera
{
	// kOffset and kThirdPerson both move the game's camera root by offsets, so most code treats them alike
	inline bool IsOffsetBase(FUCK::CameraBase a_base) { return a_base != FUCK::CameraBase::kFree; }

	// One client's session, kept intact while it's paused beneath a newer one
	struct Session
	{
		explicit Session(Host& a_host) :
			core(a_host)
		{}

		Core core;

		FUCK::ICameraClient*     client = nullptr;
		std::string              context;
		FUCK::CameraBase         base  = FUCK::CameraBase::kOffset;
		FUCK::CameraSessionFlags flags = FUCK::CameraSessionFlags::kNone;
		RE::ActorHandle          subject;
		bool                     hasSubject = false;

		bool            paused        = false;
		RE::CameraState startingState = RE::CameraState::kThirdPerson;  // the camera it began over, which must be back before it resumes

		// Offset bases: the node's transform before and after this frame's offsets, so it can be put back
		RE::NiPoint3  nodeOriginalTranslate;
		RE::NiMatrix3 nodeOriginalRotate;
		RE::NiPoint3  nodeAppliedTranslate;
		RE::NiMatrix3 nodeAppliedRotate;
		bool          nodeModified = false;

		// kFree: this frame's pose before and after FUCK's moves, worked out once per update
		struct FreePose
		{
			RE::NiPoint3     translate;
			RE::NiMatrix3    rotate;
			RE::NiQuaternion quat;
			bool             valid = false;
		};
		FreePose freeBase;
		FreePose freeFinal;

		// kThirdPerson: the camera as the session began, used as the base rather than wherever the game puts it, as that follows the subject's heading
		RE::NiPoint3  fixedTranslate;
		RE::NiMatrix3 fixedRotate;
		RE::NiPoint3  fixedSubjectPos;  // where the subject stood then, so the held camera moves with it (see HeldTranslate)
		bool          hasFixedBase = false;
		bool          turnsHeading = false;  // see TurnsHeading()

		// kThirdPerson: the facing FUCK turned the subject to, put back on resume in case another session turned it, and held while paused
		std::optional<float> subjectYaw;
		// Where the subject faced as the session began, which Reset turns it back to
		std::optional<float> startSubjectYaw;

		Presets              presets;
		std::string          selectedPreset;
		char                 presetName[64] = {};
		bool                 saveSubject    = true;
		std::array<bool, 10> presetKeysDown{};

		RE::CameraState previousCameraState = RE::CameraState::kThirdPerson;
		bool            enteredFreeCamera   = false;
		bool            disabledSaving      = false;
	};

	// Owns the camera sessions, the offset and free-camera bases, the camera's settings and its applets.
	// Sessions stack: a kResumable session taken over waits beneath the newer one, and only the top one runs
	class Manager final :
		public REX::Singleton<Manager>,
		public Host
	{
	public:
		static void InstallHooks();
		static void InstallLateHooks();

		// SmoothCam's API arrives through SKSE messages, so ask for it as plugins load
		static void RegisterSmoothCam();
		static void RequestSmoothCam();

		void LoadSettings();
		void SaveSettings() const;

		bool BeginSession(const FUCK::CameraSessionOptions* a_options);
		void EndSession(FUCK::ICameraClient* a_client);
		bool IsActive(const FUCK::ICameraClient* a_client) const { return a_client && _current && _current->client == a_client; }
		bool IsPaused(const FUCK::ICameraClient* a_client) const;

		// Offset bases: driven by the client around the game's camera update
		void Revert(FUCK::ICameraClient* a_client);
		void Update(FUCK::ICameraClient* a_client, float a_interval);

		void Move(const FUCK::CameraMove* a_move, FUCK::InputDevice a_device, float a_mult);

		void Reset(FUCK::ICameraClient* a_client);
		bool IsModified(const FUCK::ICameraClient* a_client) const;
		void SetBodyFraming(FUCK::ICameraClient* a_client, bool a_framed, const FUCK::CameraScreenRect* a_rect, float a_margin);
		bool IsBodyFramed(const FUCK::ICameraClient* a_client) const;

		// Offset bases only
		bool GetOffsets(const FUCK::ICameraClient* a_client, FUCK::CameraOffsets* a_offsets) const;
		bool SetOffsets(FUCK::ICameraClient* a_client, const FUCK::CameraOffsets* a_offsets);

		// kFree only
		bool GetPose(const FUCK::ICameraClient* a_client, FUCK::CameraPose* a_pose) const;
		bool SetPose(FUCK::ICameraClient* a_client, const FUCK::CameraPose* a_pose);

		// kFree: called from the free camera's own update and pose queries
		void OnFreeCameraUpdate(RE::FreeCameraState* a_state);
		void OverrideFreeRotation(const RE::FreeCameraState* a_state, RE::NiQuaternion& a_rotation) const;
		void OverrideFreeTranslation(const RE::FreeCameraState* a_state, RE::NiPoint3& a_translation) const;

		// kThirdPerson: called after the third-person camera's own update, which only runs while time does
		void OnThirdPersonUpdate(RE::ThirdPersonState* a_state);

		// Presets, kept per session context
		bool        SavePreset(FUCK::ICameraClient* a_client, const char* a_name, bool a_withSubject);
		bool        LoadPreset(FUCK::ICameraClient* a_client, const char* a_name);
		bool        DeletePreset(FUCK::ICameraClient* a_client, const char* a_name);
		int         GetPresetCount(const FUCK::ICameraClient* a_client) const;
		const char* GetPresetName(const FUCK::ICameraClient* a_client, int a_index) const;
		bool        DrawPresets(FUCK::ICameraClient* a_client, float a_width);

		// Each ImGui frame: collects the scroll wheel for zoom to read in the game's frame, and resumes a waiting session
		void OnFrame();

		// Folded into FUCK's own input blocking and pausing. Input is blocked with kBlockControls, or while the camera's controls are held
		bool IsBlockingInput() const;
		// Whether a tap of RB, the camera's gamepad modifier, should reach the game. Not with kBlockControls, as the game then gets nothing
		bool PassesModifierTaps() const { return _current && _inputAllowed && !(_current->flags & FUCK::CameraSessionFlags::kBlockControls); }
		bool IsPausing() const { return _current && (_current->flags & FUCK::CameraSessionFlags::kFreezeTime); }
		bool RunsPhysics() const { return _current && (_current->flags & FUCK::CameraSessionFlags::kRunPhysics); }

		bool DrawSettings(FUCK::CameraSettingsFlags a_flags);
		void DrawHelpMarker(FUCK::CameraSessionFlags a_flags, const char* a_extraKBM, const char* a_extraGP);

	private:
		// Host
		bool                        GetBasePose(RE::NiPoint3& a_translate, RE::NiMatrix3& a_rotate) override;
		std::optional<RE::NiPoint3> GetPivot() override;
		bool                        GetBodyPoints(Maths::BodyPoints& a_body) override;
		void                        MoveBaseToFront();
		bool                        IsScrollBlocked() override;
		void                        RotateSubject(float a_amount) override;

		Session*             Find(const FUCK::ICameraClient* a_client) const;
		RE::Actor*           GetSubject() const;
		std::optional<float> GetSubjectYaw() const;
		RE::FreeCameraState* GetFreeState() const;
		bool                 IsFreeState(const RE::FreeCameraState* a_state) const;
		bool                 IsFree() const { return _current && _current->base == FUCK::CameraBase::kFree; }
		bool                 IsThirdPerson() const { return _current && _current->base == FUCK::CameraBase::kThirdPerson; }

		// The third-person base turns the subject's heading while time runs, as the game moves the 3D back to it then. Paused, and on the
		// other bases, the 3D turns instead, as the head's aim is solved against the heading: it stays put against the body as it turns,
		// rather than swinging round to whatever it was looking at (a merchant, held through the barter menu). Decided as the session begins
		bool TurnsHeading() const { return _current && _current->turnsHeading; }

		void RunControls(float a_interval);
		void ApplyNode();
		void RevertNode();
		void UpdatePresetHotkeys(bool a_allowed);
		bool CapturePreset(bool a_withSubject, Preset& a_preset);
		bool ApplyPreset(const Preset& a_preset);
		void ApplyOffsets(const OffsetPreset& a_offset);
		void SetSubjectYaw(float a_yaw);
		RE::NiPoint3 HeldTranslate() const;
		void HoldSubjectYaw();
		void MoveSubject(const SubjectTransform& a_transform);
		void SetUpStart(const FUCK::CameraSessionOptions& a_options);
		void RememberCamera();

		void EnterBase(Session& a_session);
		void LeaveBase(Session& a_session);
		void PauseCurrent();
		void EndCurrent(bool a_takenOver);
		void Remove(Session& a_session);
		void TryResume();
		bool IsBaseReady(const Session& a_session) const;

		void UpdateSmoothCam();

		Config _config;

		std::vector<std::unique_ptr<Session>> _sessions;            // oldest first. The last is current unless it's waiting to resume
		Session*                              _current = nullptr;  // the running session, if any

		bool  _isUpdating     = false;  // inside the client's OnCameraUpdate(), the only time MoveCamera() applies
		float _updateInterval = 0.0f;

		// The client's last control state, read by the input hook between frames
		bool _inputAllowed  = true;
		bool _mouseLocked   = false;
		bool _cameraMoves   = true;

		// The applet's frame state, shared as only the current session draws it
		bool  _presetHovered = false;  // the list is open over an entry, so Ctrl+1-9 assigns rather than loads
		float _deleteButtonW = 0.0f;
		float _saveButtonW   = 0.0f;

		bool _holdsSmoothCam = false;
	};
}
