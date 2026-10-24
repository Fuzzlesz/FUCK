#include "Camera/Core.h"

#include "System/InputMap.h"

using namespace Camera::Maths;
namespace Keys = Input::Keymap;

namespace
{
	// Tuned against the speed sliders' defaults, so changing one changes how the camera feels for every consumer
	constexpr float MOUSE_WHEEL_MULT    =  0.1f;
	constexpr float MOUSE_MOVE_MULT     =  0.002f;
	constexpr float MOUSE_ROT_MULT      =  0.010f;

	constexpr float PITCH_CLAMP_LIMIT   =  1.55f;
	constexpr float SCROLL_SMOOTH_SPEED = 15.0f;
	constexpr float FRAMING_DURATION    =  0.8f;
	constexpr int   FRAMING_SETTLE_FRAMES = 5;      // how many updates the framing is recalculated for after it's applied instantly, while the subject's pose settles
	constexpr float FRAMING_POSE_CHANGE = 0.05f;    // kFollowPose: how much the body's height must change, as a share, before the framing follows, so idle sway can't
	constexpr float FRAMING_FOLLOW_SPEED = 4.0f;
	constexpr float ACCEL_RATE          =  1.5f;   // per second held, for pan and zoom acceleration
	constexpr float ACCEL_MAX           = 15.0f;
	constexpr float CINEMATIC_SPEED     = 10.0f;
	constexpr float EPSILON             =  0.001f;

	float InputAxis(std::uint32_t a_positive, std::uint32_t a_negative)
	{
		return (FUCK::IsInputDown(a_positive) ? 1.0f : 0.0f) - (FUCK::IsInputDown(a_negative) ? 1.0f : 0.0f);
	}
}

bool Camera::Config::HasCustomSpeeds() const
{
	const Config def;
	auto differs = [](float a, float b) { return std::abs(a - b) > EPSILON; };

	return differs(kbmPanSpeed,     def.kbmPanSpeed)     ||
	       differs(kbmRotSpeed,     def.kbmRotSpeed)     ||
	       differs(kbmRollSpeed,    def.kbmRollSpeed)    ||
	       differs(kbmZoomFovSpeed, def.kbmZoomFovSpeed) ||
	       differs(gpPanSpeed,      def.gpPanSpeed)      ||
	       differs(gpZoomSpeed,     def.gpZoomSpeed)     ||
	       differs(gpRotSpeed,      def.gpRotSpeed)      ||
	       differs(gpRollSpeed,     def.gpRollSpeed)     ||
	       differs(gpFovSpeed,      def.gpFovSpeed)      ||
	       differs(gpCharRotSpeed,  def.gpCharRotSpeed)  ||
	       differs(gpDeadzone,      def.gpDeadzone);
}

namespace Camera
{
	bool Core::IsModified() const
	{
		return std::abs(_camOffset.x) > 0.01f || std::abs(_camOffset.y) > 0.01f || std::abs(_camOffset.z) > 0.01f ||
		       std::abs(_camRotZ) > 0.001f    || std::abs(_camRotX) > 0.001f || std::abs(_camRoll) > 0.001f       ||
		       std::abs(_fovOffset) > 0.01f   || _isFramed;
	}

	bool Core::GetCurrentFov(float& a_horizontal, float& a_vertical)
	{
		auto worldCam = RE::Main::WorldRootCamera();
		if (!worldCam)
			return false;

		const auto& frustum = worldCam->GetRuntimeData2().viewFrustum;
		if (frustum.fTop <= 0.0f || frustum.fRight <= 0.0f)
			return false;

		a_vertical   = FovFromTanHalf(frustum.fTop);
		a_horizontal = FovFromTanHalf(frustum.fRight);
		return true;
	}

	float Core::GetFovScale() const
	{
		if (_baseFov <= 0.0f)
			return 1.0f;

		// Scale by how much more (or less) of the scene is on screen than at the player's FOV, as visible width grows with tan(fov / 2)
		return FovScale(_baseFov, _baseFov + _fovOffset);
	}

