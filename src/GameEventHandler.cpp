#pragma warning(disable : 4100 4189)
#include "GameEventHandler.h"
#include "RE/N/NiSmartPointer.h"
#include "REL/Relocation.h"
#include "SKSE/API.h"
#include "SKSE/Interfaces.h"
#include "Hooks.h"
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <xbyak/xbyak.h>
#include "ini.h"
#include "detours/detours.h"
#define CRASH_FIX_ALPHA
#define DISMEMBER_CRASH_FIX_ALPHA
#define STEAMDECK_CRASH_FIX
#define SKSE_COSAVE_STACK_WORKAROUND
#define MORPHCACHE_SHRINK_WORKAROUND
#define PARALLEL_MORPH_WORKAROUND
#define SAMRIM_NAME_PATCH
#ifdef SAMRIM_NAME_PATCH
#include "thirdparty/DDNG_API.h"
#endif
static bool do_reverse = false;
static bool print_flags = true;
static bool overlay_culling_fix = true;
struct Code : Xbyak::CodeGenerator {
        Code(uint64_t offset) {
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
static auto CoSaveDropStacksAddr = (void (*)(void*)) 0x0;
static void CoSaveDropStacksBypass(void*) {
    return;
}
#ifdef VR_ESL_SUPPORT
static auto LookupFormSKEEVRAddr = (RE::TESForm * (*) (RE::FormID)) 0x0;
static RE::TESForm* LookupFormSKEEVR(RE::FormID id) {
    if (auto data_handler = RE::TESDataHandler::GetSingleton()) {
        if ((id & 0xFF000000) == 0xFE000000) {
            if (auto mod_file = data_handler->LookupLoadedLightModByIndex((uint16_t) ((id & 0xFFF000) >> 12))) {
                return data_handler->LookupForm(data_handler->LookupFormIDRaw(id, mod_file->fileName), mod_file->fileName);
            }
        } else {
            if (auto mod_file = data_handler->LookupLoadedModByIndex(id >> 24)) {
                return data_handler->LookupForm(data_handler->LookupFormIDRaw(id, mod_file->fileName), mod_file->fileName);
            }
        }
    }
    return nullptr;
}
#endif
/*
static auto CoSaveStoreLogAddr = (void (*)(void*,void*,unsigned int)) 0x0;
static void CoSaveStoreLog(void* cosaveinterface, void * obj, unsigned int stackID) {
    logger::info("Co-Save logging starts here {:016X} {:08X}",(DWORD64)obj,stackID);
    void* frames[256];
    unsigned short frame_count;
    SYMBOL_INFO_PACKAGE symbol;
    symbol.si.MaxNameLen = MAX_SYM_NAME;
    symbol.si.SizeOfStruct = sizeof(SYMBOL_INFO);
    SymInitialize(GetCurrentProcess(), NULL, TRUE); 
    frame_count=CaptureStackBackTrace(0, 256, frames, NULL);
    for (int i = 0; i < frame_count; i++) {
        MEMORY_BASIC_INFORMATION frame_info;
        if (VirtualQuery(frames[i], &frame_info, sizeof(frame_info)) == 0) {
            continue;
        }
        char mod_name[2801] = "";
        GetModuleFileNameA((HMODULE) frame_info.AllocationBase, mod_name, 2800);
        logger::info("{} {:016X} {:016X} {} {:016X}",i, (DWORD64) frames[i],
                     (DWORD64)frames[i] - (DWORD64)frame_info.AllocationBase,mod_name,(DWORD64)frame_info.AllocationBase);
    }
    
    CoSaveStoreLogAddr(cosaveinterface,obj,stackID);
    logger::info("Co-Save logging ends here");
}*/
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
                if (do_reverse == true) {
                    sort_callback(RE::NiPointer(CurrentObject->parent), RE::NiPointer(CurrentObject), (uint32_t) index);
                }
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
                if (do_reverse == true) {
                    sort_callback(RE::NiPointer(CurrentObject->parent), RE::NiPointer(CurrentObject), (uint32_t) index);
                }
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
                if (a_event && a_event->reference && a_event->reference->Is3DLoaded()) {
                    a_event->reference->IncRefCount();
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
                    a_event->reference->DecRefCount();
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
#ifdef SAMRIM_NAME_PATCH
    bool ddng_loaded = false;
    const char* GetFullNameHooked(RE::TESForm* form) {
        std::lock_guard<std::recursive_mutex> lock(g_name_mutex);
        if (form != nullptr) {
            if (auto armor = form->As<RE::TESObjectARMO>()) {
                if (!ddng_loaded) {
                    if (DeviousDevicesAPI::LoadAPI()) {
                        ddng_loaded = true;
                    }
                }
                if (ddng_loaded) {
                    if (auto inventory_armor = DeviousDevicesAPI::g_API->GetDeviceInventory(armor)) {
                        if (inventory_armor->GetName() != nullptr && inventory_armor->GetName()[0] != 0x0) {
                            return inventory_armor->GetName();
                        }
                    }
                }
            }
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
#endif
    static SKEENullFix* nullSkeletonFix;
    static void (*SteamdeckVirtualKeyboardCallback)(uint64_t param_1, char* param_2) = (void (*)(uint64_t param_1, char* param_2)) 0x0;
    static void (*SteamdeckVirtualKeyboardCallback2)(uint64_t param_1, char* param_2) = (void (*)(uint64_t param_1, char* param_2)) 0x0;
    static void (*OverlayHook)(void* inter, uint32_t param_2, uint32_t param_3, RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                               RE::NiAVObject* param_6) = (void (*)(void* inter, uint32_t param_2, uint32_t param_3,
                                                                    RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                                                                    RE::NiAVObject* param_6)) 0x0;
    static void (*OverlayHook2)(void* inter, uint32_t param_2, uint32_t param_3, RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                                RE::NiAVObject* param_6) = (void (*)(void* inter, uint32_t param_2, uint32_t param_3,
                                                                     RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                                                                     RE::NiAVObject* param_6)) 0x0;
    static void (*InstallOverlayHook)(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4,
                                      RE::BSGeometry* geo, RE::NiNode* param_5,
                                      RE::BGSTextureSet* param_6) = (void (*)(void* inter, const char* param_2, const char* param_3,
                                                                              RE::TESObjectREFR* param_4, RE::BSGeometry* geo,
                                                                              RE::NiNode* param_5, RE::BGSTextureSet* param_6)) 0x0;
    static void (*ApplyMorphsHook)(void*, void*, void*, bool, bool) = (void (*)(void*, void*, void*, bool, bool)) 0x0;
    static void (*UpdateMorphsHook)(void*, void*, void*) = (void (*)(void*, void*, void*)) 0x0;
    static void (*DeepCopyDetour)(uint64_t param_1, uint64_t* param_2, uint64_t param_3,
                                  uint64_t param_4) = (void (*)(uint64_t param_1, uint64_t* param_2, uint64_t param_3,
                                                                uint64_t param_4)) 0x0;

    static void DeepCopy_fn(uint64_t param_1, uint64_t* param_2, uint64_t param_3, uint64_t param_4) {
        if (param_1 == 0x0) {
            logger::info("Invalid DeepCopy, skipping copy");
            return;
        }
        //logger::info("DeepCopy vtable {:08X}, checking refcount", *(uintptr_t*) param_1);
        if (RE::NiObject* object = (RE::NiObject*) param_1) {
            if (object->_refCount == 0) {
                logger::info("Invalid DeepCopy ref count, skipping copy", *(uintptr_t*) param_1);
                return;
            }
        }
        DeepCopyDetour(param_1, param_2, param_3, param_4);
    }
    static void FakeCallbackDone(void*, const char*) {}
    static void FakeCallbackCancel(void*, const char*) {}
    static void VirtualKeyboard_fn(uint64_t param_1, char* param_2) {
        if (param_1 != 0x0) {
            if ((*(uint64_t*) (param_1 + 0x1d0)) == 0x0) {
                (*(uint64_t*) (param_1 + 0x1d0)) = (uint64_t) FakeCallbackDone;
            }
            if ((*(uint64_t*) (param_1 + 0x1d8)) == 0x0) {
                (*(uint64_t*) (param_1 + 0x1d8)) = (uint64_t) FakeCallbackCancel;
            }
        }
        SteamdeckVirtualKeyboardCallback(param_1, param_2);
    }
    static void VirtualKeyboard2_fn(uint64_t param_1, char* param_2) {
        if (param_1 != 0x0) {
            if ((*(uint64_t*) (param_1 + 0x1d0)) == 0x0) {
                (*(uint64_t*) (param_1 + 0x1d0)) = (uint64_t) FakeCallbackDone;
            }
            if ((*(uint64_t*) (param_1 + 0x1d8)) == 0x0) {
                (*(uint64_t*) (param_1 + 0x1d8)) = (uint64_t) FakeCallbackCancel;
            }
        }
        SteamdeckVirtualKeyboardCallback2(param_1, param_2);
    }
    static void OverlayHook_fn(void* inter, uint32_t param_2, uint32_t param_3, RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                               RE::NiAVObject* param_6) {
        /* if (param_4 && param_4->Is(RE::FormType::ActorCharacter)) {
            RE::Actor* actor = param_4->As<RE::Actor>();
            if (actor) {
                if (actor->extraList.GetByType(RE::ExtraDataType::kDismemberedLimbs)) {
                    return;
                }
            }
        }*/

        OverlayHook(inter, param_2, param_3, param_4, param_5, param_6);
    }
    static void OverlayHook2_fn(void* inter, uint32_t param_2, uint32_t param_3, RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                                RE::NiAVObject* param_6) {
        /*
        if (param_4 && param_4->Is(RE::FormType::ActorCharacter)) {
            RE::Actor* actor = param_4->As<RE::Actor>();
            if (actor) {
                if (actor->extraList.GetByType(RE::ExtraDataType::kDismemberedLimbs)) {
                    return;
                }
            }
        }*/
        OverlayHook2(inter, param_2, param_3, param_4, param_5, param_6);
    }
    static bool PARALLEL_MORPH_FIX = false;
#ifdef PARALLEL_MORPH_WORKAROUND
    std::recursive_mutex update_morphs_mutex;
    std::recursive_mutex apply_morphs_mutex;
    static void ApplyMorphsHook_fn(void* arg1, void* arg2, void* arg3, bool attaching, bool defer) {
        if (PARALLEL_MORPH_FIX) {
            logger::info("Apply Morph Defer: {}", defer);
            defer = false;
            logger::info("Apply Morph New Defer: {}", defer);
            if (auto task_int = SKSE::GetTaskInterface()) {
                if (arg2 && ((RE::TESObjectREFR*) arg2)->As<RE::TESObjectREFR>()) {
                    ((RE::TESObjectREFR*) arg2)->As<RE::TESObjectREFR>()->IncRefCount();
                }
                task_int->AddTask([arg1=arg1,arg2=arg2,arg3=arg3,attaching=attaching,defer=defer] {
                    ApplyMorphsHook(arg1, arg2, arg3, attaching, defer);
                    if (arg2 && ((RE::TESObjectREFR*) arg2)->As<RE::TESObjectREFR>()) {
                        ((RE::TESObjectREFR*) arg2)->As<RE::TESObjectREFR>()->DecRefCount();
                    }    
                });
            }
        } else {
            ApplyMorphsHook(arg1, arg2, arg3, attaching, defer);
        }
    }
    static void UpdateMorphsHook_fn(void* arg1, void* arg2, void* arg3) {
        if (PARALLEL_MORPH_FIX) {
            logger::info("Update Morph Defer: {}", ((uint64_t) arg3) & 0x1);
            arg3 = (void*) 0x0;
            logger::info("Update Morph New Defer: {}", ((uint64_t) arg3) & 0x1);
            if (auto task_int = SKSE::GetTaskInterface()) {
                if (arg2 && ((RE::TESObjectREFR*) arg2)->As<RE::TESObjectREFR>()) {
                    ((RE::TESObjectREFR*) arg2)->As<RE::TESObjectREFR>()->IncRefCount();
                }
                task_int->AddTask([arg1 = arg1, arg2 = arg2, arg3 = arg3] {
                    
                    UpdateMorphsHook(arg1, arg2, arg3);
                    if (arg2 && ((RE::TESObjectREFR*) arg2)->As<RE::TESObjectREFR>()) {
                        ((RE::TESObjectREFR*) arg2)->As<RE::TESObjectREFR>()->DecRefCount();    
                    }
                });
            }
        } else {
            UpdateMorphsHook(arg1, arg2, arg3);
        }
    }
#endif
#ifdef MORPHCACHE_SHRINK_WORKAROUND
    static void (*CacheShrinkHook)(void*) = (void (*)(void*)) 0x0;
    static void (*CacheClearHook)(void*) = (void (*)(void*)) 0x0;
    static uintptr_t Morph_vtable = 0x0;
    static void CacheShrinkHook_fn(void* morphCache) {
        uintptr_t cache_ptr = (uintptr_t) morphCache;
        uintptr_t limit_ptr = cache_ptr + 0x48;
        uintptr_t current_ptr = cache_ptr + 0x50;
        uintptr_t interface_ptr = cache_ptr - 0x58;
        //logger::info("Shrink Morph Cache was called");
        if (morphCache != nullptr) {
            if (*(uintptr_t*) interface_ptr != Morph_vtable) {
                logger::info("Incorrect interface for morphs, not clearing");
            } else {
                //CacheClearHook((void*) interface_ptr);
                if (*(uint64_t*) current_ptr >= *(uint64_t*) limit_ptr) {
                    logger::info("Clearing Morph Cache to prevent crash");
                    CacheClearHook((void*) interface_ptr);
                }
            }
        }
    }
#endif
    static void InstallOverlayHook_fn(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4,
                                      RE::BSGeometry* geo, RE::NiNode* param_5, RE::BGSTextureSet* param_6) {
        RE::BSFixedString geometry_node_name(param_2);
        RE::BSGeometry* found_geo = nullptr;
        if (RE::NiAVObject* found_geometry = param_5->GetObjectByName(geometry_node_name)) {
            found_geo = found_geometry->AsGeometry();
        }
        if (found_geo) {
            if (!geo || geo->_refCount == 0 || (geo->GetType() != found_geo->GetType())) {
                logger::info("Found incorrect geometry type for overlay, fixing");
                while (found_geo) {
                    found_geo->GetGeometryRuntimeData().skinInstance = nullptr;
                    if (found_geo->parent) {
                        found_geo->parent->DetachChild(found_geo);
                    }
                    if (RE::NiAVObject* found_geometry = param_5->GetObjectByName(geometry_node_name)) {
                        found_geo = found_geometry->AsGeometry();
                    } else {
                        found_geo = nullptr;
                    }
                }
                logger::info("Found incorrect geometry type for overlays, removal complete");
            }
        }
        InstallOverlayHook(inter, param_2, param_3, param_4, geo, param_5, param_6);
        if (param_5) {
            if (RE::NiAVObject* found_geometry = param_5->GetObjectByName(geometry_node_name)) {
                found_geo = found_geometry->AsGeometry();
                if (found_geo != nullptr && print_flags == true) {
                    if (found_geo->GetGeometryRuntimeData().properties[1]) {
                        auto shader_prop = (RE::BSLightingShaderProperty*) found_geo->GetGeometryRuntimeData().properties[1].get();
                        if (shader_prop != nullptr) {
                            logger::info("before culling fix: overlay {} flags {} NiAVObjectFlags {}", param_2,
                                         shader_prop->flags.underlying(), found_geo->GetFlags().underlying());
                            if (overlay_culling_fix == true) {
                                found_geo->GetFlags().set(RE::NiAVObject::Flag::kAlwaysDraw);

                                logger::info("after culling fix: overlay {} flags {} NiAVObjectFlags {}", param_2,
                                             shader_prop->flags.underlying(), found_geo->GetFlags().underlying());
                            }
                        }
                    }
                }
            }
        }
    }
    static std::atomic<uint32_t> skee_loaded = 0;
    static std::atomic<uint32_t> samrim_loaded = 0;
    static std::atomic<uint32_t> skse_loaded = 0;
    static bool save_danger = false;
    static bool skip_load = false;
    static bool vr_esl = true;
    void GameEventHandler::onPostPostLoad() {
        mINI::INIFile file("Data\\skse\\plugins\\OverlayFix.ini");
        mINI::INIStructure ini;
        if (file.read(ini) == false) {
            ini["OverlayFix"]["reverse"] = "default";
            ini["OverlayFix"]["skipload"] = "false";
            ini["OverlayFix"]["nocull"] = "default";
            ini["OverlayFix"]["savedanger"] = "default";
            ini["OverlayFix"]["vresl"] = "default";
            ini["OverlayFix"]["parallelmorphfix"] = "default";
        }
        file.generate(ini);
        if (ini["OverlayFix"]["parallelmorphfix"] == "true") {
            PARALLEL_MORPH_FIX = true;
        }
        if (ini["OverlayFix"]["reverse"] == "true") {
            do_reverse = true;
        } else if (ini["OverlayFix"]["reverse"] == "false") {
            do_reverse = false;
        }
        if (ini["OverlayFix"]["skipload"] == "true") {
            skip_load = true;
        }
        if (ini["OverlayFix"]["nocull"] == "true") {
            overlay_culling_fix = true;
        } else if (ini["OverlayFix"]["nocull"] == "false") {
            overlay_culling_fix = false;
        }
        if (ini["OverlayFix"]["savedanger"] == "true") {
            save_danger = true;
        }
        if (ini["OverlayFix"]["vresl"] == "true") {
            vr_esl = true;
        } else if (ini["OverlayFix"]["vresl"] == "false") {
            vr_esl = false;
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

                    auto deepcopy_addr = (uintptr_t) REL::Offset(0xd18080).address();
                    DeepCopyDetour =
                        (void (*)(uint64_t param_1, uint64_t* param_2, uint64_t param_3, uint64_t param_4)) REL::Offset(0xd18080).address();
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) DeepCopyDetour, &DeepCopy_fn);
                    DetourTransactionCommit();
#endif
#ifdef DISMEMBER_CRASH_FIX_ALPHA
                    if (OverlayHook == 0x0) {
                        OverlayHook =
                            (void (*)(void* inter, uint32_t param_2, uint32_t param_3, RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                                      RE::NiAVObject* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0xd22a0);
                        OverlayHook2 =
                            (void (*)(void* inter, uint32_t param_2, uint32_t param_3, RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                                      RE::NiAVObject* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0xd23f0);
                        InstallOverlayHook = (void (*)(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4,
                                                       RE::BSGeometry* geo, RE::NiNode* param_5,
                                                       RE::BGSTextureSet* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0xd04d0);
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) InstallOverlayHook, &InstallOverlayHook_fn);
                        DetourTransactionCommit();
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) OverlayHook, &OverlayHook_fn);
                        DetourTransactionCommit();
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) OverlayHook2, &OverlayHook2_fn);
                        DetourTransactionCommit();
                    }

#endif
#ifdef PARALLEL_MORPH_WORKAROUND
                    logger::info("SKEE64 1170 parallel morph workaround applying");
                    UpdateMorphsHook = (void (*)(void*, void*, void*))((uint64_t) skee64_info.lpBaseOfDll + 0x167b0);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) UpdateMorphsHook, &UpdateMorphsHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEE64 1170 parallel morph workaround applied");
#endif
#ifdef MORPHCACHE_SHRINK_WORKAROUND
                    CacheShrinkHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0x1d280);
                    CacheClearHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0x18c30);
                    Morph_vtable = ((uintptr_t) skee64_info.lpBaseOfDll + 0x1df598);
                    logger::info("SKEE64 1170 morphcache shrink workaround applying");
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) CacheShrinkHook, &CacheShrinkHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEE64 1170 morphcache shrink workaround applied");
#endif

                    if (skip_load == true) {
                        uintptr_t skip_load_addr = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xa7a70);
                        REL::safe_write(skip_load_addr, (uint8_t*) "\x48\xe9", 2);
                        logger::info("SKEE64 1170 skipping SKEE co-save loading to fix corrupted save.");
                    }
                    logger::info("SKEE64 patched");
                } else if ((skee64_info.SizeOfImage >= 0x1d8568 + 7) &&
                           memcmp("BODYTRI", (void*) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1d8568), 7) == 0) {
                    logger::info("Found SKEE64 tags build");
                    uintptr_t patch0 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xc70c8);
                    uintptr_t patch1 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xc70dd);
                    uintptr_t patch2 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xc70e8);
                    REL::safe_write(patch0, (uint8_t*) "\x8b\xc1\x90\x90", 4);
                    REL::safe_write(patch1, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch2, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90", 7);
                    nullSkeletonFix = new SKEENullFix((uint64_t) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xd95f0));
                    const uint8_t* nullSkeletonCode = nullSkeletonFix->getCode();
                    REL::safe_write(((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1e74a8), (uint8_t*) (&nullSkeletonCode),
                                    sizeof(uint64_t));
                    if (ini["OverlayFix"]["reverse"] == "default") {
                        do_reverse = true;
                    }
#ifdef CRASH_FIX_ALPHA

                    auto deepcopy_addr = (uintptr_t) REL::Offset(0xd18080).address();
                    DeepCopyDetour =
                        (void (*)(uint64_t param_1, uint64_t* param_2, uint64_t param_3, uint64_t param_4)) REL::Offset(0xd18080).address();
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) DeepCopyDetour, &DeepCopy_fn);
                    DetourTransactionCommit();
