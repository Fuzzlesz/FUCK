#pragma once

#include "API/FUCK_Camera.h"

namespace Camera::Maths
{
	inline constexpr float DEG2RAD = std::numbers::pi_v<float> / 180.0f;

	// Framerate independent lerp factor
	inline float SmoothFactor(float a_speed, float a_dt)
	{
		return 1.0f - std::exp(-a_speed * a_dt);
	}

	// Returns the step to apply and removes it from a_pending, settling to exactly 0
	inline float DrainSmoothed(float& a_pending, float a_factor, float a_epsilon)
	{
		float step = std::abs(a_pending) > a_epsilon ? a_pending * a_factor : a_pending;
		a_pending -= step;
		return step;
	}

	inline constexpr float MIN_FOV = 1.0f;
	inline constexpr float MAX_FOV = 170.0f;

	inline float ClampFov(float a_fov) { return std::clamp(a_fov, MIN_FOV, MAX_FOV); }
	inline float TanHalf(float a_fovDeg) { return std::tan(a_fovDeg * 0.5f * DEG2RAD); }
	inline float FovFromTanHalf(float a_tanHalf) { return 2.0f * std::atan(a_tanHalf) / DEG2RAD; }

	inline float FovScale(float a_baseFov, float a_fov)
	{
		float scale = TanHalf(ClampFov(a_fov)) / TanHalf(a_baseFov);
		return std::clamp(scale, 0.1f, 10.0f);
	}

	// Yaw Matrix (Rotation around world Z-axis / Up vector)
	inline RE::NiMatrix3 MakeYaw(float a_angle)
	{
		float c = std::cos(a_angle);
		float s = std::sin(a_angle);

		RE::NiMatrix3 m;
		m.entry[0][0] = c;
		m.entry[0][1] = -s;
		m.entry[0][2] = 0.0f;
		m.entry[1][0] = s;
		m.entry[1][1] = c;
		m.entry[1][2] = 0.0f;
		m.entry[2][0] = 0.0f;
		m.entry[2][1] = 0.0f;
		m.entry[2][2] = 1.0f;
		return m;
	}

	// Pitch Matrix (Rotation around local X-axis / Right vector)
	inline RE::NiMatrix3 MakePitch(float a_angle)
	{
		float c = std::cos(a_angle);
		float s = std::sin(a_angle);

		RE::NiMatrix3 m;
		m.entry[0][0] = 1.0f;
		m.entry[0][1] = 0.0f;
		m.entry[0][2] = 0.0f;
		m.entry[1][0] = 0.0f;
		m.entry[1][1] = c;
		m.entry[1][2] = -s;
		m.entry[2][0] = 0.0f;
		m.entry[2][1] = s;
		m.entry[2][2] = c;
		return m;
	}

	// Roll Matrix (Rotation around local Y-axis / Forward vector)
	inline RE::NiMatrix3 MakeRoll(float a_angle)
	{
		float c = std::cos(a_angle);
		float s = std::sin(a_angle);

		RE::NiMatrix3 m;
		m.entry[0][0] = c;
		m.entry[0][1] = 0.0f;
		m.entry[0][2] = -s;
		m.entry[1][0] = 0.0f;
		m.entry[1][1] = 1.0f;
		m.entry[1][2] = 0.0f;
		m.entry[2][0] = s;
		m.entry[2][1] = 0.0f;
		m.entry[2][2] = c;
		return m;
	}

	// Camera axes: X right, Y forward, Z up
	inline RE::NiPoint3 RightAxis(const RE::NiMatrix3& a_rot) { return { a_rot.entry[0][0], a_rot.entry[1][0], a_rot.entry[2][0] }; }
	inline RE::NiPoint3 ForwardAxis(const RE::NiMatrix3& a_rot) { return { a_rot.entry[0][1], a_rot.entry[1][1], a_rot.entry[2][1] }; }
	inline RE::NiPoint3 UpAxis(const RE::NiMatrix3& a_rot) { return { a_rot.entry[0][2], a_rot.entry[1][2], a_rot.entry[2][2] }; }

	// A camera turned round a pivot to the front of a subject heading the given way (from +y towards +x), keeping its height and pitch
	inline void TurnToFront(RE::NiPoint3& a_translate, RE::NiMatrix3& a_rotate, const RE::NiPoint3& a_pivot, float a_subjectHeading)
	{
		const float turn = a_subjectHeading + std::numbers::pi_v<float> - std::atan2(a_rotate.entry[0][1], a_rotate.entry[1][1]);
		const float c    = std::cos(turn);
		const float s    = std::sin(turn);
		const auto  turned = [&](const RE::NiPoint3& a_v) { return RE::NiPoint3{ a_v.x * c + a_v.y * s, a_v.y * c - a_v.x * s, a_v.z }; };

		const RE::NiPoint3 offset = turned(a_translate - a_pivot);
		a_translate               = { a_pivot.x + offset.x, a_pivot.y + offset.y, a_translate.z };

		// Each axis is a column
		for (int column = 0; column < 3; ++column) {
			const auto axis           = turned({ a_rotate.entry[0][column], a_rotate.entry[1][column], a_rotate.entry[2][column] });
			a_rotate.entry[0][column] = axis.x;
			a_rotate.entry[1][column] = axis.y;
			a_rotate.entry[2][column] = axis.z;
		}
	}