	float Core::GetDistanceScale()
	{
		// Current camera-to-pivot distance relative to the base camera's
		RE::NiPoint3  baseTranslate;
		RE::NiMatrix3 baseRotate;
		auto          pivot = _host.GetPivot();
		if (!pivot || !_host.GetBasePose(baseTranslate, baseRotate))
			return 1.0f;

		float scale    = 1.0f;
		float baseDist = baseTranslate.GetDistance(*pivot);
		if (baseDist > 1.0f) {
			float zoomIn = _camOffset.y - _framingDistance * _framingBlend;  // +Y moves toward the subject
			scale        = std::max(baseDist - zoomIn, 1.0f) / baseDist;
		}

		return std::clamp(scale, 0.1f, 10.0f);
	}

	Core::SpeedScales Core::GetSpeedScales()
	{
		SpeedScales scales;

		// Pan faster when zoomed out / wide FOV so the subject crosses the screen at a consistent rate
		scales.pan = std::clamp(GetDistanceScale() * GetFovScale(), 0.1f, 10.0f);

		// Zoom by distance only, so each step covers the same share of the way to the subject at any range
		scales.zoom = GetDistanceScale();

		// Orbit & pitch only follow FOV: at a narrow FOV the same angular step swings the
		// subject much further across the screen, so slow rotation down to keep precision
		scales.rot = GetFovScale();

		return scales;
	}

	void Core::UpdateKbmAcceleration(bool a_isTranslating, float a_interval)
	{
		// Translation accelerates the longer it's held
		if (a_isTranslating) {
			_kbmAcceleration = std::min(_kbmAcceleration + ACCEL_RATE * a_interval, ACCEL_MAX);
		} else {
			_kbmAcceleration = 1.0f;
		}
	}

	void Core::UpdateZoomAcceleration(float a_interval)
	{
		// Zoom speeds up the longer it's held, when the session asks for it, so long distances can be crossed
		if (_zoomAccelEnabled && _isZooming) {
			_zoomAcceleration = std::min(_zoomAcceleration + ACCEL_RATE * a_interval, ACCEL_MAX);
		} else {
			_zoomAcceleration = 1.0f;
		}
		_isZooming = false;
	}

	void Core::Move(const FUCK::CameraMove& a_move, const Config& a_settings, bool a_isGamepad, float a_mult, float a_interval)
	{
		a_interval = std::min(a_interval, 0.1f);

		if (!a_isGamepad) {
			UpdateKbmAcceleration(a_move.panX != 0.0f || a_move.panZ != 0.0f, a_interval);
		}

		SpeedScales scale = GetSpeedScales();
		float       step  = a_mult * a_interval;

		float panSpeed   = (a_isGamepad ? a_settings.gpPanSpeed : a_settings.kbmPanSpeed * _kbmAcceleration) * scale.pan * step;
		float zoomSpeed  = (a_isGamepad ? a_settings.gpZoomSpeed : a_settings.kbmZoomFovSpeed) * scale.zoom * _zoomAcceleration * step;
		float orbitSpeed = (a_isGamepad ? a_settings.gpRotSpeed : a_settings.kbmRotSpeed) * scale.rot * step;

		_targetCamOffset.x += a_move.panX  * panSpeed;
		_targetCamOffset.z += a_move.panZ  * panSpeed;
		_targetCamOffset.y += a_move.zoom  * zoomSpeed;
		_targetCamRotZ     += a_move.orbit * orbitSpeed;

		_isZooming |= a_move.zoom != 0.0f;
	}