#endif
#ifdef DISMEMBER_CRASH_FIX_ALPHA
                    if (OverlayHook == 0x0) {
                        OverlayHook =
                            (void (*)(void* inter, uint32_t param_2, uint32_t param_3, RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                                      RE::NiAVObject* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0xd5b70);
                        OverlayHook2 =
                            (void (*)(void* inter, uint32_t param_2, uint32_t param_3, RE::TESObjectREFR* param_4, RE::NiNode* param_5,
                                      RE::NiAVObject* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0xd5cf6);
                        InstallOverlayHook = (void (*)(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4,
                                                       RE::BSGeometry* geo, RE::NiNode* param_5,
                                                       RE::BGSTextureSet* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0xd3da0);
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) InstallOverlayHook, &InstallOverlayHook_fn);
                        DetourTransactionCommit();
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) OverlayHook, &OverlayHook_fn);
                        DetourTransactionCommit();
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) OverlayHook2, &OverlayHook2_fn);
                        DetourTransactionCommit();
                    }

#endif
#ifdef PARALLEL_MORPH_WORKAROUND
                    logger::info("SKEE64 Tags 1170  parallel morph workaround applying");
                    UpdateMorphsHook = (void (*)(void*, void*, void*))((uint64_t) skee64_info.lpBaseOfDll + 0x168f0);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) UpdateMorphsHook, &UpdateMorphsHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEE64 Tags 1170 parallel morph workaround applied");
