#pragma once

#include "Camera/Maths.h"

namespace Camera
{
	struct Config
	{
		float kbmPanSpeed     = 8.0f;
		float kbmRotSpeed     = 1.5f;
		float kbmRollSpeed    = 1.5f;
		float kbmZoomFovSpeed = 30.0f;
		float kbmCharRotSpeed = 0.005f;  // radians of right-drag turn per pixel

		float gpPanSpeed     = 100.0f;
		float gpZoomSpeed    = 100.0f;
		float gpRotSpeed     = 2.0f;
		float gpRollSpeed    = 2.0f;
		float gpFovSpeed     = 30.0f;
		float gpCharRotSpeed = 3.0f;
		float gpDeadzone     = 0.25f;

		bool cinematic   = true;
		bool invertTilt  = false;  // tilting up and down the other way round, on every device
		bool invertTurn  = false;  // turning the subject the other way round, on every device
		bool invertOrbit = false;  // orbiting round the subject the other way round, on every device
		bool zoomToFocus = false;  // zooming in heads for the point on the subject nearest the cursor, rather than straight ahead
		bool turnCoast   = true;   // a flicked right-drag turn carries on and eases to a stop after release
		bool orbitHead   = true;   // a framed body orbits and zooms on the head rather than its middle
		bool followHead  = true;   // zoomed in on a framed body, the camera moves with the live head

		bool HasCustomSpeeds() const;
	};

	// Points on the subject that zoomToFocus can head for
	enum class FocusPoint : std::uint32_t
	{
		kHead,
		kChest,
		kPelvis,
		kFeet,
		kTotal
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

		// A point on the subject for zoomToFocus, or none to zoom straight ahead
		virtual std::optional<RE::NiPoint3> GetFocusPoint(FocusPoint a_point) = 0;

		// Where the subject stands, which body framing is kept relative to
		virtual std::optional<RE::NiPoint3> GetAnchor() = 0;

		// The head mesh's radius round the head node, or 0 when there's no head to measure
		virtual float GetHeadRadius() = 0;

		// Whether the body points are one part's (SetPartFraming), which moves round the subject as they turn
		virtual bool FramesPart() = 0;

		// Which way the subject faces, so a framed part follows them as they're turned but not as it sways in an idle
		virtual std::optional<float> GetFacing() = 0;

		// Asked only when a scroll is about to zoom, as the check can be costly
		virtual bool IsScrollBlocked() = 0;

		virtual void RotateSubject(float a_amount) = 0;

		// Turns the subject back to how it faced as the session began
		virtual void ResetSubject() = 0;
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
		void Move(const FUCK::CameraMoveEx& a_move, const Config& a_settings, bool a_isGamepad, float a_mult, float a_interval);

		// The base camera moved by the current offsets and framing. Returns false, leaving the base as is, if there are none
		bool Compose(const RE::NiPoint3& a_baseTranslate, const RE::NiMatrix3& a_baseRotate, RE::NiPoint3& a_outTranslate, RE::NiMatrix3& a_outRotate);

		// Restores the FOV and clears everything
		void Reset();

		// The reset gesture: back to the starting view, framed or not as it began, easing there when cinematic
		void ResetMoves();

		// Clears offsets and framing at once, keeping only roll and FOV, for when the base itself has been moved
		void SnapTo(float a_roll, float a_fov);

		// Offset bases: the offsets putting the camera at a pose (in the base's space) from this base, unframed and landed at once. With
		// a_easeToStart it then eases into the session's start: its starting FOV, and its framing where it starts framed
		void SolvePose(const RE::NiPoint3& a_baseTranslate, const RE::NiMatrix3& a_baseRotate, const RE::NiPoint3& a_translate, const RE::NiMatrix3& a_rotate, float a_fov, bool a_easeToStart = false);

		// Lands on the target offsets and finishes any framing swoop at once, e.g. for a session opening where it was left
		void Settle();

		// The same, without moving where the session began (what Reset goes back to), for a cut the client asks for
		void Land();