	void Core::Update(const FUCK::CameraControlState& a_state, const Config& a_settings, float a_interval)
	{
		a_interval = std::min(a_interval, 0.1f);

		if (_framingSettleFrames > 0) {
			--_framingSettleFrames;
			_hasFraming = false;
		}

		UpdateFramingBlend(a_interval);
		FollowFramedPose(a_interval);

		if (!a_state.inputAllowed) {
			ClearInputState();
		} else {
			HandleControls(a_state, a_settings, a_interval);
		}

		UpdateZoomAcceleration(a_interval);
		UpdateCinematic(a_settings.cinematic, a_interval);
		ApplyFov();
	}

	Core::FrameInput Core::GatherInput(const FUCK::CameraControlState& a_state) const
	{
		FrameInput in;

		in.isGamepad    = FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad;
		in.mouseLocked  = a_state.mouseLocked;

		in.ctrlDown     = FUCK::IsModifierPressed(FUCK::Modifier::kCtrl);
		in.shiftDown    = FUCK::IsModifierPressed(FUCK::Modifier::kShift);
		in.altDown      = FUCK::IsModifierPressed(FUCK::Modifier::kAlt);

		in.lbDown       = FUCK::IsInputDown(Keys::kGP_LB);
		in.rbDown       = FUCK::IsInputDown(Keys::kGP_RB);

		in.mmbDown      = FUCK::IsInputDown(Keys::kMouse_Middle);
		in.rmbDown      = FUCK::IsInputDown(Keys::kMouse_Right);

		in.isPopupOpen  = FUCK::IsPopupOpen(nullptr, FUCK::PopupFlags::kAnyPopup);

		in.isUIHovered  = a_state.uiShown && (FUCK::IsWindowHovered(1) || FUCK::IsAnyItemHovered());
		in.isItemActive = a_state.uiShown && FUCK::IsAnyItemActive();

		bool disableGP     = in.isGamepad && a_state.gamepadBlocked;
		bool isGlobalMouse = in.mmbDown && !in.isItemActive && !in.mouseLocked && !in.isUIHovered;

		in.isGlobalKBM  = in.ctrlDown && !in.isItemActive;
		in.isGlobalGP   = in.rbDown && !in.isItemActive && !disableGP;
		in.isCameraMode = in.isGlobalKBM || in.isGlobalGP || isGlobalMouse;

		return in;
	}

	void Core::HandleControls(const FUCK::CameraControlState& a_state, const Config& a_settings, float a_interval)
	{
		FrameInput in = GatherInput(a_state);

		// With cameraMoves off only turning works, so scrolls are dropped now rather than zooming once it's back on
		if (!a_state.cameraMoves) {
			_pendingScroll = 0.0f;
		} else if (in.isCameraMode) {
			in.scale = GetSpeedScales();

			HandleMouse   (in, a_settings, a_interval);
			if (_keyboardEnabled)
				HandleKeyboard(in, a_settings, a_interval);
			HandleGamepad (in, a_settings, a_interval);

			ClampPitch();
		}

		HandleSubjectRotation(in, a_settings, a_interval);
	}

	void Core::ClampPitch()
	{
		// Pitch offsets turn against the base camera's own pitch, so measure the limit from it.
		// Keeps the total short of straight up or down, to prevent gimbal flips
		float basePitch = 0.0f;

		RE::NiPoint3  baseTranslate;
		RE::NiMatrix3 baseRotate;
		if (_host.GetBasePose(baseTranslate, baseRotate)) {
			basePitch = std::asin(std::clamp(-ForwardAxis(baseRotate).z, -1.0f, 1.0f));
		}

		_targetCamRotX = std::clamp(_targetCamRotX, basePitch - PITCH_CLAMP_LIMIT, basePitch + PITCH_CLAMP_LIMIT);
	}

	void Core::UpdateFramingBlend(float a_interval)
	{
		// Ease the swoop in and out, landing exactly on the base view or the body, so a leftover fraction can't keep the camera updating
		float step        = a_interval / FRAMING_DURATION;
		_framingProgress  = std::clamp(_framingProgress + (_isFramed ? step : -step), 0.0f, 1.0f);
		_framingBlend     = _framingProgress * _framingProgress * (3.0f - 2.0f * _framingProgress);
	}

