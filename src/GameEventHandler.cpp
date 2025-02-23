#pragma warning(disable: 4100 4189)
#include "GameEventHandler.h"
#include "Hooks.h"

namespace plugin {
    void WalkOverlays(RE::NiAVObject* CurrentObject, bool hide)
    {
        if (CurrentObject == nullptr) {
            return;
        }
        if (RE::NiNode* node = CurrentObject->AsNode()) {
            for (auto& obj : node->GetChildren()) {
                if (obj.get() != nullptr) {
                    WalkOverlays(obj.get(), hide);
                }
            }
        }
        if (CurrentObject->name.contains("[Ovl")) {
            RE::BSGeometry* geo = CurrentObject->AsGeometry();
            if (geo != nullptr) {
                auto geodata = geo->GetGeometryRuntimeData();
                if (geodata.properties[1].get() != nullptr && geodata.properties[1].get()->GetType() == RE::NiShadeProperty::Type::kShade) {
                    auto shader_prop = (RE::BSLightingShaderProperty*)(geodata.properties[1].get());
                    if (shader_prop != nullptr) {
                        shader_prop->SetupGeometry(geo);
                        shader_prop->FinishSetupGeometry(geo);
                    }
                }
                geo = geo;
            }
            return;
        }
        if (CurrentObject->name.contains("[SOvl")) {
            RE::BSGeometry* geo = CurrentObject->AsGeometry();
            if (geo != nullptr) {
                auto geodata = geo->GetGeometryRuntimeData();
                if (geodata.properties[1].get() != nullptr && geodata.properties[1].get()->GetType() == RE::NiShadeProperty::Type::kShade) {
                    auto shader_prop = (RE::BSLightingShaderProperty*)(geodata.properties[1].get());
                    if (shader_prop != nullptr) {
                        shader_prop->SetupGeometry(geo);
                        shader_prop->FinishSetupGeometry(geo);
                    }
                }
                geo = geo;
            }
            return;
        }
    }
    class Update3DModelOverlayFix : public RE::BSTEventSink<SKSE::NiNodeUpdateEvent>
    {
        RE::BSEventNotifyControl ProcessEvent(const SKSE::NiNodeUpdateEvent* a_event, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>* a_eventSource)
        {
            if (a_event && a_event->reference) {
                WalkOverlays(a_event->reference->GetCurrent3D(), false);
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };
    void GameEventHandler::onLoad() {
        logger::info("onLoad()");
    }

    void GameEventHandler::onPostLoad() {
        logger::info("onPostLoad()");
    }

    void GameEventHandler::onPostPostLoad() {
        logger::info("onPostPostLoad()");
        
    }

    void GameEventHandler::onInputLoaded() {
        logger::info("onInputLoaded()");
    }

    void GameEventHandler::onDataLoaded() {
        logger::info("onDataLoaded()");
    }

    void GameEventHandler::onNewGame() {
        if (overlayfix==nullptr)
        {
            overlayfix = new Update3DModelOverlayFix();
            SKSE::GetNiNodeUpdateEventSource()->AddEventSink<SKSE::NiNodeUpdateEvent>(overlayfix);
        }
        logger::info("onNewGame()");
    }

    void GameEventHandler::onPreLoadGame() {
        if (overlayfix==nullptr)
        {
            overlayfix = new Update3DModelOverlayFix();
            SKSE::GetNiNodeUpdateEventSource()->AddEventSink<SKSE::NiNodeUpdateEvent>(overlayfix);
        }
        logger::info("onPreLoadGame()");
    }

    void GameEventHandler::onPostLoadGame() {
        if (overlayfix==nullptr)
        {
            overlayfix = new Update3DModelOverlayFix();
            SKSE::GetNiNodeUpdateEventSource()->AddEventSink<SKSE::NiNodeUpdateEvent>(overlayfix);
        }
        logger::info("onPostLoadGame()");
    }

    void GameEventHandler::onSaveGame() {
        logger::info("onSaveGame()");
    }

    void GameEventHandler::onDeleteGame() {
        logger::info("onDeleteGame()");
    }
}  // namespace plugin
