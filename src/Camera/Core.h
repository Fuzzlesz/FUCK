#pragma once

#include "Camera/Maths.h"

namespace Camera
{
	struct Config
	{
		float kbmPanSpeed     = 15.0f;
		float kbmRotSpeed     = 1.5f;
		float kbmRollSpeed    = 1.5f;
		float kbmZoomFovSpeed = 30.0f;

		float gpPanSpeed     = 100.0f;
		float gpZoomSpeed    = 100.0f;
		float gpRotSpeed     = 2.0f;
		float gpRollSpeed    = 2.0f;
		float gpFovSpeed     = 30.0f;
		float gpCharRotSpeed = 3.0f;
		float gpDeadzone     = 0.25f;

		bool cinematic = true;

		bool HasCustomSpeeds() const;
	};

	// What the core needs from the camera it's moving and the subject it's looking at
	class Host
	{
	public:
		virtual ~Host() = default;

		// The camera as the game placed it this frame, before any offsets
		virtual bool GetBasePose(RE::NiPoint3& a_translate, RE::NiMatrix3& a_rotate) = 0;

		// Orbit centre, which zoom speed is also measured against
		virtual std::optional<RE::NiPoint3> GetPivot() = 0;

		// The body's top, bottom and centre, for body framing
		virtual bool GetBodyPoints(Maths::BodyPoints& a_body) = 0;

		// Asked only when a scroll is about to zoom, as the check can be costly
		virtual bool IsScrollBlocked() = 0;

		virtual void RotateSubject(float a_amount) = 0;
	};

	// Where the offsets are heading, for presets
	struct Offsets
	{
		RE::NiPoint3 translate{ 0.0f, 0.0f, 0.0f };
		float        orbit = 0.0f;
		float        pitch = 0.0f;
		float        roll  = 0.0f;
		float        fov   = 0.0f;  // absolute, 0 when not known yet
	};

	class Core
	{
	public:
		explicit Core(Host& a_host) :
			_host(a_host)
		{}

		void Update(const FUCK::CameraControlState& a_state, const Config& a_settings, float a_interval);

		// Moves the camera at the device's speed settings, scaled by distance and FOV like the built-in controls.
		// Call before Update, as Update settles this frame's movement
		void Move(const FUCK::CameraMove& a_move, const Config& a_settings, bool a_isGamepad, float a_mult, float a_interval);

		// The base camera moved by the current offsets and framing. Returns false, leaving the base as is, if there are none
		bool Compose(const RE::NiPoint3& a_baseTranslate, const RE::NiMatrix3& a_baseRotate, RE::NiPoint3& a_outTranslate, RE::NiMatrix3& a_outRotate);

		// Restores the FOV and clears everything
		void Reset();

		// Clears offsets and framing at once, keeping only roll and FOV, for when the base itself has been moved
		void SnapTo(float a_roll, float a_fov);

		// Lands on the target offsets and finishes any framing swoop at once, e.g. for a session opening where it was left
		void Settle();

		// Puts the player's FOV back for the session taking over, and keeps the offsets to carry on from when resumed
		void Pause();

		// Sets the session's FOV again, for after the game's update, where another mod may have changed it
		void ApplyFov();

		void SetZoomAcceleration(bool a_enabled) { _zoomAccelEnabled = a_enabled; }
		void SetKeyboardEnabled(bool a_enabled) { _keyboardEnabled = a_enabled; }
		void SetFollowPose(bool a_enabled) { _followPose = a_enabled; }
		void SetTurningEnabled(bool a_enabled) { _turningEnabled = a_enabled; }

		void SetBodyFraming(bool a_framed, const FUCK::CameraScreenRect& a_rect, float a_margin);
		bool IsBodyFramed() const { return _isFramed; }
		bool GetBodyFraming(FUCK::CameraScreenRect& a_rect, float& a_margin) const;

		// Eases to the offsets when cinematic, as the controls do
		Offsets GetTargetOffsets() const;
		void    SetTargetOffsets(const Offsets& a_offsets);

		void AccumulateScroll(float a_delta) { _pendingScroll += a_delta; }

		bool HasAnyCamera() const { return IsModified() || _framingBlend > 0.001f; }
		bool IsModified() const;
		bool IsFovChanged() const { return std::abs(_targetFovOffset) > 0.01f; }  // the player has changed the FOV from where the session began

		static bool GetCurrentFov(float& a_horizontal, float& a_vertical);

	private:
		struct SpeedScales
		{
			float pan  = 1.0f;
			float zoom = 1.0f;
			float rot  = 1.0f;
		};