	void Core::ClearInputState()
	{
		// Drop anything queued up so it doesn't fire the moment the lock lifts
		_pendingScroll    = 0.0f;
		_smoothScrollZoom = 0.0f;
		_smoothScrollFov  = 0.0f;
		_kbmAcceleration  = 1.0f;
		_zoomAcceleration = 1.0f;
		_isZooming        = false;
		_wasMouseSubjRot  = false;

		// Wait for X/Y to be released, so buttons pressed while locked don't turn the subject afterwards
		_lockoutGPSubjRot = true;
	}

	void Core::HandleMouse(const FrameInput& a_in, const Config& a_settings, float a_interval)
	{
		if (a_in.ctrlDown && !a_in.isUIHovered && !a_in.isPopupOpen) {
			if (_pendingScroll != 0.0f && !_host.IsScrollBlocked()) {
				float amount = _pendingScroll * (a_settings.kbmZoomFovSpeed * MOUSE_WHEEL_MULT);
				if (a_in.shiftDown) {
					// FOV (Ctrl + Shift + Scroll)
					_smoothScrollFov += amount;
				} else {
					// Zoom (Ctrl + Scroll)
					_smoothScrollZoom += amount * a_in.scale.zoom;
				}
			}
		}
		_pendingScroll = 0.0f;

		float scrollFactor = SmoothFactor(SCROLL_SMOOTH_SPEED, a_interval);
		_targetCamOffset.y += DrainSmoothed(_smoothScrollZoom, scrollFactor, EPSILON);
		_targetFovOffset   -= DrainSmoothed(_smoothScrollFov,  scrollFactor, EPSILON);

		if (!a_in.mmbDown || a_in.isUIHovered || a_in.isPopupOpen || a_in.mouseLocked)
			return;

		ImVec2 mouseDelta = FUCK::GetMouseDelta();
		float  mult       = MOUSE_MOVE_MULT;

		if (a_in.ctrlDown && a_in.shiftDown) {
			// Roll (Middle + Ctrl + Shift)
			_targetCamRoll += mouseDelta.x * a_settings.kbmRollSpeed * mult;
		} else if (a_in.ctrlDown) {
			// Pitch (Middle + Ctrl)
			_targetCamRotX += mouseDelta.y * a_settings.kbmRotSpeed * mult * a_in.scale.rot;
		} else if (a_in.rmbDown) {
			// Orbit (Middle + Right)
			_targetCamRotZ += mouseDelta.x * a_settings.kbmRotSpeed * mult * a_in.scale.rot;
		} else {
			// Pan (Middle)
			_targetCamOffset.x -= mouseDelta.x * a_settings.kbmPanSpeed * mult * a_in.scale.pan;
			_targetCamOffset.z += mouseDelta.y * a_settings.kbmPanSpeed * mult * a_in.scale.pan;
		}
	}

	void Core::HandleKeyboard(const FrameInput& a_in, const Config& a_settings, float a_interval)
	{
		if (!a_in.isGlobalKBM)
			return;

		UpdateKbmAcceleration(FUCK::IsInputDown(Keys::kKB_W) || FUCK::IsInputDown(Keys::kKB_S) ||
		                      FUCK::IsInputDown(Keys::kKB_A) || FUCK::IsInputDown(Keys::kKB_D), a_interval);

		float speed     = a_settings.kbmPanSpeed     * _kbmAcceleration * a_in.scale.pan * a_interval;
		float rotSpeed  = a_settings.kbmRotSpeed     * a_in.scale.rot * a_interval;
		float rollSpeed = a_settings.kbmRollSpeed    * a_interval;
		float fovSpeed  = a_settings.kbmZoomFovSpeed * a_interval;
		float zoomSpeed = fovSpeed * a_in.scale.zoom * _zoomAcceleration;

		float ws = InputAxis(Keys::kKB_W, Keys::kKB_S);
		float da = InputAxis(Keys::kKB_D, Keys::kKB_A);
		float eq = InputAxis(Keys::kKB_E, Keys::kKB_Q);
		float rf = InputAxis(Keys::kKB_R, Keys::kKB_F);

		if (a_in.shiftDown) {
			// Secondary (Ctrl + Shift): Zoom, FOV, Roll & Pitch
			_targetCamOffset.y += ws * zoomSpeed;
			_isZooming         |= ws != 0.0f;
			_targetFovOffset   += da * fovSpeed;
			_targetCamRoll     += eq * rollSpeed;
			_targetCamRotX     += rf * rotSpeed;
		} else {
			// Primary (Ctrl): Pan, Orbit & Pitch
			_targetCamOffset.z += ws * speed;
			_targetCamOffset.x += da * speed;
			_targetCamRotZ     += eq * rotSpeed;
			_targetCamRotX     += rf * rotSpeed;
		}
	}

