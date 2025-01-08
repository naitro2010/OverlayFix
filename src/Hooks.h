#pragma once

namespace plugin {
    class Hooks {
        private:
            static void quitGame();
            static inline REL::Relocation<decltype(quitGame)> origQuitGame;

        public:
            static void installQuitGameHook();
    };

}  // namespace plugin