		struct FrameInput
		{
			bool isGamepad    = false;
			bool isGlobalKBM  = false;
			bool isGlobalGP   = false;
			bool isCameraMode = false;

			bool ctrlDown  = false;
			bool shiftDown = false;
			bool altDown   = false;
			bool lbDown    = false;
			bool rbDown    = false;
			bool mmbDown   = false;
			bool rmbDown   = false;

			bool isPopupOpen  = false;
			bool isUIHovered  = false;
			bool isItemActive = false;
			bool mouseLocked  = false;

			SpeedScales scale;
		};

		FrameInput GatherInput(const FUCK::CameraControlState& a_state) const;

		void UpdateFramingBlend(float a_interval);
		void ClearInputState();
		void HandleControls(const FUCK::CameraControlState& a_state, const Config& a_settings, float a_interval);
		void HandleMouse(const FrameInput& a_in, const Config& a_settings, float a_interval);
		void HandleKeyboard(const FrameInput& a_in, const Config& a_settings, float a_interval);
		void HandleGamepad(const FrameInput& a_in, const Config& a_settings, float a_interval);
		void HandleSubjectRotation(const FrameInput& a_in, const Config& a_settings, float a_interval);
		void UpdateCinematic(bool a_enabled, float a_interval);
		void CaptureBaseFov();
		void ClearMotion();
		void ClampPitch();

		float       GetFovScale() const;
		float       GetDistanceScale();
		SpeedScales GetSpeedScales();
		void        UpdateKbmAcceleration(bool a_isTranslating, float a_interval);
		void        UpdateZoomAcceleration(float a_interval);
		void        UpdateFraming(const RE::NiPoint3& a_pivot, bool a_ease = false);
		void        FollowFramedPose(float a_interval);

		Host& _host;

		RE::NiPoint3 _camOffset{ 0.0f, 0.0f, 0.0f };
		float        _camRotZ   = 0.0f;
		float        _camRotX   = 0.0f;
		float        _camRoll   = 0.0f;
		float        _fovOffset = 0.0f;

		RE::NiPoint3 _targetCamOffset{ 0.0f, 0.0f, 0.0f };
		float        _targetCamRotZ   = 0.0f;
		float        _targetCamRotX   = 0.0f;
		float        _targetCamRoll   = 0.0f;
		float        _targetFovOffset = 0.0f;

		float _baseFov = 0.0f;  // player's FOV, restored on reset

		float                  _framingProgress   = 0.0f;  // linear swoop progress, eased into _framingBlend
		float                  _framingBlend      = 0.0f;  // 0 = base view, 1 = body
		float                  _framingDistance   = 0.0f;
		bool                   _isFramed          = false;
		bool                   _hasFraming        = false;
		FUCK::CameraScreenRect _framingRect;
		float                  _framingMargin     = 1.0f;
		int                    _framingSettleFrames = 0;
		RE::NiPoint3           _framingCamDelta  { 0.0f, 0.0f, 0.0f };
		RE::NiPoint3           _framingPivotDelta{ 0.0f, 0.0f, 0.0f };

		// kFollowPose: what the framing eases to when the pose changes (see FollowFramedPose), and the body height it was taken at
		bool                   _followPose        = false;
		RE::NiPoint3           _framingCamDeltaTo  { 0.0f, 0.0f, 0.0f };
		RE::NiPoint3           _framingPivotDeltaTo{ 0.0f, 0.0f, 0.0f };
		float                  _framingDistanceTo = 0.0f;
		float                  _framingHeight     = 0.0f;
		float                  _poseHeight        = 0.0f;  // kFollowPose: the body's height last frame
		float                  _poseStillFor      = 0.0f;  // kFollowPose: seconds it's held still
		// The moves made before framing the body, which turning it off goes back to
		RE::NiPoint3 _unframedOffset{ 0.0f, 0.0f, 0.0f };
		float        _unframedRotZ = 0.0f;
		float        _unframedRotX = 0.0f;
		float _pendingScroll    = 0.0f;
		float _smoothScrollZoom = 0.0f;
		float _smoothScrollFov  = 0.0f;

		float _kbmAcceleration  = 1.0f;
		bool  _lockoutGPSubjRot = false;
		bool  _wasMouseSubjRot  = false;
		float _pendingSubjectTurn = 0.0f;  // the part of a turn still to ease in

		bool  _zoomAccelEnabled = false;
		bool  _keyboardEnabled  = true;
		bool  _turningEnabled   = true;
		float _zoomAcceleration = 1.0f;
		bool  _isZooming        = false;  // set by this frame's zoom input, and read once all of the frame's input is handled

		RE::NiPoint3  _originalTranslate;
		RE::NiMatrix3 _originalRotate;
	};
}