		// Frames the body again, e.g. for another subject, at once or easing over
		void Reframe(bool a_instant);

		// Eases away the orbit, pan, zoom, tilt, roll and FOV, which would otherwise carry a newly framed part off where framing places it
		void ClearMovesForFraming();
		void RequestSideView(bool a_side) { _sideViewPending = a_side; }

		// Framing kept where it is, not following the part or pose as they change, nor the base camera's turn, then framed again easing
		// over once let go
		void HoldFraming(bool a_held);
		void KeepView(bool a_keep);
		bool KeepsView() const { return _keepView; }
		// A newly framed part is eased onto as one move, the framing and the eased-away moves together
		void EasePartChange();

		// The FOV the session opens at, and the reset gesture goes back to, rather than the player's
		void  SetStartFov(float a_fov);
		float GetStartFov() const { return _startFovOffset != 0.0f ? _baseFov + _startFovOffset : 0.0f; }
		float GetAppliedFov() const { return _baseFov > 0.0f ? _baseFov + _appliedFovOffset : 0.0f; }  // the FOV last given to the game, 0 before any

		// Puts the player's FOV back for the session taking over, and keeps the offsets to carry on from when resumed
		void Pause();

		// Sets the session's FOV again, for after the game's update, where another mod may have changed it
		void ApplyFov();
		void SetFovModifier(float a_offset);

		void SetZoomAcceleration(bool a_enabled) { _zoomAccelEnabled = a_enabled; }
		void SetKeyboardEnabled(bool a_enabled) { _keyboardEnabled = a_enabled; }
		void SetNoModifier(bool a_enabled) { _noModifier = a_enabled; }
		void SetFollowPose(bool a_enabled) { _followPose = a_enabled; }
		void SetTurningEnabled(bool a_enabled) { _turningEnabled = a_enabled; }

		void SetBodyFraming(bool a_framed, const FUCK::CameraScreenRect& a_rect, float a_margin);
		bool IsBodyFramed() const { return _isFramed; }
		bool GetBodyFraming(FUCK::CameraScreenRect& a_rect, float& a_margin) const;

		// Eases to the offsets when cinematic, as the controls do
		Offsets GetTargetOffsets() const;
		void    SetTargetOffsets(const Offsets& a_offsets);

		void AccumulateScroll(float a_delta) { _pendingScroll += a_delta; }
		void AccumulateDrag(const ImVec2& a_delta) { _pendingDrag.x += a_delta.x; _pendingDrag.y += a_delta.y; }
		bool IsDragging() const { return _isDragging; }

		bool HasAnyCamera() const { return IsModified() || _framingBlend > 0.001f; }
		bool IsModified() const;
		bool IsFovChanged() const { return std::abs(_targetFovOffset - _startFovOffset) > 0.01f; }  // the player has changed the FOV from where the session began

		static bool GetCurrentFov(float& a_horizontal, float& a_vertical);
		static float GetScreenFov();  // worldFOV with the game's modifier, the FOV on screen
		static float Tilt(const Config& a_settings);
		static float Orbit(const Config& a_settings);

		// The FOV the last frame was drawn at, which the view frustum is built for till the game's next camera update. Noted each frame, so
		// a session beginning before then (one swapped for another) measures its framing against the frustum as it is
		static void NoteDrawnFov() { s_drawnFov = GetScreenFov(); }

	private:
		static inline float s_drawnFov = 0.0f;

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

			ImVec2 mouseDelta{ 0.0f, 0.0f };  // the mouse's own movement this frame, which doesn't stop at the screen's edge

			SpeedScales scale;
		};

		FrameInput GatherInput(const FUCK::CameraControlState& a_state) const;