#endif
#ifdef MORPHCACHE_SHRINK_WORKAROUND
                    CacheShrinkHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0x1d1e0);
                    CacheClearHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0x18d70);
                    Morph_vtable = ((uintptr_t) skee64_info.lpBaseOfDll + 0x1e57d8);
                    logger::info("SKEE64 Tags 1170 morphcache shrink workaround applying");
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) CacheShrinkHook, &CacheShrinkHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEE64 Tags 1170 morphcache shrink workaround applied");
#endif

                    if (skip_load == true) {
                        uintptr_t skip_load_addr = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0xab340);
                        REL::safe_write(skip_load_addr, (uint8_t*) "\x48\xe9", 2);
                        logger::info("SKEE64 Tags 1170 skipping SKEE co-save loading to fix corrupted save.");
                    }
                    logger::info("SKEE64 Tags build patched");
                } else if ((skee64_info.SizeOfImage >= 0x1787b8 + 7) &&
                           memcmp("BODYTRI", (void*) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1787b8), 7) == 0) {
                    uintptr_t patch0 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1be78);
                    uintptr_t patch1 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1be8d);
                    uintptr_t patch2 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1be98);
                    uintptr_t patch3 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x84e8);
                    uintptr_t patch4 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x84fa);
                    REL::safe_write(patch0, (uint8_t*) "\x8b\xca\x90\x90", 4);
                    REL::safe_write(patch1, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch2, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch3, (uint8_t*) "\x8b\xd1\x90\x90", 4);
                    REL::safe_write(patch4, (uint8_t*) "\x90\x90", 2);
                    auto version = REL::Module::get().version();
                    if (version == REL::Version(1, 5, 97, 0)) {
#ifdef PARALLEL_MORPH_WORKAROUND
                        logger::info("SKEE64 UBE2 parallel morph workaround applying");
                        UpdateMorphsHook = (void (*)(void*, void*, void*))((uint64_t) skee64_info.lpBaseOfDll + 0x94b0);
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) UpdateMorphsHook, &UpdateMorphsHook_fn);
                        DetourTransactionCommit();
                        logger::info("SKEE64 UBE2 parallel morph workaround applied");
