#ifndef TWLM_FILETYPES_H
#define TWLM_FILETYPES_H

#include <initializer_list>
#include <string_view>
#include <strings.h>
#include <vector>

namespace twlm {
template <typename Extensions>
inline bool matchesExtension(std::string_view filename, const Extensions &extensions) {
	for (std::string_view suffix : extensions) {
		if (filename.size() > suffix.size() &&
			strncasecmp(filename.data() + filename.size() - suffix.size(), suffix.data(), suffix.size()) == 0)
			return true;
	}
	return false;
}
}

// Braced literal lists don't allocate; configured extension lists aren't copied.
inline bool extension(std::string_view filename, std::initializer_list<std::string_view> extensions) {
	return twlm::matchesExtension(filename, extensions);
}

inline bool extension(std::string_view filename, const std::vector<std::string_view> &extensions) {
	return twlm::matchesExtension(filename, extensions);
}

#endif
