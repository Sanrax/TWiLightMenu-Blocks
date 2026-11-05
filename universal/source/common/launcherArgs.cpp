// Launcher path and argument helpers shared by every menu.
#include "common/customLaunchers.h"
#include "common/stringtool.h"

std::string LauncherBinary::on(const char *dev) const
{
	if (device == 1)
		return "sd:" + rel;
	if (device == 2)
		return "fat:" + rel;
	return dev + rel;
}

const LauncherBinary *CustomLauncher::variant(const char *name) const
{
	for (const LauncherVariant &v : variants) {
		if (v.name == name)
			return v.binary.empty() ? NULL : &v.binary;
	}
	return NULL;
}

std::string buildLauncherArg(const CustomLauncher &launcher, const std::string &romPath,
			     const std::string &romPathFat, const std::string &filename)
{
	if (launcher.arg.empty())
		return "";

	std::string nameNoExt = filename;
	const size_t dot = nameNoExt.find_last_of('.');
	if (dot != nameNoExt.npos)
		nameNoExt.erase(dot);

	std::string dirPart = romPath;
	const size_t slash = dirPart.find_last_of('/');
	if (slash != dirPart.npos)
		dirPart.erase(slash);

	std::string arg = launcher.arg;
	arg = replaceAll(arg, "%PATH_FAT%", romPathFat.empty() ? romPath : romPathFat);
	arg = replaceAll(arg, "%PATH%", romPath);
	arg = replaceAll(arg, "%NAME_NOEXT%", nameNoExt);
	arg = replaceAll(arg, "%NAME%", filename);
	arg = replaceAll(arg, "%DIR%", dirPart);

	return arg;
}

std::vector<std::string> splitLauncherExtraArgs(const CustomLauncher &launcher)
{
	std::vector<std::string> args;

	size_t pos = 0;
	while (pos < launcher.extraArgs.size()) {
		const size_t start = launcher.extraArgs.find_first_not_of(" \t", pos);
		if (start == launcher.extraArgs.npos)
			break;
		size_t end = launcher.extraArgs.find_first_of(" \t", start);
		if (end == launcher.extraArgs.npos)
			end = launcher.extraArgs.size();
		args.push_back(launcher.extraArgs.substr(start, end - start));
		pos = end;
	}

	return args;
}