#endif
#ifdef MORPHCACHE_SHRINK_WORKAROUND
                        CacheShrinkHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0x9a70);
                        CacheClearHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0x6200);
                        Morph_vtable = ((uintptr_t) skee64_info.lpBaseOfDll + 0x180978);
                        logger::info("SKEE64 UBE2 morphcache shrink workaround applying");
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) CacheShrinkHook, &CacheShrinkHook_fn);
                        DetourTransactionCommit();
                        logger::info("SKEE64 UBE2 morphcache shrink workaround applied");
#endif
#ifdef CRASH_FIX_ALPHA

                        DeepCopyDetour =
                            (void (*)(uint64_t param_1, uint64_t* param_2, uint64_t param_3, uint64_t param_4)) REL::Offset(0xc529a0)
                                .address();
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) DeepCopyDetour, &DeepCopy_fn);
                        DetourTransactionCommit();
                        logger::info("SKEE64 UBE2 crash fix 1 applied");
#endif
#ifdef DISMEMBER_CRASH_FIX_ALPHA
                        InstallOverlayHook = (void (*)(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4,
                                                       RE::BSGeometry* geo, RE::NiNode* param_5,
                                                       RE::BGSTextureSet* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0x83160);
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) InstallOverlayHook, &InstallOverlayHook_fn);
                        DetourTransactionCommit();
                        logger::info("SKEE64 UBE2 crash fix 2 applied");
