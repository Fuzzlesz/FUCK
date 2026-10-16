#pragma once

namespace Camera
{
	// Where the subject stood, kept with free-camera presets when asked for
	struct SubjectTransform
	{
		std::array<float, 3> position{};
		std::array<float, 3> angle{};
		std::string          cellPlugin;  // plugin and local ID rather than a form ID, so a load order change can't break it
		std::uint32_t        cellLocalID = 0;

		std::optional<float> facingYaw;  // the 3D's heading, as turning moves that rather than the actor's
	};

	// Offsets on the session's base camera, plus the subject's facing relative to it
	struct OffsetPreset
	{
		std::array<float, 3> offset{};
		float                orbit = 0.0f;
		float                pitch = 0.0f;
		float                roll  = 0.0f;
		float                fov   = 0.0f;  // absolute, 0 for unchanged

		bool                 framed        = false;
		std::array<float, 3> framingRect{};  // centre X, centre Y, half height
		float                framingMargin = 1.0f;

		std::optional<float> facingYaw;  // the subject's heading minus the base camera's, so it holds wherever the base camera is
	};

	// A world pose, plus the subject's transform when saved with it
	struct FreePreset
	{
		std::array<float, 3> position{};
		float                pitch = 0.0f;
		float                yaw   = 0.0f;
		float                roll  = 0.0f;
		float                fov   = 0.0f;

		std::optional<SubjectTransform> subject;
	};

	struct Preset
	{
		int                         version = 1;
		std::string                 name;
		int                         hotkey = 0;  // 1-9, 0 for none
		std::optional<OffsetPreset> offset;
		std::optional<FreePreset>   free;
	};

	// A still-camera context's presets, kept together in one file
	struct PresetList
	{
		int                         version = 1;
		std::vector<Preset>         presets;
		std::optional<OffsetPreset> last;  // the camera as the last kRememberCamera session left it
	};

	// The presets a session can use, under FUCK's camera folder:
	// free-camera presets are world poses shared by every free-camera session, one file each in camera\free\.
	// Still-camera presets are relative to the client's camera and see little use, so each context keeps one file, e.g. camera\still\FUCK-RACE.json
	class Presets
	{
	public:
		void Open(std::string_view a_context, bool a_isFree);
		void Close();
		bool IsOpen() const { return !_folder.empty() || !_listFile.empty(); }

		// Sorted by name
		const std::vector<Preset>& GetAll() const { return _presets; }
		const Preset*              Find(std::string_view a_name) const;
		const Preset*              FindByHotkey(int a_hotkey) const;

		// Replaces a preset of the same name, keeping its shortcut
		void Store(Preset a_preset);
		bool Remove(std::string_view a_name);

		// Moves the shortcut here from whichever preset had it. 0 clears it
		bool SetHotkey(std::string_view a_name, int a_hotkey);

		// The base name followed by the lowest number not already taken, e.g. "Preset 3"
		std::string MakeUniqueName(std::string_view a_base) const;

		// Still only: the camera as a kRememberCamera session left it, kept beside the presets
		const std::optional<OffsetPreset>& GetLast() const { return _last; }
		void                               StoreLast(const OffsetPreset& a_last);

	private:
		void        Load();
		void        LoadFolder();
		void        LoadList();
		void        Write(std::size_t a_index) const;
		void        WriteList() const;
		std::string MakeFilePath(std::string_view a_name) const;
		void        Sort();

		std::string                 _folder;    // free: one file per preset here
		std::string                 _listFile;  // still: every preset in this file
		bool                        _isFree = false;
		std::vector<Preset>         _presets;
		std::vector<std::string>    _files;  // free: each preset's file, by index
		std::optional<OffsetPreset> _last;
	};
}
