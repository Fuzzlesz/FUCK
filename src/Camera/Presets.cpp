#include "Camera/Presets.h"

#include "System/Settings.h"

#include <numeric>

namespace
{
	// A name as a file or folder name, keeping it readable
	std::string ToFileName(std::string_view a_name)
	{
		std::string name;
		name.reserve(a_name.size());
		for (char c : a_name) {
			bool safe = std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == ' ' || c == '(' || c == ')';
			name += safe ? c : '_';
		}

		// Windows drops trailing spaces and dots from file names
		while (!name.empty() && (name.back() == ' ' || name.back() == '.')) {
			name.pop_back();
		}
		return name;
	}

	bool LessByName(const std::string& a_left, const std::string& a_right)
	{
		return std::ranges::lexicographical_compare(a_left, a_right, [](unsigned char a, unsigned char b) {
			return std::tolower(a) < std::tolower(b);
		});
	}
}

namespace Camera
{
	void Presets::Open(std::string_view a_context, bool a_isFree)
	{
		Close();

		// Still-camera presets only mean something in their own menu, so they need its name
		if (!a_isFree && a_context.empty())
			return;

		auto name = ToFileName(a_context);
		if (!a_isFree && name.empty())
			return;

		_isFree   = a_isFree;
		auto root = ::Settings::Core.GetConfigDirectory() + "camera\\";
		if (a_isFree) {
			_folder = root + "free\\";
		} else {
			_listFile = root + "still\\" + name + ".json";
		}
		Load();
	}

	void Presets::Close()
	{
		_folder.clear();
		_listFile.clear();
		_presets.clear();
		_files.clear();
		_last.reset();
	}

	void Presets::Load()
	{
		if (_isFree) {
			LoadFolder();
		} else {
			LoadList();
		}
		Sort();
	}

	void Presets::LoadList()
	{
		std::error_code ec;
		if (!fs::exists(_listFile, ec))
			return;

		// Unknown keys are skipped, so a file written by a later version still loads
		PresetList  list;
		std::string buffer;
		if (auto err = glz::read_file_json<glz::opts{ .error_on_unknown_keys = false }>(list, _listFile, buffer)) {
			logger::warn("Camera presets: couldn't read {} ({})", _listFile, glz::format_error(err, buffer));
			return;
		}

		for (auto& preset : list.presets) {
			if (!preset.offset || preset.name.empty() || Find(preset.name))
				continue;

			_presets.push_back(std::move(preset));
			_files.emplace_back();
		}

		_last = list.last;
	}

	void Presets::LoadFolder()
	{
		std::error_code ec;
		if (!fs::is_directory(_folder, ec))
			return;

		for (const auto& entry : fs::directory_iterator(_folder, ec)) {
			if (!entry.is_regular_file(ec) || entry.path().extension() != ".json")
				continue;

			// Unknown keys are skipped, so a file written by a later version still loads
			Preset      preset;
			std::string path = entry.path().string();
			std::string buffer;
			if (auto err = glz::read_file_json<glz::opts{ .error_on_unknown_keys = false }>(preset, path, buffer)) {
				logger::warn("Camera presets: couldn't read {} ({})", path, glz::format_error(err, buffer));
				continue;
			}

			// Only the kind this folder holds, in case a file was moved by hand
			if (_isFree ? !preset.free : !preset.offset)
				continue;

			if (preset.name.empty()) {
				preset.name = entry.path().stem().string();
			}

			if (Find(preset.name)) {
				logger::warn("Camera presets: skipped {}, as another file has the name '{}'", path, preset.name);
				continue;
			}

			_presets.push_back(std::move(preset));
			_files.push_back(std::move(path));
		}
	}

	void Presets::Sort()
	{
		std::vector<std::size_t> order(_presets.size());
		std::iota(order.begin(), order.end(), std::size_t{ 0 });
		std::ranges::stable_sort(order, [&](std::size_t a, std::size_t b) { return LessByName(_presets[a].name, _presets[b].name); });

		std::vector<Preset>      presets;
		std::vector<std::string> files;
		presets.reserve(order.size());
		files.reserve(order.size());
		for (auto i : order) {
			presets.push_back(std::move(_presets[i]));
			files.push_back(std::move(_files[i]));
		}
		_presets = std::move(presets);
		_files   = std::move(files);
	}