#endif
                        if (skip_load == true) {
                            uintptr_t skip_load_addr = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x514ea);
                            REL::safe_write(skip_load_addr, (uint8_t*) "\x48\xe9", 2);
                            logger::info("SKEE64 UBE2 skipping SKEE co-save loading to fix corrupted save.");
                        }
                    }
                    logger::info("SKEE64 UBE2 patched");
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
                    auto version = REL::Module::get().version();
                    if (version == REL::Version(1, 5, 97, 0)) {
#ifdef PARALLEL_MORPH_WORKAROUND
                        logger::info("SKEE64 1597 parallel morph workaround applying");
                        UpdateMorphsHook = (void (*)(void*, void*, void*))((uint64_t) skee64_info.lpBaseOfDll + 0x86d0);
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) UpdateMorphsHook, &UpdateMorphsHook_fn);
                        DetourTransactionCommit();
                        logger::info("SKEE64 1597 parallel morph workaround applied");
#endif
#ifdef MORPHCACHE_SHRINK_WORKAROUND
                        CacheShrinkHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0x8c10);
                        CacheClearHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0x5340);
                        Morph_vtable = ((uintptr_t) skee64_info.lpBaseOfDll + 0x173ec8);
                        logger::info("SKEE64 1597 morphcache shrink workaround applying");
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) CacheShrinkHook, &CacheShrinkHook_fn);
                        DetourTransactionCommit();
                        logger::info("SKEE64 1597 morphcache shrink workaround applied");
