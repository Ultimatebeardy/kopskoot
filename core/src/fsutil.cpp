// SPDX-License-Identifier: GPL-2.0-or-later
#include "killsense/fsutil.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

namespace killsense {

std::filesystem::path pathFromUtf8(const std::string &utf8)
{
#if defined(__cpp_char8_t)
	return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
#else
	return std::filesystem::u8path(utf8);
#endif
}

std::string pathToUtf8(const std::filesystem::path &path)
{
#if defined(__cpp_char8_t)
	const std::u8string s = path.u8string();
	return std::string(s.begin(), s.end());
#else
	return path.u8string();
#endif
}

bool readTextFile(const std::filesystem::path &path, std::string &out, size_t maxBytes)
{
	std::error_code ec;
	const auto size = std::filesystem::file_size(path, ec);
	if (ec || size > maxBytes)
		return false;
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

bool writeTextFileAtomic(const std::filesystem::path &path, const std::string &content)
{
	std::error_code ec;
	if (path.has_parent_path())
		std::filesystem::create_directories(path.parent_path(), ec);
	std::filesystem::path tmp = path;
	tmp += ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		if (!out)
			return false;
		out.write(content.data(), static_cast<std::streamsize>(content.size()));
		out.flush();
		if (!out)
			return false;
	}
	std::filesystem::rename(tmp, path, ec);
	if (ec) {
		// rename() over an existing file can fail on some Windows setups; fall back.
		std::filesystem::remove(path, ec);
		std::filesystem::rename(tmp, path, ec);
		if (ec) {
			std::filesystem::remove(tmp, ec);
			return false;
		}
	}
	return true;
}

} // namespace killsense
