#include "ImGui/GameFonts.h"

#include <zlib.h>

namespace GameFonts
{
	namespace
	{
		constexpr std::array FACE_NAMES{ "$EverywhereFont", "$EverywhereMediumFont", "$EverywhereBoldFont" };

		// SWF fonts are drawn on a 1024 unit em in twentieths, so dividing by ten gives the 2048 unit em TrueType fonts usually use
		constexpr int UNITS_PER_EM = 2048;
		constexpr int UNIT_SCALE   = 10;

		std::optional<float> s_lineHeight;  // The built-in typeface's ascent to descent per em, see SetLineHeight()

		// ==================================================
		// Reading
		// ==================================================

		// Reads a file from the game's data, loose or in an archive
		std::vector<std::uint8_t> ReadGameFile(const std::string& a_path)
		{
			RE::BSResourceNiBinaryStream stream(a_path);
			if (!stream.good() || !stream.stream)
				return {};

			std::vector<std::uint8_t> data(stream.stream->totalSize);
			stream.read(data.data(), static_cast<std::uint32_t>(data.size()));
			return data;
		}

		struct Mapping
		{
			std::string font;
			bool        bold = false;
		};

		struct Config
		{
			std::vector<std::string>             libraries;
			std::array<std::optional<Mapping>, 3> faces;
		};

