#pragma warning(disable : 4100 4189)
#include "GameEventHandler.h"
#include "RE/N/NiSmartPointer.h"
#include "REL/Relocation.h"
#include "SKSE/API.h"
#include "SKSE/Interfaces.h"
#include "Hooks.h"
#include <windows.h>
#include <psapi.h>
#include <xbyak/xbyak.h>
#include "ini.h"
#define CRASH_FIX_ALPHA
static bool do_reverse = false;
struct Code : Xbyak::CodeGenerator {
        Code(uint64_t offset) {
            mov(rax, offset);
            jmp(rax);
        }
};
struct DeepCopyCheck : Xbyak::CodeGenerator {
        DeepCopyCheck(uint64_t ok_offset, uint64_t bs_skin_vtable,uint64_t skin_vtable) {
            push(rbx);
            cmp(rcx, 0x0);
            je("BADSKIN");
            mov(eax, dword[rcx + 0x8]);
            cmp(eax, 0x0);
            jle("BADSKIN");
            mov(rax, qword[rcx]);
            mov(rbx, bs_skin_vtable);
            cmp(rax, rbx);
            je("GOODSKIN");
            mov(rbx, skin_vtable);
            cmp(rax, rbx);
            je("GOODSKIN");
            L("BADSKIN");
            pop(rbx);
            mov(rax, 0x0);
            ret();
            L("GOODSKIN");
            pop(rbx);
            mov(rax, ok_offset);
            jmp(rax);
            
        }
};
struct DeepCopyHook : Xbyak::CodeGenerator {
        DeepCopyHook(uint64_t check_offset) {
            mov(rax, check_offset);
            jmp(rax);
        }
};
struct DeepCopyOK : Xbyak::CodeGenerator {
        DeepCopyOK(uint64_t offset) {
            push(rbx);
            push(rsi);
            push(rdi);
            sub(rsp, 0x650);
            mov(qword[rsp + 0x20], (uint64_t) - 0x2);
            mov(rax, offset);
            jmp(rax);
        }
};
struct SKEENullFix : Xbyak::CodeGenerator {
        SKEENullFix(uint64_t offset) {
            push(r8);
            cmp(r8, 0x0);
            jz("L1");
            mov(r8, qword[r8]);
            cmp(r8, 0x0);
            jz("L1");
            mov(rax, offset);
            pop(r8);
            jmp(rax);
            L("L1");
            pop(r8);
            ret();
        }
};
#undef GetObject
namespace plugin {
    void WalkOverlays(RE::NiAVObject* CurrentObject, bool hide,
                      std::function<void(RE::NiPointer<RE::NiNode>, RE::NiPointer<RE::NiAVObject>, uint32_t)>& sort_callback) {
        if (CurrentObject == nullptr) {
            return;
        }
        if (RE::NiNode* node = CurrentObject->AsNode()) {
            for (auto& obj: node->GetChildren()) {
                if (obj.get() != nullptr) {
                    WalkOverlays(obj.get(), hide, sort_callback);
                }
            }
        }

        if (CurrentObject->name.contains("[Ovl")) {
            unsigned long index = 256;
            size_t offset = std::string(CurrentObject->name.c_str()).find_last_of("Ovl", CurrentObject->name.size()) + 1;
            if (offset < CurrentObject->name.size() && ((CurrentObject->name.size() - offset) - 1) > 0) {
                std::string overlay_index_str =
                    std::string(CurrentObject->name.c_str()).substr(offset, ((CurrentObject->name.size() - offset) - 1));
                const char* index_cstr = overlay_index_str.c_str();
                index = strtoul(index_cstr, NULL, 10);
            }
            if (do_reverse == true) {
                sort_callback(RE::NiPointer(CurrentObject->parent), RE::NiPointer(CurrentObject), (uint32_t) index);
            }
            RE::BSGeometry* geo = CurrentObject->AsGeometry();
            if (geo != nullptr) {
                auto geodata = geo->GetGeometryRuntimeData();
                if (geodata.properties[1].get() != nullptr && geodata.properties[1].get()->GetType() == RE::NiShadeProperty::Type::kShade) {
                    auto shader_prop = (RE::BSLightingShaderProperty*) (geodata.properties[1].get());
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
            unsigned long index = 256;
            size_t offset = std::string(CurrentObject->name.c_str()).find_last_of("Ovl", CurrentObject->name.size()) + 1;
            if (offset < CurrentObject->name.size() && ((CurrentObject->name.size() - offset) - 1) > 0) {
                std::string overlay_index_str =
                    std::string(CurrentObject->name.c_str()).substr(offset, ((CurrentObject->name.size() - offset) - 1));
                const char* index_cstr = overlay_index_str.c_str();
                index = strtoul(index_cstr, NULL, 10);
            }
            if (do_reverse == true) {
                sort_callback(RE::NiPointer(CurrentObject->parent), RE::NiPointer(CurrentObject), (uint32_t) index);
            }
            RE::BSGeometry* geo = CurrentObject->AsGeometry();
            if (geo != nullptr) {
                auto geodata = geo->GetGeometryRuntimeData();
                if (geodata.properties[1].get() != nullptr && geodata.properties[1].get()->GetType() == RE::NiShadeProperty::Type::kShade) {
                    auto shader_prop = (RE::BSLightingShaderProperty*) (geodata.properties[1].get());
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
    class Update3DModelOverlayFix : public RE::BSTEventSink<SKSE::NiNodeUpdateEvent> {
            RE::BSEventNotifyControl ProcessEvent(const SKSE::NiNodeUpdateEvent* a_event,
                                                  RE::BSTEventSource<SKSE::NiNodeUpdateEvent>* a_eventSource) {
                if (a_event && a_event->reference) {
                    std::map<RE::NiAVObject*, uint32_t> object_to_overlay_index_map;
                    std::map<RE::NiNode*, std::map<uint32_t, RE::NiAVObject*>> reverse_map;
                    auto reverse_map_ptr = &reverse_map;
                    auto oto_map_ptr = &object_to_overlay_index_map;
                    auto callback = [=](RE::NiPointer<RE::NiNode> parent, RE::NiPointer<RE::NiAVObject> obj, uint32_t index) {
                        if (!reverse_map_ptr->contains(parent.get())) {
                            std::map<uint32_t, RE::NiAVObject*> obj_map;
                            reverse_map_ptr->insert_or_assign(parent.get(), obj_map);
                        }
                        if (parent.get() && obj.get()) {
                            auto& m = reverse_map_ptr->at(parent.get());
                            m.insert_or_assign(obj->parentIndex, obj.get());
                            oto_map_ptr->insert_or_assign(obj.get(), index);
                        }
                    };
                    std::function<void(RE::NiPointer<RE::NiNode>, RE::NiPointer<RE::NiAVObject>, uint32_t)> callback_fn = callback;
                    WalkOverlays(a_event->reference->GetCurrent3D(), false, callback_fn);
                    for (auto& node_pair: reverse_map) {
                        std::map<RE::NiAVObject*, uint32_t> original_indices;
                        std::map<RE::NiAVObject*, uint32_t> new_indices;
                        for (auto& obj_pair: node_pair.second) {
                            original_indices.insert_or_assign(obj_pair.second, obj_pair.second->parentIndex);
                        }
                        std::vector<RE::NiAVObject*> keys;
                        for (auto p: original_indices) {
                            keys.push_back(p.first);
                        }
                        int new_index = 0;
                        if (keys.size() >= 2) {
                            if (original_indices[keys[0]] < original_indices[keys[1]]) {
                                for (int i = (int) original_indices.size() - 1; i >= 0; i -= 1) {
                                    new_indices.insert_or_assign(keys[new_index], original_indices[keys[i]]);
                                    new_index += 1;
                                }

                                std::map<uint32_t, RE::NiPointer<RE::NiAVObject>> child_objects;
                                for (auto index_pair: original_indices) {
                                    RE::NiPointer<RE::NiAVObject> temporary;

                                    node_pair.first->DetachChildAt(index_pair.second, temporary);
                                    child_objects.insert_or_assign(new_indices[index_pair.first], temporary);
                                }
                                for (auto& obj_pair: child_objects) {
                                    node_pair.first->InsertChildAt(obj_pair.first, obj_pair.second.get());
                                }
                            }
                        }
                    }
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
    std::recursive_mutex g_name_mutex;
    std::map<RE::FormID, std::string> ExtraNames;
    const char* GetFullNameHooked(RE::TESForm* form) {
        std::lock_guard<std::recursive_mutex> lock(g_name_mutex);
        if (form != nullptr) {
            if (form->GetName() != nullptr && form->GetName()[0] != 0x0) {
                return form->GetName();
            } else {
                if (ExtraNames.contains(form->formID)) {
                    return ExtraNames[form->formID].c_str();
                } else {
                    ExtraNames.insert(std::pair(form->formID, std::format("{:08X}", (uint32_t) form->formID)));
                    return ExtraNames[form->formID].c_str();
                }
            }
        }
        return "";
    }
    static SKEENullFix* nullSkeletonFix;
    static DeepCopyHook* deepCopyHook;
    static DeepCopyCheck* deepCopyCheck;
    static DeepCopyOK* deepCopyOk;
    static std::atomic<uint32_t> skee_loaded = 0;
    static std::atomic<uint32_t> samrim_loaded = 0;
    void GameEventHandler::onPostPostLoad() {
        mINI::INIFile file("Data\\skse\\plugins\\OverlayFix.ini");
        mINI::INIStructure ini;
        if (file.read(ini) == false) {
            ini["OverlayFix"]["reverse"] = "default";
            file.generate(ini);
        } else {
            if (ini["OverlayFix"]["reverse"] == "true") {
                do_reverse = true;
            } else if (ini["OverlayFix"]["reverse"] == "false") {
                do_reverse = false;
            }
        }
        if (HMODULE handle = GetModuleHandleA("skee64.dll")) {
            MODULEINFO skee64_info;
            GetModuleInformation(GetCurrentProcess(), handle, &skee64_info, sizeof(skee64_info));
            uint32_t expected = 0;
            if (skee_loaded.compare_exchange_strong(expected, 1) == true && expected == 0) {
                logger::info("Got SKEE64 information");
                uint8_t signature1170[] = {0xff, 0x90, 0xf0, 0x03, 0x00, 0x00};
                if ((skee64_info.SizeOfImage >= 0xc2950 + 0x40) &&
                    memcmp(signature1170, (void*) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xc2950 + (uintptr_t) 0x28),
                           sizeof(signature1170)) == 0) {
                    uintptr_t patch0 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1cea8);
                    uintptr_t patch1 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1cebd);
                    uintptr_t patch2 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1cec8);
                    uintptr_t patch3 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1bd58);
                    uintptr_t patch4 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1bd6a);
                    REL::safe_write(patch0, (uint8_t*) "\x8b\xca\x90\x90", 4);
                    REL::safe_write(patch1, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch2, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch3, (uint8_t*) "\x8b\xd1\x90\x90", 4);
                    REL::safe_write(patch4, (uint8_t*) "\x90\x90", 2);
                    nullSkeletonFix = new SKEENullFix((uint64_t) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xd5d20));
                    const uint8_t* nullSkeletonCode = nullSkeletonFix->getCode();
                    REL::safe_write(((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1e21d8), (uint8_t*) (&nullSkeletonCode),
                                    sizeof(uint64_t));
                    if (ini["OverlayFix"]["reverse"] == "default") {
                        do_reverse = true;
                    }
#ifdef CRASH_FIX_ALPHA
                    auto skin_vtable=REL::Offset(0x19b0718).address();
                    auto deepcopy_addr = (uintptr_t) REL::Offset(0xd18080).address();
                    auto deepcopy_okret_addr = (uintptr_t) REL::Offset(0xd18094).address();
                    deepCopyOk=new DeepCopyOK ((uint64_t) deepcopy_okret_addr);
                    const uint64_t deepcopy_ok = (uint64_t)deepCopyOk->getCode();
                    deepCopyCheck=new DeepCopyCheck ((uint64_t) deepcopy_ok,skin_vtable,skin_vtable);
                    deepCopyHook=new DeepCopyHook ((uint64_t) deepCopyCheck->getCode());
                    const uint8_t* deepcopy_hook_code = deepCopyHook->getCode();
                    REL::safe_write(deepcopy_addr, deepcopy_hook_code, deepCopyHook->getSize());
#endif                                   
                    logger::info("SKEE64 patched");
                } else if ((skee64_info.SizeOfImage >= 0x16b478 + 7) &&
                           memcmp("BODYTRI", (void*) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x16b478), 7) == 0) {
                    uintptr_t patch0 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x18438);
                    uintptr_t patch1 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1844d);
                    uintptr_t patch2 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x18458);
                    uintptr_t patch3 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x7798);
                    uintptr_t patch4 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x77aa);
                    REL::safe_write(patch0, (uint8_t*) "\x8b\xca\x90\x90", 4);
                    REL::safe_write(patch1, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch2, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch3, (uint8_t*) "\x8b\xd1\x90\x90", 4);
                    REL::safe_write(patch4, (uint8_t*) "\x90\x90", 2);
                    logger::info("SKEE64 0416 patched");
                } else if ((skee64_info.SizeOfImage >= 0x17ec68 + 7) &&
                           memcmp("BODYTRI", (void*) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x17ec68), 7) == 0) {
                    uintptr_t patch0 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xbfd8);
                    uintptr_t patch1 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xbfed);
                    uintptr_t patch2 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xbff8);
                    uintptr_t patch3 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xafd8);
                    uintptr_t patch4 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xafea);
                    REL::safe_write(patch0, (uint8_t*) "\x8b\xca\x90\x90", 4);
                    REL::safe_write(patch1, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch2, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch3, (uint8_t*) "\x8b\xd1\x90\x90", 4);
                    REL::safe_write(patch4, (uint8_t*) "\x90\x90", 2);
                    logger::info("SKEE64 04194 patched");
                } else if ((skee64_info.SizeOfImage >= 0x178b18 + 7) &&
                           memcmp("BODYTRI", (void*) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x178b18), 7) == 0) {
                    // unofficial 1179 GOG patch version
                    uintptr_t patch0 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x25548);
                    uintptr_t patch1 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x2555d);
                    uintptr_t patch2 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x25568);
                    uintptr_t patch3 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xe6f8);
                    uintptr_t patch4 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xe70a);
                    REL::safe_write(patch0, (uint8_t*) "\x8b\xca\x90\x90", 4);
                    REL::safe_write(patch1, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch2, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch3, (uint8_t*) "\x8b\xd1\x90\x90", 4);
                    REL::safe_write(patch4, (uint8_t*) "\x90\x90", 2);
                    if (ini["OverlayFix"]["reverse"] == "default") {
                        do_reverse = true;
                    }
                    logger::info("SKEE64 U1179 GOG patched");
                } else if ((skee64_info.SizeOfImage >= 0x16bce8 + 7) &&
                           memcmp("BODYTRI", (void*) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x16bce8), 7) == 0) {
                    uintptr_t patch0 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x18d18);
                    uintptr_t patch1 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x18d2d);
                    uintptr_t patch2 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x18d38);
                    uintptr_t patch3 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x67a8);
                    uintptr_t patch4 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x67ba);
                    REL::safe_write(patch0, (uint8_t*) "\x8b\xca\x90\x90", 4);
                    REL::safe_write(patch1, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch2, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch3, (uint8_t*) "\x8b\xd1\x90\x90", 4);
                    REL::safe_write(patch4, (uint8_t*) "\x90\x90", 2);
                    logger::info("SKEE64 VR patched");
                } else {
                    logger::error("Wrong SKEE64 version");
                }
            }
        } else if (HMODULE handlevr = GetModuleHandleA("skeevr.dll")) {
            MODULEINFO skee64_info;
            GetModuleInformation(GetCurrentProcess(), handlevr, &skee64_info, sizeof(skee64_info));
            uint32_t expected = 0;
            if (skee_loaded.compare_exchange_strong(expected, 1) == true && expected == 0) {
                logger::info("Got SKEEVR information");
                if ((skee64_info.SizeOfImage >= 0x16bce8 + 7) &&
                    memcmp("BODYTRI", (void*) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x16bce8), 7) == 0) {
                    uintptr_t patch0 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x18d18);
                    uintptr_t patch1 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x18d2d);
                    uintptr_t patch2 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x18d38);
                    uintptr_t patch3 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x67a8);
                    uintptr_t patch4 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x67ba);
                    REL::safe_write(patch0, (uint8_t*) "\x8b\xca\x90\x90", 4);
                    REL::safe_write(patch1, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch2, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch3, (uint8_t*) "\x8b\xd1\x90\x90", 4);
                    REL::safe_write(patch4, (uint8_t*) "\x90\x90", 2);
                    logger::info("SKEE64 VR patched");
                } else {
                    logger::error("Wrong SKEE64 VR version");
                }
            }
        }