	void Presets::WriteList() const
	{
		std::error_code ec;
		fs::create_directories(fs::path(_listFile).parent_path(), ec);

		PresetList  list{ .presets = _presets, .last = _last };
		std::string buffer;
		if (auto err = glz::write_file_json(list, _listFile, buffer)) {
			logger::warn("Camera presets: couldn't write {} ({})", _listFile, glz::format_error(err, buffer));
		}
	}

	void Presets::Write(std::size_t a_index) const
	{
		if (!_isFree) {
			WriteList();
			return;
		}

		if (a_index >= _presets.size())
			return;

		std::error_code ec;
		fs::create_directories(_folder, ec);

		std::string buffer;
		if (auto err = glz::write_file_json(_presets[a_index], _files[a_index], buffer)) {
			logger::warn("Camera presets: couldn't write {} ({})", _files[a_index], glz::format_error(err, buffer));
		}
	}

	std::string Presets::MakeFilePath(std::string_view a_name) const
	{
		auto stem = ToFileName(a_name);
		if (stem.empty()) {
			stem = "Preset";
		}

		// Another name can come out the same once made safe, so number it rather than overwrite
		std::error_code ec;
		auto            path = _folder + stem + ".json";
		for (int i = 2; fs::exists(path, ec) || std::ranges::find(_files, path) != _files.end(); ++i) {
			path = std::format("{}{} ({}).json", _folder, stem, i);
		}
		return path;
	}

	const Preset* Presets::Find(std::string_view a_name) const
	{
		auto it = std::ranges::find(_presets, a_name, &Preset::name);
		return it != _presets.end() ? &*it : nullptr;
	}

	const Preset* Presets::FindByHotkey(int a_hotkey) const
	{
		if (a_hotkey <= 0)
			return nullptr;

		auto it = std::ranges::find(_presets, a_hotkey, &Preset::hotkey);
		return it != _presets.end() ? &*it : nullptr;
	}

	void Presets::Store(Preset a_preset)
	{
		if (!IsOpen() || a_preset.name.empty())
			return;

		auto it = std::ranges::find(_presets, a_preset.name, &Preset::name);
		if (it != _presets.end()) {
			a_preset.hotkey = it->hotkey;
			*it             = std::move(a_preset);
			Write(static_cast<std::size_t>(it - _presets.begin()));
			return;
		}

		auto name = a_preset.name;
		_files.push_back(_isFree ? MakeFilePath(name) : std::string());
		_presets.push_back(std::move(a_preset));
		Sort();

		auto added = std::ranges::find(_presets, name, &Preset::name);
		Write(static_cast<std::size_t>(added - _presets.begin()));
	}

	bool Presets::Remove(std::string_view a_name)
	{
		auto it = std::ranges::find(_presets, a_name, &Preset::name);
		if (it == _presets.end())
			return false;

		auto index = static_cast<std::size_t>(it - _presets.begin());

		if (_isFree) {
			std::error_code ec;
			fs::remove(_files[index], ec);
			if (ec) {
				logger::warn("Camera presets: couldn't delete {} ({})", _files[index], ec.message());
			}
		}

		_presets.erase(_presets.begin() + index);
		_files.erase(_files.begin() + index);

		if (!_isFree) {
			WriteList();
		}
		return true;
	}

	bool Presets::SetHotkey(std::string_view a_name, int a_hotkey)
	{
		auto it = std::ranges::find(_presets, a_name, &Preset::name);
		if (it == _presets.end() || a_hotkey < 0 || a_hotkey > 9)
			return false;

		if (a_hotkey > 0) {
			for (std::size_t i = 0; i < _presets.size(); ++i) {
				if (_presets[i].hotkey == a_hotkey && &_presets[i] != &*it) {
					_presets[i].hotkey = 0;
					if (_isFree)
						Write(i);
				}
			}
		}

		// The list file holds them all, so one write covers both changes
		it->hotkey = a_hotkey;
		Write(static_cast<std::size_t>(it - _presets.begin()));
		return true;
	}

	void Presets::StoreLast(const OffsetPreset& a_last)
	{
		if (_isFree || _listFile.empty())
			return;

		_last = a_last;
		WriteList();
	}

	std::string Presets::MakeUniqueName(std::string_view a_base) const
	{
		for (int i = 1;; ++i) {
			auto name = std::format("{} {}", a_base, i);
			if (!Find(name))
				return name;
		}
	}
}