		void UpdateFramingBlend(float a_interval);
		void UpdatePartEase(float a_interval);
		void FollowTurnedPart();
		RE::NiPoint3 FramedPivot(const Maths::BodyPoints& a_body);
		void ClearInputState();
		void HandleControls(const FUCK::CameraControlState& a_state, const Config& a_settings, float a_interval);
		void HandleMouse(const FrameInput& a_in, const Config& a_settings, float a_interval);
		void HandleKeyboard(const FrameInput& a_in, const Config& a_settings, float a_interval);
		void HandleGamepad(const FrameInput& a_in, const Config& a_settings, float a_interval);
		void HandleSubjectRotation(const FrameInput& a_in, const Config& a_settings, float a_interval);
		bool HandleResetGesture(const FrameInput& a_in, float a_interval);
		void UpdateCinematic(bool a_enabled, float a_interval);
		void FollowHandover();
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
		RE::NiMatrix3 HeldRotate(const RE::NiMatrix3& a_baseRotate);
		void        GetFramedBase(const RE::NiPoint3& a_baseTranslate, const std::optional<RE::NiPoint3>& a_hostPivot, RE::NiPoint3& a_pivot, RE::NiPoint3& a_camBase) const;
		bool        GetTargetView(RE::NiPoint3& a_translate, RE::NiMatrix3& a_rotate, RE::NiPoint3* a_subject = nullptr);
		void        Zoom(float a_amount, bool a_atCursor);
		void        ClearFocus();
		float       HeadClearance();
		bool        PivotsOnHead(bool a_pair) const;
		RE::NiPoint3 HeadFollow(const RE::NiPoint3& a_anchor) const;
		void        KeepOutOfHead(RE::NiPoint3& a_translate, const RE::NiMatrix3& a_rotate);

		std::optional<FocusPoint> PickFocus(const RE::NiPoint3& a_translate, const RE::NiMatrix3& a_rotate, bool a_atCursor);

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

		float _baseFov      = 0.0f;  // the FOV on screen as the session began
		float _baseModifier = 0.0f;  // the game's FOV modifier then, restored on reset
		float _baseControlZ = 0.0f;  // and its camera's FOV control

		float _startFovOffset   = 0.0f;  // where the FOV starts (SetStartFov), from the player's
		float _appliedFovOffset = 0.0f;  // the FOV offset last given to the game, which the view in use is at

		// A handover easing into framing (SolvePose with a_easeToStart): the offsets and FOV it began at, which give way as the framing comes in
		struct Handover
		{
			RE::NiPoint3 offset;
			float        rotZ = 0.0f;
			float        rotX = 0.0f;
			float        roll = 0.0f;
			float        fov  = 0.0f;
		};
		std::optional<Handover> _handover;

		float                  _framingProgress   = 0.0f;  // linear swoop progress, eased into _framingBlend
		float                  _framingBlend      = 0.0f;  // 0 = base view, 1 = body
		float                  _framingDistance   = 0.0f;
		bool                   _isFramed          = false;
		bool                   _framingHeld       = false;  // HoldFraming()
		bool                   _keepView          = false;  // KeepView(): framing starts from the view as it is, and the base's turn is held
		bool                   _keepViewPending   = false;  // KeepView(): the next framing measures how far it is from the view
		RE::NiPoint3           _framingAdjust;              // KeepView(): added to every framing, so the first lands where the camera is
		RE::NiPoint3           _lastOutTranslate;           // where Compose() last put the camera, before the orbit and moves
		bool                   _hasLastOut        = false;
		bool                   _hasHeldRotate     = false;
		float                  _heldRotateEase    = 1.0f;   // 0 to 1 through the turn back once let go
		RE::NiMatrix3          _heldRotate;
		bool                   _hasFraming        = false;
		bool                   _sideViewPending   = false;  // a framed part that reads flat from the front: orbit to its side once measured
		FUCK::CameraScreenRect _framingRect;
		float                  _framingMargin     = 1.0f;
		int                    _framingSettleFrames = 0;
		RE::NiPoint3           _framingCamDelta  { 0.0f, 0.0f, 0.0f };
		RE::NiPoint3           _framingPivotDelta{ 0.0f, 0.0f, 0.0f };

