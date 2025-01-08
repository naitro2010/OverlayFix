#include "Hooks.h"
namespace plugin {
    void Hooks::quitGame() {
        logger::info("Game quitting");
        origQuitGame();
    }

    void Hooks::installQuitGameHook() {
        REL::RelocationID hookFuncAddr = REL::RelocationID(35545, 36544);
        int hookFuncOffset = REL::Relocate(0x35, 0x1AE);
        std::uintptr_t hook = hookFuncAddr.address() + hookFuncOffset;
        auto& trampoline = SKSE::GetTrampoline();
        trampoline.create(64);
        origQuitGame = trampoline.write_call<5>(hook, quitGame);
        logger::info("QuitGame hook installed at address 0x{:X} (id {} + 0x{:X})", hookFuncAddr.offset(), hookFuncAddr.id(),
                     hookFuncOffset);
    }
}  // namespace plugin
