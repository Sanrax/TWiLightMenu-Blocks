#ifndef TWLM_LAUNCHPATH_H
#define TWLM_LAUNCHPATH_H

#include <string>
#include <string_view>

inline std::string launchPath(std::string_view directory, std::string_view target) {
	if (target.find(":/") != target.npos)
		return std::string(target);
	if (!target.empty() && target.front() == '/') {
		const size_t colon = directory.find(':');
		return (colon == directory.npos ? std::string{} : std::string(directory.substr(0, colon + 1))) + std::string(target);
	}
	std::string path(directory);
	if (!path.empty() && path.back() != '/') path += '/';
	path.append(target.data(), target.size());
	return path;
}

#endif