		std::optional<Config> ReadConfig()
		{
			const auto data = ReadGameFile(R"(Interface\fontconfig.txt)");
			if (data.empty())
				return std::nullopt;

			Config      config;
			std::string text(data.begin(), data.end());

			static const std::regex library(R"re(^\s*fontlib\s+"([^"]+)")re", std::regex::icase);
			static const std::regex map(R"re(^\s*map\s+"([^"]+)"\s*=\s*"([^"]+)"\s*(\w*))re", std::regex::icase);

			std::istringstream lines(text);
			for (std::string line; std::getline(lines, line);) {
				std::smatch match;
				if (std::regex_search(line, match, library)) {
					config.libraries.push_back(match[1]);
				} else if (std::regex_search(line, match, map)) {
					for (std::size_t i = 0; i < FACE_NAMES.size(); ++i) {
						if (match[1] == FACE_NAMES[i])
							config.faces[i] = Mapping{ match[2], _stricmp(match[3].str().c_str(), "Bold") == 0 };
					}
				}
			}
			return config;
		}

		// Reads a font library and decompresses its tags
		std::vector<std::uint8_t> ReadMovie(const std::string& a_path)
		{
			auto file = ReadGameFile(a_path);
			if (file.size() < 8)
				return {};

			const char signature = static_cast<char>(file[0]);
			if (signature == 'F' || signature == 'G')  // Uncompressed, as FWS or Scaleform's GFX
				return { file.begin() + 8, file.end() };
			if (signature != 'C')  // Anything else that isn't zlib compressed (CWS or Scaleform's CFX) isn't a movie
				return {};

			uLongf size = *reinterpret_cast<const std::uint32_t*>(file.data() + 4);
			if (size <= 8)
				return {};
			size -= 8;

			std::vector<std::uint8_t> body(size);
			if (uncompress(body.data(), &size, file.data() + 8, static_cast<uLong>(file.size() - 8)) != Z_OK)
				return {};
			body.resize(size);
			return body;
		}

		// ==================================================
		// DefineFont3
		// ==================================================

		class BitReader
		{
		public:
			BitReader(const std::uint8_t* a_data, std::size_t a_size, std::size_t a_offset) :
				_data(a_data), _bits(a_size * 8), _pos(a_offset * 8)
			{}

			std::uint32_t U(int a_count)
			{
				std::uint32_t value = 0;
				for (int i = 0; i < a_count; ++i, ++_pos) {
					if (_pos >= _bits)
						throw std::out_of_range("glyph");
					value = (value << 1) | ((_data[_pos >> 3] >> (7 - (_pos & 7))) & 1);
				}
				return value;
			}

			std::int32_t S(int a_count)
			{
				const std::uint32_t value = U(a_count);
				return a_count && (value & (1u << (a_count - 1))) ? static_cast<std::int32_t>(value) - (1 << a_count) : static_cast<std::int32_t>(value);
			}

		private:
			const std::uint8_t* _data;
			std::size_t         _bits;
			std::size_t         _pos;
		};

		struct Point
		{
			std::int32_t x;
			std::int32_t y;
			bool         onCurve;
		};
		using Contour = std::vector<Point>;

		struct SwfFont
		{
			std::string                       name;
			bool                              bold = false;
			std::vector<std::vector<Contour>> glyphs;
			std::vector<std::uint16_t>        codes;
			std::vector<std::int16_t>         advances;
			int                               ascent  = 0;
			int                               descent = 0;
			int                               leading = 0;
		};

		template <class T>
		T Read(const std::uint8_t* a_data, std::size_t a_size, std::size_t a_offset)
		{
			if (a_offset + sizeof(T) > a_size)
				throw std::out_of_range("font");
			T value;
			std::memcpy(&value, a_data + a_offset, sizeof(T));
			return value;
		}

		// Reads a glyph's outlines. Each starts with a move, followed by straight edges and quadratic curves, which are the curves TrueType uses
		std::vector<Contour> ReadShape(const std::uint8_t* a_data, std::size_t a_size, std::size_t a_offset)
		{
			BitReader    bits(a_data, a_size, a_offset);
			const int    fillBits = static_cast<int>(bits.U(4));
			const int    lineBits = static_cast<int>(bits.U(4));
			std::int32_t x = 0, y = 0;

			std::vector<Contour> contours;
			auto                 current = [&]() -> Contour& {
                if (contours.empty())
                    contours.push_back({ { x, y, true } });
                return contours.back();
			};

			while (true) {
				if (bits.U(1) == 0) {
					const auto flags = bits.U(5);
					if (flags == 0)
						break;
					if (flags & 0x01) {
						const int moveBits = static_cast<int>(bits.U(5));
						x = bits.S(moveBits);
						y = bits.S(moveBits);
						contours.push_back({ { x, y, true } });
					}
					if (flags & 0x02)
						bits.U(fillBits);
					if (flags & 0x04)
						bits.U(fillBits);
					if (flags & 0x08)
						bits.U(lineBits);
					if (flags & 0x10)
						throw std::runtime_error("glyph styles");
				} else if (bits.U(1) == 1) {
					const int count = static_cast<int>(bits.U(4)) + 2;
					if (bits.U(1)) {
						x += bits.S(count);
						y += bits.S(count);
					} else if (bits.U(1)) {
						y += bits.S(count);
					} else {
						x += bits.S(count);
					}
					current().push_back({ x, y, true });
				} else {
					const int          count = static_cast<int>(bits.U(4)) + 2;
					const std::int32_t cx    = x + bits.S(count);
					const std::int32_t cy    = y + bits.S(count);
					x                        = cx + bits.S(count);
					y                        = cy + bits.S(count);
					current().push_back({ cx, cy, false });
					current().push_back({ x, y, true });
				}
			}
			return contours;
		}

		std::optional<SwfFont> ReadFont(const std::uint8_t* a_data, std::size_t a_size)
		{
			SwfFont font;

			const auto flags      = Read<std::uint8_t>(a_data, a_size, 2);
			const auto nameLength = Read<std::uint8_t>(a_data, a_size, 4);
			if (5u + nameLength > a_size)
				return std::nullopt;
			font.name = std::string(reinterpret_cast<const char*>(a_data + 5), nameLength);
			font.name.erase(std::find(font.name.begin(), font.name.end(), '\0'), font.name.end());
			font.bold = flags & 0x01;

			std::size_t pos   = 5 + nameLength;
			const auto  count = Read<std::uint16_t>(a_data, a_size, pos);
			pos += 2;
			if (count == 0 || !(flags & 0x80))  // Skipped without glyphs, or without the layout that spaces them
				return std::nullopt;

			const bool        wideOffsets = flags & 0x08;
			const bool        wideCodes   = flags & 0x04;
			const std::size_t table       = pos;
			auto              offsetAt    = [&](std::size_t a_index) -> std::size_t {
                return wideOffsets ? Read<std::uint32_t>(a_data, a_size, table + a_index * 4) : Read<std::uint16_t>(a_data, a_size, table + a_index * 2);
			};

			font.glyphs.reserve(count);
			for (std::size_t i = 0; i < count; ++i)
				font.glyphs.push_back(ReadShape(a_data, a_size, table + offsetAt(i)));

			pos = table + offsetAt(count);
			font.codes.reserve(count);
			for (std::size_t i = 0; i < count; ++i) {
				font.codes.push_back(wideCodes ? Read<std::uint16_t>(a_data, a_size, pos) : Read<std::uint8_t>(a_data, a_size, pos));
				pos += wideCodes ? 2 : 1;
			}

			font.ascent  = Read<std::uint16_t>(a_data, a_size, pos);
			font.descent = Read<std::uint16_t>(a_data, a_size, pos + 2);
			font.leading = Read<std::int16_t>(a_data, a_size, pos + 4);
			pos += 6;
			font.advances.reserve(count);
			for (std::size_t i = 0; i < count; ++i)
				font.advances.push_back(Read<std::int16_t>(a_data, a_size, pos + i * 2));

			return font;
		}

		// Finds the first DefineFont3 in a library with the font's name, preferring the weight asked for
		std::optional<SwfFont> FindFont(const std::vector<std::uint8_t>& a_movie, const Mapping& a_mapping)
		{
			if (a_movie.empty())
				return std::nullopt;

			const auto* data = a_movie.data();
			const auto  size = a_movie.size();

			// The tags start after the frame size (a packed rectangle), the frame rate, and the frame count
			const std::size_t rectBits = 5 + (data[0] >> 3) * 4;
			std::size_t       pos      = (rectBits + 7) / 8 + 4;

			std::optional<SwfFont> anyWeight;
			while (pos + 2 <= size) {
				const auto   header = Read<std::uint16_t>(data, size, pos);
				const auto   type   = header >> 6;
				std::size_t  length = header & 0x3F;
				pos += 2;
				if (length == 0x3F) {
					length = Read<std::uint32_t>(data, size, pos);
					pos += 4;
				}
				if (type == 0 || pos + length > size)
					break;

				if (type == 75) {
					const auto nameLength = Read<std::uint8_t>(data, size, pos + 4);
					std::string name(reinterpret_cast<const char*>(data + pos + 5), std::min<std::size_t>(nameLength, length - 5));
					name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
					if (name == a_mapping.font) {
						if (auto font = ReadFont(data + pos, length)) {
							if (font->bold == a_mapping.bold)
								return font;
							if (!anyWeight)
								anyWeight = std::move(font);
						}
					}
				}
				pos += length;
			}
			return anyWeight;
		}

		// ==================================================
		// TrueType
		// ==================================================

		class Writer
		{
		public:
			void U8(std::uint8_t a_value) { bytes.push_back(a_value); }
			void U16(std::uint32_t a_value)
			{
				U8(static_cast<std::uint8_t>(a_value >> 8));
				U8(static_cast<std::uint8_t>(a_value));
			}
			void S16(std::int32_t a_value) { U16(static_cast<std::uint16_t>(static_cast<std::int16_t>(a_value))); }
			void U32(std::uint32_t a_value)
			{
				U16(a_value >> 16);
				U16(a_value & 0xFFFF);
			}
			void Zeros(std::size_t a_count) { bytes.insert(bytes.end(), a_count, 0); }
			void Pad() { Zeros((4 - bytes.size() % 4) % 4); }
			void Append(const std::vector<std::uint8_t>& a_bytes) { bytes.insert(bytes.end(), a_bytes.begin(), a_bytes.end()); }

			std::vector<std::uint8_t> bytes;
		};

		std::int32_t Unit(std::int32_t a_swf) { return static_cast<std::int32_t>(std::lround(a_swf / static_cast<double>(UNIT_SCALE))); }

		std::vector<std::uint8_t> BuildTrueType(const SwfFont& a_font)
		{
			// TrueType wants an empty .notdef glyph first
			const std::size_t count = a_font.glyphs.size() + 1;

			Writer               glyf;
			std::vector<std::uint32_t> loca;
			std::int32_t         xMin = INT32_MAX, yMin = INT32_MAX, xMax = INT32_MIN, yMax = INT32_MIN;
			std::uint16_t        maxPoints = 0, maxContours = 0;

			// FreeType places each glyph by its left edge, so the edges are kept to give each letter its spacing
			std::vector<std::int32_t> bearings(count, 0);

			loca.push_back(0);
			for (std::size_t g = 0; g < a_font.glyphs.size(); ++g) {
				const auto& contours = a_font.glyphs[g];
				loca.push_back(static_cast<std::uint32_t>(glyf.bytes.size()));

				// SWF's y runs down and TrueType's up, so it's flipped. An outline that ends back on its starting point drops the repeated point
				std::vector<Point>         points;
				std::vector<std::uint16_t> ends;
				for (const auto& contour : contours) {
					std::size_t used = contour.size();
					if (used > 1 && contour.back().onCurve && contour.back().x == contour.front().x && contour.back().y == contour.front().y)
						--used;
					for (std::size_t i = 0; i < used; ++i)
						points.push_back({ Unit(contour[i].x), -Unit(contour[i].y), contour[i].onCurve });
					if (used)
						ends.push_back(static_cast<std::uint16_t>(points.size() - 1));
				}
				if (points.empty())
					continue;

				std::int32_t gxMin = INT32_MAX, gyMin = INT32_MAX, gxMax = INT32_MIN, gyMax = INT32_MIN;
				for (const auto& point : points) {
					gxMin = std::min(gxMin, point.x);
					gyMin = std::min(gyMin, point.y);
					gxMax = std::max(gxMax, point.x);
					gyMax = std::max(gyMax, point.y);
				}
				bearings[g + 1] = gxMin;
				xMin        = std::min(xMin, gxMin);
				yMin        = std::min(yMin, gyMin);
				xMax        = std::max(xMax, gxMax);
				yMax        = std::max(yMax, gyMax);
				maxPoints   = std::max(maxPoints, static_cast<std::uint16_t>(points.size()));
				maxContours = std::max(maxContours, static_cast<std::uint16_t>(ends.size()));

				glyf.S16(static_cast<std::int32_t>(ends.size()));
				glyf.S16(gxMin);
				glyf.S16(gyMin);
				glyf.S16(gxMax);
				glyf.S16(gyMax);
				for (const auto end : ends)
					glyf.U16(end);
				glyf.U16(0);  // No instructions
				for (const auto& point : points)
					glyf.U8(point.onCurve ? 1 : 0);
				std::int32_t last = 0;
				for (const auto& point : points) {
					glyf.S16(point.x - last);
					last = point.x;
				}
				last = 0;
				for (const auto& point : points) {
					glyf.S16(point.y - last);
					last = point.y;
				}
				glyf.Pad();
			}
			loca.push_back(static_cast<std::uint32_t>(glyf.bytes.size()));
			if (xMin > xMax)
				xMin = yMin = xMax = yMax = 0;

			std::int32_t       ascent  = Unit(a_font.ascent);
			std::int32_t       descent = Unit(a_font.descent);
			const std::int32_t gap     = Unit(a_font.leading);
			// The built-in typeface's span, divided between ascent and descent as this font divides its own
			if (s_lineHeight && ascent + descent > 0) {
				const double span = *s_lineHeight * UNITS_PER_EM;
				const double up   = static_cast<double>(ascent) / (ascent + descent);
				ascent            = static_cast<std::int32_t>(std::lround(span * up));
				descent           = static_cast<std::int32_t>(std::lround(span * (1.0 - up)));
			}

			std::int32_t maxAdvance = 0;
			for (const auto advance : a_font.advances)
				maxAdvance = std::max(maxAdvance, Unit(advance));

			std::map<std::string, Writer> tables;

			auto& head = tables["head"];
			head.U32(0x10000);
			head.U32(0x10000);
			head.U32(0);  // The checksum adjustment, which FreeType doesn't check
			head.U32(0x5F0F3CF5);
			head.U16(0x0B);
			head.U16(UNITS_PER_EM);
			head.Zeros(16);  // Created and modified dates
			head.S16(xMin);
			head.S16(yMin);
			head.S16(xMax);
			head.S16(yMax);
			head.U16(a_font.bold ? 1 : 0);
			head.U16(8);
			head.S16(2);
			head.S16(1);  // Long loca offsets
			head.S16(0);

			auto& hhea = tables["hhea"];
			hhea.U32(0x10000);
			hhea.S16(ascent);
			hhea.S16(-descent);
			hhea.S16(gap);
			hhea.U16(maxAdvance);
			hhea.S16(xMin);
			hhea.S16(0);
			hhea.S16(xMax);
			hhea.S16(1);
			hhea.Zeros(14);
			hhea.U16(static_cast<std::uint32_t>(count));

			auto& maxp = tables["maxp"];
			maxp.U32(0x10000);
			maxp.U16(static_cast<std::uint32_t>(count));
			maxp.U16(maxPoints);
			maxp.U16(maxContours);
			maxp.U16(0);
			maxp.U16(0);
			maxp.U16(2);
			maxp.Zeros(16);

			auto& hmtx = tables["hmtx"];
			hmtx.U16(0);
			hmtx.S16(0);
			for (std::size_t g = 0; g < a_font.advances.size(); ++g) {
				hmtx.U16(static_cast<std::uint32_t>(std::max(0, Unit(a_font.advances[g]))));
				hmtx.S16(bearings[g + 1]);
			}

			// A format 4 table with one segment per character, then the closing segment TrueType requires
			std::map<std::uint16_t, std::uint16_t> glyphOf;
			for (std::size_t i = 0; i < a_font.codes.size(); ++i) {
				if (a_font.codes[i] != 0xFFFF)
					glyphOf.emplace(a_font.codes[i], static_cast<std::uint16_t>(i + 1));
			}
			const std::uint32_t segments = static_cast<std::uint32_t>(glyphOf.size() + 1);
			std::uint32_t       range    = 1;
			std::uint32_t       selector = 0;
			while (range * 2 <= segments) {
				range *= 2;
				++selector;
			}

			Writer subtable;
			subtable.U16(4);
			subtable.U16(0);  // The length, filled in below
			subtable.U16(0);
			subtable.U16(segments * 2);
			subtable.U16(range * 2);
			subtable.U16(selector);
			subtable.U16(segments * 2 - range * 2);
			for (const auto& [code, glyph] : glyphOf)
				subtable.U16(code);
			subtable.U16(0xFFFF);
			subtable.U16(0);
			for (const auto& [code, glyph] : glyphOf)
				subtable.U16(code);
			subtable.U16(0xFFFF);
			for (const auto& [code, glyph] : glyphOf)
				subtable.U16((glyph - code) & 0xFFFF);
			subtable.U16(1);
			subtable.Zeros(segments * 2);
			subtable.bytes[2] = static_cast<std::uint8_t>(subtable.bytes.size() >> 8);
			subtable.bytes[3] = static_cast<std::uint8_t>(subtable.bytes.size());

			auto& cmap = tables["cmap"];
			cmap.U16(0);
			cmap.U16(1);
			cmap.U16(3);
			cmap.U16(1);
			cmap.U32(12);
			cmap.Append(subtable.bytes);

			auto& os2 = tables["OS/2"];
			os2.U16(4);
			os2.S16(maxAdvance / 2);
			os2.U16(a_font.bold ? 700 : 400);
			os2.U16(5);
			os2.Zeros(2 + 20 + 2);  // Embedding, and the subscript, superscript, strikeout, and family class values
			os2.Zeros(10 + 16);     // PANOSE and the Unicode ranges
			os2.U32(0x4E4F4E45);    // The vendor ID, "NONE"
			os2.U16(a_font.bold ? 0x20 : 0x40);
			os2.U16(glyphOf.empty() ? 0 : glyphOf.begin()->first);
			os2.U16(glyphOf.empty() ? 0 : glyphOf.rbegin()->first);
			os2.S16(ascent);
			os2.S16(-descent);
			os2.S16(gap);
			os2.U16(static_cast<std::uint32_t>(std::max(0, ascent)));
			os2.U16(static_cast<std::uint32_t>(std::max(0, descent)));
			os2.U32(1);
			os2.U32(0);
			os2.Zeros(10);  // x height, cap height, default and break characters, and max context

			// The family and full name, both the font's own name
			std::vector<std::uint8_t> name;
			for (const char c : a_font.name) {
				name.push_back(0);
				name.push_back(static_cast<std::uint8_t>(c));
			}
			auto& nameTable = tables["name"];
			nameTable.U16(0);
			nameTable.U16(2);
			nameTable.U16(6 + 12 * 2);
			for (const std::uint16_t id : { std::uint16_t{ 1 }, std::uint16_t{ 4 } }) {
				nameTable.U16(3);
				nameTable.U16(1);
				nameTable.U16(0x409);
				nameTable.U16(id);
				nameTable.U16(static_cast<std::uint32_t>(name.size()));
				nameTable.U16(0);
			}
			nameTable.Append(name);

			auto& post = tables["post"];
			post.U32(0x30000);
			post.Zeros(28);

			auto& locaTable = tables["loca"];
			for (const auto offset : loca)
				locaTable.U32(offset);

			tables["glyf"] = std::move(glyf);

			// The table directory, sorted by tag as std::map keeps them
			Writer         file;
			const auto     tableCount = static_cast<std::uint32_t>(tables.size());
			std::uint32_t  search     = 1;
			std::uint32_t  log        = 0;
			while (search * 2 <= tableCount) {
				search *= 2;
				++log;
			}
			file.U32(0x10000);
			file.U16(tableCount);
			file.U16(search * 16);
			file.U16(log);
			file.U16(tableCount * 16 - search * 16);

			std::uint32_t offset = 12 + 16 * tableCount;
			for (auto& [tag, table] : tables) {
				const auto length = static_cast<std::uint32_t>(table.bytes.size());  // The length recorded leaves out the padding
				table.Pad();
				std::uint32_t sum = 0;
				for (std::size_t i = 0; i < table.bytes.size(); i += 4)
					sum += (table.bytes[i] << 24) | (table.bytes[i + 1] << 16) | (table.bytes[i + 2] << 8) | table.bytes[i + 3];
				for (const char c : tag)
					file.U8(static_cast<std::uint8_t>(c));
				file.U32(sum);
				file.U32(offset);
				file.U32(length);
				offset += static_cast<std::uint32_t>(table.bytes.size());
			}
			for (auto& [tag, table] : tables)
				file.Append(table.bytes);
			return std::move(file.bytes);
		}

		std::vector<ImWchar> BuildRanges(const SwfFont& a_font)
		{
			std::vector<std::uint16_t> codes(a_font.codes.begin(), a_font.codes.end());
			std::ranges::sort(codes);
			codes.erase(std::unique(codes.begin(), codes.end()), codes.end());
			std::erase(codes, 0);

			std::vector<ImWchar> ranges;
			for (std::size_t i = 0; i < codes.size();) {
				std::size_t last = i;
				while (last + 1 < codes.size() && codes[last + 1] == codes[last] + 1)
					++last;
				ranges.push_back(codes[i]);
				ranges.push_back(codes[last]);
				i = last + 1;
			}
			ranges.push_back(0);
			return ranges;
		}

		std::optional<std::array<Font, 3>> LoadAll()
		{
			const auto start  = std::chrono::steady_clock::now();
			const auto config = ReadConfig();
			if (!config) {
				logger::error("Game fonts: Interface\\fontconfig.txt couldn't be read, so the chosen typeface is used");
				return std::nullopt;
			}

			std::array<Font, 3> fonts;
			if (config->libraries.empty()) {
				logger::error("Game fonts: no font libraries in Interface\\fontconfig.txt, so the built-in typeface is used");
				return fonts;
			}

			std::map<std::string, std::vector<std::uint8_t>> movies;
			for (std::size_t i = 0; i < fonts.size(); ++i) {
				const auto& mapping = config->faces[i];
				if (!mapping) {
					logger::error("Game fonts: fontconfig.txt doesn't map {}", FACE_NAMES[i]);
					continue;
				}

				// A theme often maps every face to the same font, so it's only converted once
				const auto same = std::find_if(config->faces.begin(), config->faces.begin() + i, [&](const auto& a_other) {
					return a_other && a_other->font == mapping->font && a_other->bold == mapping->bold;
				});
				if (same != config->faces.begin() + i && !fonts[same - config->faces.begin()].ttf.empty()) {
					fonts[i] = fonts[same - config->faces.begin()];
					logger::info("Game fonts: {} is '{}' too", FACE_NAMES[i], fonts[i].name);
					continue;
				}

				for (const auto& library : config->libraries) {
					auto& movie = movies[library];
					if (movie.empty())
						movie = ReadMovie(library);

					try {
						if (auto font = FindFont(movie, *mapping)) {
							fonts[i] = { font->name, BuildTrueType(*font), BuildRanges(*font) };
							logger::info("Game fonts: {} is '{}'{} from {}, {} glyphs", FACE_NAMES[i], font->name, font->bold ? " (bold)" : "", library, font->glyphs.size());
							break;
						}
					} catch (const std::exception& e) {
						logger::error("Game fonts: couldn't read '{}' in {} ({})", mapping->font, library, e.what());
					}
				}
				if (fonts[i].ttf.empty())
					logger::error("Game fonts: '{}' for {} isn't in any font library fontconfig.txt names", mapping->font, FACE_NAMES[i]);
			}

			const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
			logger::info("Game fonts: read and converted in {:.1f} ms{}", ms, s_lineHeight ? ", sized by the em as the game sizes them" : "");
			return fonts;
		}
	}

	void SetLineHeight(float a_perEm)
	{
		s_lineHeight = a_perEm;
	}

	const Font& Get(Face a_face)
	{
		static const Font                               none;
		static const std::optional<std::array<Font, 3>> fonts = LoadAll();
		return fonts ? (*fonts)[std::to_underlying(a_face)] : none;
	}
}