#endif
#ifdef CRASH_FIX_ALPHA

                        DeepCopyDetour =
                            (void (*)(uint64_t param_1, uint64_t* param_2, uint64_t param_3, uint64_t param_4)) REL::Offset(0xc529a0)
                                .address();
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) DeepCopyDetour, &DeepCopy_fn);
                        DetourTransactionCommit();
                        logger::info("SKEE64 1597 crash fix 1 applied");
#endif
#ifdef DISMEMBER_CRASH_FIX_ALPHA
                        InstallOverlayHook = (void (*)(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4,
                                                       RE::BSGeometry* geo, RE::NiNode* param_5,
                                                       RE::BGSTextureSet* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0x7f5e0);
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) InstallOverlayHook, &InstallOverlayHook_fn);
                        DetourTransactionCommit();
                        logger::info("SKEE64 1597 crash fix 2 applied");
#endif
                        if (skip_load == true) {
                            uintptr_t skip_load_addr = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x4e32a);
                            REL::safe_write(skip_load_addr, (uint8_t*) "\x48\xe9", 2);
                            logger::info("SKEE64 1597 skipping SKEE co-save loading to fix corrupted save.");
                        }
                    }
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
#ifdef CRASH_FIX_ALPHA

                    DeepCopyDetour =
                        (void (*)(uint64_t param_1, uint64_t* param_2, uint64_t param_3, uint64_t param_4)) REL::Offset(0xc8ca90).address();
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) DeepCopyDetour, &DeepCopy_fn);
                    DetourTransactionCommit();
                    logger::info("SKEE64 041914 crash fix 1 applied");
#endif
#ifdef MORPHCACHE_SHRINK_WORKAROUND
                    CacheShrinkHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0xc360);
                    CacheClearHook = (void (*)(void*))((uint64_t) skee64_info.lpBaseOfDll + 0x89f0);
                    Morph_vtable = ((uintptr_t) skee64_info.lpBaseOfDll + 0x184b80);
                    logger::info("SKEE64 041914 morphcache shrink workaround applying");
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) CacheShrinkHook, &CacheShrinkHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEE64 041914 morphcache shrink workaround applied");
#endif
#ifdef DISMEMBER_CRASH_FIX_ALPHA
                    InstallOverlayHook =
                        (void (*)(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4, RE::BSGeometry* geo,
                                  RE::NiNode* param_5, RE::BGSTextureSet* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0x8be90);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) InstallOverlayHook, &InstallOverlayHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEE64 041914 crash fix 2 applied");