	void Core::HandleGamepad(const FrameInput& a_in, const Config& a_settings, float a_interval)
	{
		if (!a_in.isGlobalGP)
			return;

		float panSpeed  = a_settings.gpPanSpeed  * a_in.scale.pan * a_interval;
		float zoomSpeed = a_settings.gpZoomSpeed * a_in.scale.zoom * _zoomAcceleration * a_interval;
		float rotSpeed  = a_settings.gpRotSpeed  * a_in.scale.rot * a_interval;
		float rollSpeed = a_settings.gpRollSpeed * a_interval;
		float fovSpeed  = a_settings.gpFovSpeed  * a_interval;

		// Thumbsticks & triggers (RB held)
		auto axis = [&](std::uint32_t a_key) {
			return ApplyDeadzone(FUCK::GetAnalogInput(a_key), a_settings.gpDeadzone);
		};

		float leftX  = axis(Keys::kGP_LSX);
		float leftY  = axis(Keys::kGP_LSY);
		float rightX = axis(Keys::kGP_RSX);
		float rightY = axis(Keys::kGP_RSY);
		float lt     = axis(Keys::kGP_LT);
		float rt     = axis(Keys::kGP_RT);

		_targetCamOffset.z += leftY * panSpeed;
		_targetCamOffset.x += leftX * panSpeed;
		_targetFovOffset   += (rt - lt) * fovSpeed;

		if (a_in.lbDown) {
			// LB + RB: Roll & Pitch
			_targetCamRoll += rightX * rollSpeed;
			_targetCamRotX -= rightY * rotSpeed;
		} else {
			// RB: Orbit & Zoom
			_targetCamRotZ     -= rightX * rotSpeed;
			_targetCamOffset.y += rightY * zoomSpeed;
			_isZooming         |= rightY != 0.0f;
		}
	}

	void Core::HandleSubjectRotation(const FrameInput& a_in, const Config& a_settings, float a_interval)
	{
		if (!_turningEnabled) {
			_wasMouseSubjRot = false;
			return;
		}

		// Right-drag, or RB + X/Y on gamepad
		bool gpY = FUCK::IsInputDown(Keys::kGP_Y);
		bool gpX = FUCK::IsInputDown(Keys::kGP_X);

		// Prevent input "bleed" when selecting UI items with the gamepad.
		if (a_in.isPopupOpen || a_in.isItemActive) {
			_lockoutGPSubjRot = true;
		} else if (!gpY && !gpX) {
			_lockoutGPSubjRot = false;
		}

		// Block mouse rotation over ImGui windows or client UI that owns the mouse
		bool allowMouseRot = FUCK::IsInputDown(Keys::kMouse_Right) && !a_in.mmbDown && !a_in.isUIHovered && !a_in.isPopupOpen && !a_in.mouseLocked;
		bool allowGPRot    = a_in.rbDown && (gpY || gpX) && !_lockoutGPSubjRot;

		float rotAmount = 0.0f;

		if (allowMouseRot) {
			if (_wasMouseSubjRot) {  // Ignore the very first frame to prevent delta spikes
				rotAmount = FUCK::GetMouseDelta().x * MOUSE_ROT_MULT;
			}
		} else if (allowGPRot) {
			if (gpY)
				rotAmount -= a_settings.gpCharRotSpeed * a_interval;
			if (gpX)
				rotAmount += a_settings.gpCharRotSpeed * a_interval;
		}

		_wasMouseSubjRot = allowMouseRot;

		if (rotAmount != 0.0f) {
			_host.RotateSubject(rotAmount);
		}
	}

