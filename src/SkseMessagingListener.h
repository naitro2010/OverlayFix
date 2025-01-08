
/*
MIT License

Copyright (c) 2023 Emerson Pinter

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

template <class T>
class SkseMessagingListener {
    protected:
        SkseMessagingListener() = default;

    private:
        SkseMessagingListener(SkseMessagingListener&) = delete;
        SkseMessagingListener& operator=(SkseMessagingListener&&) = delete;
        void operator=(SkseMessagingListener&) = delete;

    public:
        ~SkseMessagingListener() = default;

        // This plugin is finishing load.
        virtual void onLoad(){};

        // All plugins have finished running SKSEPlugin_Load.
        virtual void onPostLoad(){};

        // All kPostLoad message handlers have run.
        virtual void onPostPostLoad(){};

        // All game data has been found.
        virtual void onInputLoaded(){};

        // All ESM/ESL/ESP plugins have loaded, main menu is now active.
        virtual void onDataLoaded(){};

        // Player starts a new game from main menu.
        virtual void onNewGame(){};

        // Player selected a game to load, but it hasn't loaded yet.
        virtual void onPreLoadGame(){};

        // Player selected save game has finished loading.
        virtual void onPostLoadGame(){};

        // Player has saved a game.
        virtual void onSaveGame(){};

        // Player deleted a saved game from within the load menu.
        virtual void onDeleteGame(){};

        auto registerListener() {
            if (!SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
                    switch (message->type) {
                        case SKSE::MessagingInterface::kPostLoad:
                            T::getInstance().onPostLoad();
                            break;
                        case SKSE::MessagingInterface::kPostPostLoad:
                            T::getInstance().onPostPostLoad();
                            break;
                        case SKSE::MessagingInterface::kInputLoaded:
                            T::getInstance().onInputLoaded();
                            break;
                        case SKSE::MessagingInterface::kDataLoaded:
                            T::getInstance().onDataLoaded();
                            break;
                        case SKSE::MessagingInterface::kNewGame:
                            T::getInstance().onNewGame();
                            break;
                        case SKSE::MessagingInterface::kPreLoadGame:
                            T::getInstance().onPreLoadGame();
                            break;
                        case SKSE::MessagingInterface::kPostLoadGame:
                            T::getInstance().onPostLoadGame();
                            break;
                        case SKSE::MessagingInterface::kSaveGame:
                            T::getInstance().onSaveGame();
                            break;
                        case SKSE::MessagingInterface::kDeleteGame:
                            T::getInstance().onDeleteGame();
                            break;
                    }
                })) {
                SKSE::stl::report_and_fail("Unable to register message listener.");
            }
        }
};