#endif
                    if (skip_load == true) {
                        uintptr_t skip_load_addr = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x57d0c);
                        REL::safe_write(skip_load_addr, (uint8_t*) "\x48\xe9", 2);
                        logger::info("SKEE64 041914 skipping SKEE co-save loading to fix corrupted save.");
                    }
                    logger::info("SKEE64 041914 patched");
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
#ifdef DISMEMBER_CRASH_FIX_ALPHA
                    logger::info("SKEEVR InstallOverlay patching");
                    InstallOverlayHook =
                        (void (*)(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4, RE::BSGeometry* geo,
                                  RE::NiNode* param_5, RE::BGSTextureSet* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0x7c4b0);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) InstallOverlayHook, &InstallOverlayHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEEVR InstallOverlay patched");
#endif
#ifdef VR_ESL_SUPPORT
                    if (vr_esl == true) {
                        void** lookupform_addr = (void**) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1c7c80);
                        lookupform_addr[0] = LookupFormSKEEVR;
                        logger::info("SKEEVR extra ESL patches applied");
                    }

#endif
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
#ifdef DISMEMBER_CRASH_FIX_ALPHA
                    logger::info("SKEEVR InstallOverlay patching");
                    InstallOverlayHook =
                        (void (*)(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4, RE::BSGeometry* geo,
                                  RE::NiNode* param_5, RE::BGSTextureSet* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0x7c4b0);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) InstallOverlayHook, &InstallOverlayHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEEVR InstallOverlay patched");
#endif
#ifdef PARALLEL_MORPH_WORKAROUND
                    logger::info("SKEEVR parallel morph workaround applying");
                    if (ini["OverlayFix"]["parallelmorphfix"] == "default" || ini["OverlayFix"]["parallelmorphfix"] == "") {
                        PARALLEL_MORPH_FIX = true;
                    }
                    UpdateMorphsHook = (void (*)(void*, void*, void*))((uint64_t) skee64_info.lpBaseOfDll + 0x7540);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) UpdateMorphsHook, &UpdateMorphsHook_fn);
                    DetourTransactionCommit();
                    ApplyMorphsHook = (void (*)(void*, void*, void*, bool, bool))((uint64_t) skee64_info.lpBaseOfDll + 0x7480);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) ApplyMorphsHook, &ApplyMorphsHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEEVR parallel morph workaround applied");
#endif
#ifdef VR_ESL_SUPPORT
                    if (vr_esl == true) {
                        void** lookupform_addr = (void**) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x1c7c80);
                        lookupform_addr[0] = LookupFormSKEEVR;
                        logger::info("SKEEVR extra ESL patches applied");
                    }

#endif
                    logger::info("SKEE64 VR patched");

                } else if ((skee64_info.SizeOfImage >= 0x172cc8 + 7) &&
                           memcmp("BODYTRI", (void*) ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x172cc8), 7) == 0) {
                    uintptr_t patch0 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x7f88);
                    uintptr_t patch1 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x7f9d);
                    uintptr_t patch2 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x7fa8);
                    uintptr_t patch3 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x71a8);
                    uintptr_t patch4 = ((uintptr_t) skee64_info.lpBaseOfDll + (uintptr_t) 0x71ba);
                    REL::safe_write(patch0, (uint8_t*) "\x8b\xca\x90\x90", 4);
                    REL::safe_write(patch1, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch2, (uint8_t*) "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
                    REL::safe_write(patch3, (uint8_t*) "\x8b\xd1\x90\x90", 4);
                    REL::safe_write(patch4, (uint8_t*) "\x90\x90", 2);
#ifdef DISMEMBER_CRASH_FIX_ALPHA
                    logger::info("SKEEVR 0p5 InstallOverlay patching");
                    InstallOverlayHook =
                        (void (*)(void* inter, const char* param_2, const char* param_3, RE::TESObjectREFR* param_4, RE::BSGeometry* geo,
                                  RE::NiNode* param_5, RE::BGSTextureSet* param_6))((uint64_t) skee64_info.lpBaseOfDll + 0x83b00);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) InstallOverlayHook, &InstallOverlayHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEEVR 0p5 InstallOverlay patched");
#endif
#ifdef PARALLEL_MORPH_WORKAROUND
                    logger::info("SKEEVR 0p5 parallel morph workaround applying");
                    if (ini["OverlayFix"]["parallelmorphfix"] == "default" || ini["OverlayFix"]["parallelmorphfix"] == "") {
                        PARALLEL_MORPH_FIX = true;
                    }
                    UpdateMorphsHook = (void (*)(void*, void*, void*))((uint64_t) skee64_info.lpBaseOfDll + 0x80a0);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) UpdateMorphsHook, &UpdateMorphsHook_fn);
                    DetourTransactionCommit();
                    ApplyMorphsHook = (void (*)(void*, void*, void*, bool, bool))((uint64_t) skee64_info.lpBaseOfDll + 0x7e60);
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&(PVOID&) ApplyMorphsHook, &ApplyMorphsHook_fn);
                    DetourTransactionCommit();
                    logger::info("SKEEVR 0p5 parallel morph workaround applied");