	// A camera out in front of a subject heading the given way (from +y towards +x), looking back at the pivot and tilted down by a_pitch.
	// Body framing then sets how far away. For where the camera it starts from would give too many different views (a dragon's)
	inline void FrontView(RE::NiPoint3& a_translate, RE::NiMatrix3& a_rotate, const RE::NiPoint3& a_pivot, float a_subjectHeading, float a_pitch)
	{
		constexpr float BACK = 300.0f;  // a starting distance, until framing sets it

		const float        c = std::cos(a_pitch);
		const float        s = std::sin(a_pitch);
		const RE::NiPoint3 look{ -std::sin(a_subjectHeading), -std::cos(a_subjectHeading), 0.0f };
		const RE::NiPoint3 right{ look.y, -look.x, 0.0f };
		const RE::NiPoint3 forward{ look.x * c, look.y * c, -s };
		const RE::NiPoint3 up{ look.x * s, look.y * s, c };

		// Each axis is a column: right, forward (the way the camera looks), then up the screen
		for (int row = 0; row < 3; ++row) {
			a_rotate.entry[row][0] = right[row];
			a_rotate.entry[row][1] = forward[row];
			a_rotate.entry[row][2] = up[row];
		}
		a_translate = a_pivot - forward * BACK;
	}

	// A camera beyond the feet of a body lying down, raised and looking down along it with the head toward the top of the screen.
	// Body framing then sets how far away. False if the body has no length across the floor to look along
	inline bool LookAlongLyingBody(RE::NiPoint3& a_translate, RE::NiMatrix3& a_rotate, const RE::NiPoint3& a_head, const RE::NiPoint3& a_pelvis)
	{
		constexpr float PITCH = 60.0f * std::numbers::pi_v<float> / 180.0f;  // down from level
		constexpr float BACK  = 250.0f;                                       // a starting distance, until framing sets it

		const RE::NiPoint3 body   = a_head - a_pelvis;
		const float        length = std::sqrt(body.x * body.x + body.y * body.y);
		if (length < 1.0f)
			return false;

		const float        c = std::cos(PITCH);
		const float        s = std::sin(PITCH);
		const RE::NiPoint3 along{ body.x / length, body.y / length, 0.0f };
		const RE::NiPoint3 right{ along.y, -along.x, 0.0f };
		const RE::NiPoint3 forward{ along.x * c, along.y * c, -s };
		const RE::NiPoint3 up{ along.x * s, along.y * s, c };

		// Each axis is a column: right, forward (the way the camera looks), then up the screen
		for (int row = 0; row < 3; ++row) {
			a_rotate.entry[row][0] = right[row];
			a_rotate.entry[row][1] = forward[row];
			a_rotate.entry[row][2] = up[row];
		}
		a_translate = a_pelvis + body * 0.5f - forward * BACK;
		return true;
	}

	// Heading of a rotation's forward axis around world Z, matching the free camera's yaw
	inline float YawOf(const RE::NiMatrix3& a_rot)
	{
		RE::NiPoint3 forward = ForwardAxis(a_rot);
		return std::atan2(forward.x, forward.y);
	}

	// The free camera's angles for a camera rotation. Its own rotation is EulerAnglesToAxesZXY(pitch, 0, yaw),
	// columns Right / Forward / Up, and roll turns Right and Up around Forward, as the roll offset does
	inline void AnglesFromRotation(const RE::NiMatrix3& a_rot, float& a_pitch, float& a_yaw, float& a_roll)
	{
		RE::NiPoint3 forward = ForwardAxis(a_rot);

		a_yaw   = std::atan2(forward.x, forward.y);
		a_pitch = std::asin(std::clamp(-forward.z, -1.0f, 1.0f));

		RE::NiMatrix3 unrolled;
		unrolled.EulerAnglesToAxesZXY(a_pitch, 0.0f, a_yaw);

		RE::NiPoint3 right = RightAxis(a_rot);
		a_roll             = std::atan2(right.Dot(UpAxis(unrolled)), right.Dot(RightAxis(unrolled)));
	}

