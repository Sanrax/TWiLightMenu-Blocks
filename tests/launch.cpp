#include "launch/launchPlanner.h"
#include "common/filetypes.h"
#include "common/launchpath.h"
#include <cassert>
#include <iostream>

static LauncherBinary binary(const char *name) {
    LauncherBinary result;
    result.rel = name;
    result.onSd = result.onFat = true;
    return result;
}
int main() {
    assert(launchPath("fat:/roms", "emulator.nds") == "fat:/roms/emulator.nds");
    assert(launchPath("fat:/roms/", "sd:/emulator.nds") == "sd:/emulator.nds");
    assert(launchPath("fat:/roms/", "/emulator.nds") == "fat:/emulator.nds");
    std::string filename = "Game.NDSignored";
    std::string suffix = ".ndsnot-a-terminator";
    assert(extension(std::string_view(filename.data(), 8), {std::string_view(suffix.data(), 4)}));
    assert(extension("Test.lz77.sfc", {".sfc", ".lz77.sfc"}));
    assert(!extension(".nds", {".nds"}));
    assert(!extension("game.nds.bak", {".nds"}));

    CustomLauncher launcher;
    launcher.binary = binary("/emulators/nesds.nds");
    launcher.arg = "%PATH%";
    launcher.extraArgs = "-fast\t debug";
    launcher.dataDirs = {"data/nes", "data/nes"};
    LaunchEnv env;
    env.isDSiMode = env.dsiFeatures = env.sdFound = env.isRunFromSD = true;
    LaunchRequest request{"sd:/roms/nes", "Test Game.nes", {"from-argv", "first"}, false};
    auto plan = planLaunch(launcher, env, request);
    assert(plan.ok && !plan.useNDSB && plan.ndsToBoot == "sd:/emulators/nesds.nds");
    assert((plan.args == std::vector<std::string>{"from-argv", "first", "-fast", "debug", "sd:/roms/nes/Test Game.nes"}));
    assert((plan.mkdirs == std::vector<std::string>{"sd:/data", "sd:/data/nes"}));
    assert(!plan.boostVram);
    launcher.binary.device = 2;
    plan = planLaunch(launcher, env, request);
    assert(plan.ndsToBoot == "fat:/emulators/nesds.nds");
    launcher.arg = "%NAME_NOEXT%|%DIR%|%NAME%|%PATH_FAT%";
    assert(buildLauncherArg(launcher, "sd:/roms/a.ext", "fat:/roms/a.ext", "a.ext") == "a|sd:/roms|a.ext|fat:/roms/a.ext");

    launcher = CustomLauncher{};
    launcher.handler = LauncherHandler::Snes;
    launcher.variants = {{"LEGACY", binary("/emulators/snes.nds")}, {"LEGACY_TWLM", binary("/emulators/snes-twlm.nds")}};
    env.arm7SCFGLocked = true;
    request = {"sd:/roms/snes", "Game.lz77.sfc", {"prefix"}, false};
    plan = planLaunch(launcher, env, request);
    assert(plan.ok && plan.useNDSB && plan.writeBootstrapIni);
    assert(plan.ndsToBoot == "sd:/emulators/snes-twlm.nds");
    assert(plan.romToRamDisk == 1 && plan.romIsCompressed);
    assert(plan.bootstrapHomebrewArg == "fat:/ROM.SMC");
    assert(plan.bootstrapRamDrivePath == "sd:/roms/snes/Game.lz77.sfc");
    assert((plan.args == std::vector<std::string>{"prefix", "fat:/roms/snes/Game.lz77.sfc"}));
    request.filename = "Game.sfc";
    plan = planLaunch(launcher, env, request);
    assert(plan.ok && !plan.romIsCompressed && plan.romToRamDisk == -1);
    env.secondaryDevice = true;
    request.romFolder = "fat:/roms/snes";
    plan = planLaunch(launcher, env, request);
    assert(plan.ok && !plan.useNDSB && plan.dsModeSwitch);
    assert(!plan.boostCpu && !plan.boostVram);
    launcher.variants.clear();
    assert(!planLaunch(launcher, env, request).ok);
    std::cout << "extension views, argv order, explicit devices and bootstrap RAM-disk plans passed\n";
}