		// kFollowPose: what the framing eases to when the pose changes (see FollowFramedPose), and the body height it was taken at
		bool                   _followPose        = false;
		RE::NiPoint3           _framingCamDeltaTo  { 0.0f, 0.0f, 0.0f };
		RE::NiPoint3           _framingPivotDeltaTo{ 0.0f, 0.0f, 0.0f };
		std::optional<float>   _framingFacing;  // the subject's facing as framed, which a framed part follows only when it changes
		float                  _framingDistanceTo = 0.0f;

		// EasePartChange(): how far through, and where the framing and the moves set out from
		float                  _partEase = 1.0f;
		RE::NiPoint3           _easeFromCam{ 0.0f, 0.0f, 0.0f };
		RE::NiPoint3           _easeFromPivot{ 0.0f, 0.0f, 0.0f };
		RE::NiPoint3           _easeFromOffset{ 0.0f, 0.0f, 0.0f };
		float                  _easeFromDistance = 0.0f;
		float                  _easeFromRotZ     = 0.0f;
		float                  _easeFromRotX     = 0.0f;
		float                  _easeFromRoll     = 0.0f;
		float                  _framingHeight     = 0.0f;
		float                  _poseHeight        = 0.0f;  // kFollowPose: the body's height last frame
		float                  _poseStillFor      = 0.0f;  // kFollowPose: seconds it's held still
		RE::NiPoint3           _partLast{};               // kFollowPose: a framed part's place last frame, from the anchor
		float                  _partStillFor      = 0.0f;  // kFollowPose: seconds it's been at rest
		float                  _framingSpread     = 0.0f;  // the space between two framed together, as framed
		float                  _cameraDistance    = 0.0f;  // camera to pivot as last composed, which speeds scale by
		bool                   _startedFramed     = false; // the session opened framed, which the reset gesture goes back to
		// The moves made before framing the body, which turning it off goes back to
		RE::NiPoint3 _unframedOffset{ 0.0f, 0.0f, 0.0f };
		float        _unframedRotZ = 0.0f;
		float        _unframedRotX = 0.0f;
		float _pendingScroll    = 0.0f;
		float _smoothScrollZoom = 0.0f;
		float _smoothScrollFov  = 0.0f;

		ImVec2 _pendingDrag{ 0.0f, 0.0f };
		bool   _isDragging = false;
		// zoomToFocus: the point zooming in heads for, how far it's zoomed in towards it, and the sideways shift that brought it to the middle,
		// which zooming back out takes off again
		bool                      _zoomToFocus = false;
		std::optional<FocusPoint> _focus;
		float                     _focusDepth  = 0.0f;
		float                     _focusShiftX = 0.0f;
		float                     _focusShiftZ = 0.0f;
		float                     _sinceZoomIn = 0.0f;
		float                     _focusBlend  = 0.0f;  // 0 = speeds go by the pivot's distance, 1 = by the focus point's

		float _kbmAcceleration  = 1.0f;
		bool  _lockoutGPSubjRot = false;
		bool  _wasMouseSubjRot  = false;
		bool  _orbitHead        = true;
		bool  _followHead       = true;
		RE::NiPoint3 _headFollow{ 0.0f, 0.0f, 0.0f };  // followHead: eased, so a pose that snaps (a weapon swap) doesn't jerk the camera
		float _turnSpeed        = 0.0f;  // turnCoast: radians a second the drag was turning at
		float _turnCoast        = 0.0f;  // turnCoast: radians a second still carried after release

		// The reset gesture: a double middle click, or RB + R3
		float _sinceMiddleClick  = 0.0f;  // seconds since middle mouse last went down
		float _middleClickTravel = 0.0f;  // how far the mouse has moved since
		bool  _wasMiddleDown     = false;
		bool  _wasR3Down         = false;

		bool  _zoomAccelEnabled = false;
		bool  _keyboardEnabled  = true;
		bool  _noModifier       = false;  // kNoModifier: the keys and wheel without Ctrl
		bool  _turningEnabled   = true;
		float _zoomAcceleration = 1.0f;
		bool  _isZooming        = false;  // set by this frame's zoom input, and read once all of the frame's input is handled

		RE::NiPoint3  _originalTranslate;
		RE::NiMatrix3 _originalRotate;
	};
}
