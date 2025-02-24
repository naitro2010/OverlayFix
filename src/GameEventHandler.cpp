#pragma warning(disable: 4100 4189)
#include "GameEventHandler.h"
#include "RE/N/NiSmartPointer.h"
#include "REL/Relocation.h"
#include "SKSE/API.h"
#include "SKSE/Interfaces.h"
#include "Hooks.h"
#include <windows.h>
#include <psapi.h>
#undef GetObject
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
    static std::atomic<uint32_t> skee_loaded = 0;
    void GameEventHandler::onPostPostLoad() {
        if (HMODULE handle = GetModuleHandleA("skee64.dll")) {
			MODULEINFO skee64_info;
			GetModuleInformation(GetCurrentProcess(), handle, &skee64_info, sizeof(skee64_info));
			uint32_t expected = 0;
			if (skee_loaded.compare_exchange_strong(expected, 1) == true && expected == 0) {
				logger::info("Got SKEE64 information");
				uint8_t signature1170[] = { 0xff, 0x90, 0xf0, 0x03, 0x00, 0x00 };
				if ((skee64_info.SizeOfImage >= 0xc2950+0x40) && memcmp(signature1170, (void*)((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0xc2950 + (uintptr_t)0x28), sizeof(signature1170)) == 0) {
					uintptr_t patch0=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x1cea8);
                    uintptr_t patch1=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x1cebd);
                    uintptr_t patch2=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x1cec8);
                    uintptr_t patch3=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x1bd58);
                    uintptr_t patch4=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x1bd6a);
		            REL::safe_write(patch0,(uint8_t*)"\x8b\xca\x90\x90",4);
                    REL::safe_write(patch1,(uint8_t*)"\x90\x90\x90\x90\x90\x90\x90\x90",8);
                    REL::safe_write(patch2,(uint8_t*)"\x90\x90\x90\x90\x90\x90\x90\x90",8);
                    REL::safe_write(patch3,(uint8_t*)"\x8b\xd1\x90\x90",4);
                    REL::safe_write(patch4,(uint8_t*)"\x90\x90",2);
					logger::info("SKEE64 patched");
				}
                else if ((skee64_info.SizeOfImage >= 0x16b478+7) && memcmp("BODYTRI",(void*)((uintptr_t)skee64_info.lpBaseOfDll+(uintptr_t)0x16b478),7) == 0) {
                    uintptr_t patch0=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x18438);
                    uintptr_t patch1=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x1844d);
                    uintptr_t patch2=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x18458);
                    uintptr_t patch3=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x7798);
                    uintptr_t patch4=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x77aa);
                    REL::safe_write(patch0,(uint8_t*)"\x8b\xca\x90\x90",4);
                    REL::safe_write(patch1,(uint8_t*)"\x90\x90\x90\x90\x90\x90\x90\x90",8);
                    REL::safe_write(patch2,(uint8_t*)"\x90\x90\x90\x90\x90\x90\x90\x90",8);
                    REL::safe_write(patch3,(uint8_t*)"\x8b\xd1\x90\x90",4);
                    REL::safe_write(patch4,(uint8_t*)"\x90\x90",2);
                    logger::info("SKEE64 0416 patched");
                }
                else if ((skee64_info.SizeOfImage >= 0x17ec68+7) && memcmp("BODYTRI",(void*)((uintptr_t)skee64_info.lpBaseOfDll+(uintptr_t)0x17ec68),7) == 0) {
                    uintptr_t patch0=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0xbfd8);
                    uintptr_t patch1=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0xbfed);
                    uintptr_t patch2=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0xbff8);
                    uintptr_t patch3=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0xafd8);
                    uintptr_t patch4=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0xafea);
                    REL::safe_write(patch0,(uint8_t*)"\x8b\xca\x90\x90",4);
                    REL::safe_write(patch1,(uint8_t*)"\x90\x90\x90\x90\x90\x90\x90\x90",8);
                    REL::safe_write(patch2,(uint8_t*)"\x90\x90\x90\x90\x90\x90\x90\x90",8);
                    REL::safe_write(patch3,(uint8_t*)"\x8b\xd1\x90\x90",4);
                    REL::safe_write(patch4,(uint8_t*)"\x90\x90",2);
                    logger::info("SKEE64 04194 patched");
                }
                else if ((skee64_info.SizeOfImage >= 0x178b18+7) && memcmp("BODYTRI",(void*)((uintptr_t)skee64_info.lpBaseOfDll+(uintptr_t)0x178b18),7) == 0) {
                    // unofficial 1179 GOG patch version
                    uintptr_t patch0=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x25548);
                    uintptr_t patch1=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x2555d);
                    uintptr_t patch2=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0x25568);
                    uintptr_t patch3=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0xe6f8);
                    uintptr_t patch4=((uintptr_t)skee64_info.lpBaseOfDll + (uintptr_t)0xe70a);
                    REL::safe_write(patch0,(uint8_t*)"\x8b\xca\x90\x90",4);
                    REL::safe_write(patch1,(uint8_t*)"\x90\x90\x90\x90\x90\x90\x90\x90",8);
                    REL::safe_write(patch2,(uint8_t*)"\x90\x90\x90\x90\x90\x90\x90\x90",8);
                    REL::safe_write(patch3,(uint8_t*)"\x8b\xd1\x90\x90",4);
                    REL::safe_write(patch4,(uint8_t*)"\x90\x90",2);
                    logger::info("SKEE64 U1179 GOG patched");
                } else {
                    logger::error("Wrong SKEE64 version");
                }
				
			}
		} else {
			logger::error("Get SKEE64 last error {}", GetLastError());
		}
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