	inline RE::NiPoint3 OrbitAroundZ(const RE::NiPoint3& a_pivot, const RE::NiPoint3& a_point, float a_yaw)
	{
		float c = std::cos(a_yaw);
		float s = std::sin(a_yaw);

		float rx = a_point.x - a_pivot.x;
		float ry = a_point.y - a_pivot.y;

		return {
			a_pivot.x + (rx * c - ry * s),
			a_pivot.y + (rx * s + ry * c),
			a_point.z
		};
	}

	struct BodyFraming
	{
		RE::NiPoint3 camPos;
		float        depth;
	};

	// Body framing's shape, tuned on the vanilla skeleton
	inline constexpr float FRAMING_HEAD_ROOM = 0.15f;  // how far above the head node the top of the head is taken to be, as a share of the node's height off the floor. About 18 units
	inline constexpr float FRAMING_MARGIN    = 1.15f;  // the room around a framed body, as a scale on its height, when the client doesn't pass its own

	// The body's top and bottom for framing, and its centre
	struct BodyPoints
	{
		RE::NiPoint3 centre;  // halfway up, over the pelvis
		RE::NiPoint3 top;     // the top of the head
		RE::NiPoint3 bottom;  // the floor under the pelvis
	};

	// From the floor to the top of the head, so crouching, which lowers the pelvis more than the head, doesn't cut the head off.
	// The top follows the head forward when it leans, and the rest is centred over the pelvis, so an idle that steps aside stays in frame
	inline BodyPoints MakeBodyPoints(const RE::NiPoint3& a_pelvis, const RE::NiPoint3& a_head, float a_floor)
	{
		const float top = a_head.z + (a_head.z - a_floor) * FRAMING_HEAD_ROOM;
		return {
			{ a_pelvis.x, a_pelvis.y, (a_floor + top) * 0.5f },
			{ a_head.x, a_head.y, top },
			{ a_pelvis.x, a_pelvis.y, a_floor }
		};
	}

	// A body lying down, e.g. in a bed, runs from the feet to the top of the head along its length rather than up from the floor,
	// so a camera looking down on it frames all of it
	inline BodyPoints MakeLyingBodyPoints(const RE::NiPoint3& a_head, const RE::NiPoint3& a_feet)
	{
		const RE::NiPoint3 length = a_head - a_feet;
		const RE::NiPoint3 top    = a_head + length * FRAMING_HEAD_ROOM;
		return { (top + a_feet) * 0.5f, top, a_feet };
	}

	inline std::optional<BodyFraming> FrameFullBody(
		const BodyPoints&             a_body,
		const RE::NiMatrix3&          a_camRotate,
		float                         a_tanHalfV,
		float                         a_tanHalfH,
		float                         a_margin,
		const FUCK::CameraScreenRect& a_rect)
	{
		// The area shrunk by the margin, so the body fits inside it with that much room around
		const float halfHeight = a_rect.halfHeight / std::max(a_margin, 0.01f);
		if (halfHeight < 0.01f)
			return std::nullopt;

		const auto forward = ForwardAxis(a_camRotate);
		const auto up      = UpAxis(a_camRotate);
		const auto right   = RightAxis(a_camRotate);

		// How far the top and bottom are from the centre, up the screen and into it, so a head leaning toward the camera counts as nearer, and so bigger
		const float topUp  = (a_body.top - a_body.centre).Dot(up);
		const float topFwd = (a_body.top - a_body.centre).Dot(forward);
		const float botUp  = (a_body.bottom - a_body.centre).Dot(up);
		const float botFwd = (a_body.bottom - a_body.centre).Dot(forward);
		if (topUp - botUp < 1.0f)
			return std::nullopt;

		// Solved in perspective so the top lands on the area's top edge and the bottom on its bottom edge.
		// For an upright body facing the camera, this is the same as fitting its height
		const float topEdge = (a_rect.centreY + halfHeight) * a_tanHalfV;
		const float botEdge = (a_rect.centreY - halfHeight) * a_tanHalfV;
		const float depth   = ((topUp - botUp) - topEdge * topFwd + botEdge * botFwd) / (topEdge - botEdge);
		if (depth < 1.0f)
			return std::nullopt;

		const float lift = topEdge * (topFwd + depth) - topUp;

		RE::NiPoint3 camPos = a_body.centre - forward * depth - up * lift - right * (a_rect.centreX * depth * a_tanHalfH);
		return BodyFraming{ camPos, depth };
	}

	inline float ApplyDeadzone(float a_value, float a_deadzone)
	{
		float magnitude = std::abs(a_value);
		if (magnitude < a_deadzone)
			return 0.0f;

		float normalized = (magnitude - a_deadzone) / (1.0f - a_deadzone);
		float curved     = normalized * normalized;
		return a_value < 0.0f ? -curved : curved;
	}
}
