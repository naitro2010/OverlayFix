#pragma once
#include "SkseMessagingListener.h"

namespace plugin {
    class Update3DModelOverlayFix;
    class GameEventHandler : public SkseMessagingListener<GameEventHandler> {
        private:
            GameEventHandler() {
                registerListener();
            };
            GameEventHandler(GameEventHandler&) = delete;
            GameEventHandler& operator=(GameEventHandler&&) = delete;
            void operator=(GameEventHandler&) = delete;
            Update3DModelOverlayFix* overlayfix = nullptr;
        public:
            [[nodiscard]] static GameEventHandler& getInstance() {
                static GameEventHandler instance;
                return instance;
            }

            void onLoad() override;
            void onPostLoad() override;
            void onPostPostLoad() override;
            void onInputLoaded() override;
            void onDataLoaded() override;
            void onNewGame() override;
            void onPreLoadGame() override;
            void onPostLoadGame() override;
            void onSaveGame() override;
            void onDeleteGame() override;
    };
}  // namespace plugin