	void Core::UpdateCinematic(bool a_enabled, float a_interval)
	{
		if (!a_enabled) {
			_camOffset = _targetCamOffset;
			_camRotZ   = _targetCamRotZ;
			_camRotX   = _targetCamRotX;
			_camRoll   = _targetCamRoll;
			_fovOffset = _targetFovOffset;
			return;
		}

		float t      = SmoothFactor(CINEMATIC_SPEED, a_interval);
		_camOffset.x = std::lerp(_camOffset.x, _targetCamOffset.x, t);
		_camOffset.y = std::lerp(_camOffset.y, _targetCamOffset.y, t);
		_camOffset.z = std::lerp(_camOffset.z, _targetCamOffset.z, t);
		_camRotZ     = std::lerp(_camRotZ,     _targetCamRotZ,     t);
		_camRotX     = std::lerp(_camRotX,     _targetCamRotX,     t);
		_camRoll     = std::lerp(_camRoll,     _targetCamRoll,     t);
		_fovOffset   = std::lerp(_fovOffset,   _targetFovOffset,   t);
	}

	void Core::ApplyFov()
	{
		auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera)
			return;

		CaptureBaseFov();

		if (_baseFov != 0.0f)
			camera->GetRuntimeData2().worldFOV = _baseFov + _fovOffset;
	}

	void Core::CaptureBaseFov()
	{
		// Grab the player's FOV on the first frame (restored on reset), so the view matches vanilla until it's adjusted
		if (_baseFov != 0.0f)
			return;

		if (auto camera = RE::PlayerCamera::GetSingleton())
			_baseFov = camera->GetRuntimeData2().worldFOV;
	}

	// kFollowPose: a framed body that stands up from a kneel, or sits, is framed again, easing over so the camera doesn't jump
	void Core::FollowFramedPose(float a_interval)
	{
		if (!_isFramed || !_hasFraming)
			return;

		BodyPoints body;
		if (_followPose && _host.GetBodyPoints(body) && std::abs(body.top.GetDistance(body.bottom) - _framingHeight) > _framingHeight * FRAMING_POSE_CHANGE)
			UpdateFraming(_host.GetPivot().value_or(_originalTranslate), true);

		const float t      = SmoothFactor(FRAMING_FOLLOW_SPEED, a_interval);
		_framingCamDelta   = _framingCamDelta + (_framingCamDeltaTo - _framingCamDelta) * t;
		_framingPivotDelta = _framingPivotDelta + (_framingPivotDeltaTo - _framingPivotDelta) * t;
		_framingDistance   = std::lerp(_framingDistance, _framingDistanceTo, t);
	}

	void Core::UpdateFraming(const RE::NiPoint3& a_pivot, bool a_ease)
	{
		// Bail if anything isn't ready yet. It's tried again next frame
		auto worldCam = RE::Main::WorldRootCamera();
		if (!worldCam)
			return;

		const auto& frustum = worldCam->GetRuntimeData2().viewFrustum;
		if (frustum.fTop <= 0.01f || frustum.fRight <= 0.01f)
			return;

		BodyPoints body;
		if (!_host.GetBodyPoints(body))
			return;

		auto framing = FrameFullBody(body, _originalRotate, frustum.fTop, frustum.fRight, _framingMargin, _framingRect);
		if (!framing)
			return;

		float pivotDepth = (a_pivot - _originalTranslate).Dot(ForwardAxis(_originalRotate));

		// Stored relative to the base camera so idle animation can't drag the framing around
		_framingCamDeltaTo   = framing->camPos - _originalTranslate;
		_framingPivotDeltaTo = body.centre - _originalTranslate;
		_framingDistanceTo   = std::max(framing->depth - pivotDepth, 0.0f);
		_framingHeight       = body.top.GetDistance(body.bottom);  // along the body, so it holds for one lying down too

		if (!a_ease || !_hasFraming) {
			_framingCamDelta   = _framingCamDeltaTo;
			_framingPivotDelta = _framingPivotDeltaTo;
			_framingDistance   = _framingDistanceTo;
		}
		_hasFraming = true;
	}

	bool Core::Compose(const RE::NiPoint3& a_baseTranslate, const RE::NiMatrix3& a_baseRotate, RE::NiPoint3& a_outTranslate, RE::NiMatrix3& a_outRotate)
	{
		a_outTranslate = a_baseTranslate;
		a_outRotate    = a_baseRotate;

		if (!HasAnyCamera())
			return false;

		_originalTranslate = a_baseTranslate;
		_originalRotate    = a_baseRotate;

		RE::NiPoint3 pivot = _host.GetPivot().value_or(_originalTranslate);

		if (_isFramed && !_hasFraming) {
			UpdateFraming(pivot);
		}

		RE::NiPoint3 camBase = _originalTranslate;
		if (_hasFraming && _framingBlend > 0.0f) {
			RE::NiPoint3 bodyPivot = _originalTranslate + _framingPivotDelta;
			pivot                  = pivot + (bodyPivot - pivot) * _framingBlend;
			camBase                = _originalTranslate + _framingCamDelta * _framingBlend;
		}

		RE::NiPoint3  orbitedTranslate = OrbitAroundZ(pivot, camBase, _camRotZ);
		RE::NiMatrix3 orbitedRotate    = MakeYaw(_camRotZ) * _originalRotate;
		RE::NiMatrix3 pitchMat         = MakePitch(_camRotX);
		RE::NiMatrix3 rollMat          = MakeRoll(_camRoll);

		RE::NiPoint3 finalOffset = orbitedRotate * (pitchMat * _camOffset);
		a_outTranslate           = orbitedTranslate + finalOffset;
		a_outRotate              = orbitedRotate * pitchMat * rollMat;
		return true;
	}

	void Core::SetBodyFraming(bool a_framed, const FUCK::CameraScreenRect& a_rect, float a_margin)
	{
		if (a_margin <= 0.0f) {
			a_margin = FRAMING_MARGIN;
		}

		if (a_framed == _isFramed) {
			// Already framed: a new area re-frames in place and keeps the offsets, e.g. once a menu's layout can be read
			bool changed = a_rect.centreX != _framingRect.centreX || a_rect.centreY != _framingRect.centreY ||
			               a_rect.halfHeight != _framingRect.halfHeight || a_margin != _framingMargin;
			if (_isFramed && changed) {
				_framingRect   = a_rect;
				_framingMargin = a_margin;
				_hasFraming    = false;
			}
			return;
		}

		_isFramed = a_framed;
		_smoothScrollZoom = 0.0f;

		// Framing the body eases out the moves so the camera settles on it, and turning it off eases back to them
		if (_isFramed) {
			_framingRect   = a_rect;
			_framingMargin = a_margin;
			_hasFraming    = false;

			_unframedOffset  = _targetCamOffset;
			_unframedRotZ    = _targetCamRotZ;
			_unframedRotX    = _targetCamRotX;
			_targetCamOffset = { 0.0f, 0.0f, 0.0f };
			_targetCamRotZ   = 0.0f;
			_targetCamRotX   = 0.0f;
		} else {
			_targetCamOffset = _unframedOffset;
			_targetCamRotZ   = _unframedRotZ;
			_targetCamRotX   = _unframedRotX;
		}
	}

	bool Core::GetBodyFraming(FUCK::CameraScreenRect& a_rect, float& a_margin) const
	{
		if (!_isFramed)
			return false;

		a_rect   = _framingRect;
		a_margin = _framingMargin;
		return true;
	}

	Offsets Core::GetTargetOffsets() const
	{
		Offsets offsets;
		offsets.translate = _targetCamOffset;
		offsets.orbit     = _targetCamRotZ;
		offsets.pitch     = _targetCamRotX;
		offsets.roll      = _targetCamRoll;
		offsets.fov       = _baseFov != 0.0f ? _baseFov + _targetFovOffset : 0.0f;
		return offsets;
	}

	void Core::SetTargetOffsets(const Offsets& a_offsets)
	{
		_targetCamOffset = a_offsets.translate;
		_targetCamRotZ   = a_offsets.orbit;
		_targetCamRotX   = a_offsets.pitch;
		_targetCamRoll   = a_offsets.roll;

		CaptureBaseFov();
		if (a_offsets.fov > 0.0f && _baseFov != 0.0f) {
			_targetFovOffset = a_offsets.fov - _baseFov;
		}

		// Drop scroll still easing in, so it can't push past the preset
		_smoothScrollZoom = 0.0f;
		_smoothScrollFov  = 0.0f;
	}

	void Core::Reset()
	{
		if (auto camera = RE::PlayerCamera::GetSingleton()) {
			if (_baseFov != 0.0f)
				camera->GetRuntimeData2().worldFOV = _baseFov;
		}

		ClearMotion();
		_baseFov = 0.0f;
	}

	void Core::SnapTo(float a_roll, float a_fov)
	{
		float fovOffset = _fovOffset;
		ClearMotion();

		_camRoll       = a_roll;
		_targetCamRoll = a_roll;

		// A pose without a FOV keeps the current one
		CaptureBaseFov();
		if (a_fov > 0.0f && _baseFov != 0.0f) {
			fovOffset = a_fov - _baseFov;
		}
		_fovOffset       = fovOffset;
		_targetFovOffset = fovOffset;
	}

	void Core::Settle()
	{
		_camOffset = _targetCamOffset;
		_camRotZ   = _targetCamRotZ;
		_camRotX   = _targetCamRotX;
		_camRoll   = _targetCamRoll;
		_fovOffset = _targetFovOffset;

		_framingProgress = _isFramed ? 1.0f : 0.0f;
		_framingBlend    = _framingProgress;

		// Framed instantly, so the framing is recalculated over the next few frames while the subject's pose catches up, e.g. after turning to face the camera
		_framingSettleFrames = _isFramed ? FRAMING_SETTLE_FRAMES : 0;
	}

	void Core::Pause()
	{
		if (auto camera = RE::PlayerCamera::GetSingleton()) {
			if (_baseFov != 0.0f)
				camera->GetRuntimeData2().worldFOV = _baseFov;
		}

		ClearInputState();
	}

	void Core::ClearMotion()
	{
		_camOffset           = { 0.0f, 0.0f, 0.0f };
		_camRotZ             = 0.0f;
		_camRotX             = 0.0f;
		_camRoll             = 0.0f;
		_fovOffset           = 0.0f;
		_targetCamOffset     = { 0.0f, 0.0f, 0.0f };
		_targetCamRotZ       = 0.0f;
		_targetCamRotX       = 0.0f;
		_targetCamRoll       = 0.0f;
		_targetFovOffset     = 0.0f;
		_unframedOffset      = { 0.0f, 0.0f, 0.0f };
		_unframedRotZ        = 0.0f;
		_unframedRotX        = 0.0f;
		_isFramed            = false;
		_framingProgress     = 0.0f;
		_framingBlend        = 0.0f;
		_framingDistance     = 0.0f;
		_hasFraming          = false;
		_smoothScrollZoom    = 0.0f;
		_smoothScrollFov     = 0.0f;
	}
}