#endif
                    logger::info("SKEEVR 0p5 patched");

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
                std::vector<unsigned char> get_full_name_prefix = {0x48, 0x83, 0xec, 0x28, 0x0f, 0xb6, 0x41, 0x1a, 0x4c, 0x8b,
                                                                   0xc1, 0x83, 0xc0, 0xf6, 0x83, 0xf8, 0x7b, 0x77, 0x72};
                std::vector<unsigned char> samrim_data((unsigned char*) samrim_info.lpBaseOfDll,
                                                       (unsigned char*) samrim_info.lpBaseOfDll + samrim_info.SizeOfImage);
                auto itfound =
                    std::search(samrim_data.begin(), samrim_data.end(), get_full_name_prefix.begin(), get_full_name_prefix.end());
                if (itfound != samrim_data.end()) {
                    if ((samrim_info.SizeOfImage >= 0x1000)) {
                        uintptr_t patch0 = ((uintptr_t) samrim_info.lpBaseOfDll + (uintptr_t) (itfound - samrim_data.begin()));
                        Code c0((uint64_t) &GetFullNameHooked);
                        const uint8_t* hook0 = c0.getCode();
                        REL::safe_write(patch0, hook0, c0.getSize());
                        logger::info("SAM patched");
                    }
                }
            }
        }
#endif
#ifdef SKSE_COSAVE_STACK_WORKAROUND
        if (save_danger == true) {
            if (HMODULE handleskse = GetModuleHandleA("skse64_1_6_1170.dll")) {
                MODULEINFO skse_info;
                GetModuleInformation(GetCurrentProcess(), handleskse, &skse_info, sizeof(skse_info));
                uint32_t expected = 0;
                if (skse_loaded.compare_exchange_strong(expected, 1) == true && expected == 0) {
                    unsigned char signature[] = {0x4c, 0x8b, 0xa2, 0x00, 0x02, 0x00, 0x00, 0x49, 0x8b, 0x55, 0x30, 0x48, 0x2b, 0xc2,
                                                 0x48, 0xc1, 0xf8, 0x04, 0x48, 0x85, 0xc0, 0x0f, 0x84, 0x76, 0x01, 0x00, 0x00};
                    if ((skse_info.SizeOfImage >= 0x5e000) &&
                        memcmp((void*) signature, (void*) ((uintptr_t) skse_info.lpBaseOfDll + (uintptr_t) 0x5d550), sizeof(signature)) ==
                            0) {
                        //logger::info("Patching Co-Save Logging for debugging CTD");
                        //CoSaveStoreLogAddr=(void (*)(void*, void*, unsigned int))((uintptr_t) skse_info.lpBaseOfDll + (uintptr_t) 0x5d890);
                        CoSaveDropStacksAddr = (void (*)(void*))((uintptr_t) skse_info.lpBaseOfDll + (uintptr_t) 0x5d530);
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) CoSaveDropStacksAddr, &CoSaveDropStacksBypass);
                        DetourTransactionCommit();
                        logger::info(
                            "Patched 1170 Co-Save Persistent Object save to prevent CTD, this may cause save corruption and should only be "
                            "used "
                            "in one case.");
                    }
                }
            }
            if (HMODULE handleskse = GetModuleHandleA("sksevr_1_4_15.dll")) {
                MODULEINFO skse_info;
                GetModuleInformation(GetCurrentProcess(), handleskse, &skse_info, sizeof(skse_info));
                uint32_t expected = 0;
                if (skse_loaded.compare_exchange_strong(expected, 1) == true && expected == 0) {
                    unsigned char signature[] = {0x56, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0x05, 0xf3, 0x2b,
                                                 0x0f, 0x00, 0x45, 0x33, 0xff, 0x48, 0x8b, 0xf1, 0x41, 0x8b, 0xef, 0x48, 0x8b, 0x10};
                    if ((skse_info.SizeOfImage >= 0x6f600) &&
                        memcmp((void*) signature, (void*) ((uintptr_t) skse_info.lpBaseOfDll + (uintptr_t) 0x6f535), sizeof(signature)) ==
                            0) {
                        //logger::info("Patching Co-Save Logging for debugging CTD");
                        //CoSaveStoreLogAddr=(void (*)(void*, void*, unsigned int))((uintptr_t) skse_info.lpBaseOfDll + (uintptr_t) 0x5d890);
                        CoSaveDropStacksAddr = (void (*)(void*))((uintptr_t) skse_info.lpBaseOfDll + (uintptr_t) 0x6f530);
                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID&) CoSaveDropStacksAddr, &CoSaveDropStacksBypass);
                        DetourTransactionCommit();
                        logger::info(
                            "Patched VR Co-Save Persistent Object save to prevent CTD, this may cause save corruption and should only be "
                            "used "
                            "in one case.");
                    }
                }
            }
        }
#endif
#ifdef STEAMDECK_CRASH_FIX
        auto version = REL::Module::get().version();
        if (version == REL::Version(1, 6, 1170, 0)) {
            logger::info("Patching Steamdeck keyboard crash");
            SteamdeckVirtualKeyboardCallback = (void (*)(uint64_t param_1, char* param_2)) REL::Offset(0x1531c60).address();
            SteamdeckVirtualKeyboardCallback2 = (void (*)(uint64_t param_1, char* param_2)) REL::Offset(0x1531cd0).address();
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&) SteamdeckVirtualKeyboardCallback, &VirtualKeyboard_fn);
            DetourTransactionCommit();
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&) SteamdeckVirtualKeyboardCallback2, &VirtualKeyboard2_fn);
            DetourTransactionCommit();
            logger::info("completed patching Steamdeck keyboard crash");
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