#ifdef SAMRIM_NAME_PATCH
        if (HMODULE handlesam = GetModuleHandleA("samrim.dll")) {
            MODULEINFO samrim_info;
            GetModuleInformation(GetCurrentProcess(), handlesam, &samrim_info, sizeof(samrim_info));
            uint32_t expected = 0;
            if (samrim_loaded.compare_exchange_strong(expected, 1) == true && expected == 0) {
                if ((samrim_info.SizeOfImage >= 0x1d2ac8 + 10) &&
                    memcmp("No dyeable", (void*) ((uintptr_t) samrim_info.lpBaseOfDll + (uintptr_t) 0x1d2ac8), 10) == 0) {
                    uintptr_t patch0 = ((uintptr_t) samrim_info.lpBaseOfDll + (uintptr_t) 0x12a320);
                    Code c0((uint64_t) &GetFullNameHooked);
                    const uint8_t* hook0 = c0.getCode();
                    REL::safe_write(patch0, hook0, c0.getSize());
                    logger::info("SAM patched");
                }
            }
        }
#endif
        logger::info("onPostPostLoad()");
    }

    void GameEventHandler::onInputLoaded() {
        logger::info("onInputLoaded()");
    }

    void GameEventHandler::onDataLoaded() {
        logger::info("onDataLoaded()");
    }

    void GameEventHandler::onNewGame() {
        if (overlayfix == nullptr) {
            overlayfix = new Update3DModelOverlayFix();
            SKSE::GetNiNodeUpdateEventSource()->AddEventSink<SKSE::NiNodeUpdateEvent>(overlayfix);
        }
        logger::info("onNewGame()");
    }

    void GameEventHandler::onPreLoadGame() {
        if (overlayfix == nullptr) {
            overlayfix = new Update3DModelOverlayFix();
            SKSE::GetNiNodeUpdateEventSource()->AddEventSink<SKSE::NiNodeUpdateEvent>(overlayfix);
        }
        logger::info("onPreLoadGame()");
    }

    void GameEventHandler::onPostLoadGame() {
        if (overlayfix == nullptr) {
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
