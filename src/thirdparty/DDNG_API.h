#pragma once
#ifdef SAMRIM_NAME_PATCH
#ifdef DDNG_FRAMEWORK
#include "DeviceReader.h"
#endif

/*
* How to get API:
    SKSE::GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) 
    {
        switch (message->type) 
        {
            case MessagingInterface::kPostLoadGame:
            case MessagingInterface::kNewGame:
            {
                bool loc_res = DeviousDevicesAPI::LoadAPI();
                if (loc_res)
                {
                    // Use API
                    // Example
                    DeviousDevicesAPI::g_API->GetDeviceRender(...);
                }
            }
            break;
        }
    });

* Do not forget to include this source file to your project!
*/

#define DD_APIVERSION 2U

namespace DeviousDevicesAPI {
    //Device mod prototype
    struct DeviceModPrototype {
            std::string name;
            uint8_t group_TES4[32];
            uint8_t group_ARMO[32];
            size_t size;
            uint8_t* rawdata = nullptr;
            std::vector<uint8_t[16]> devicerecords;
            std::vector<std::string> masters;
    };
#ifdef DDNG_FRAMEWORK
    static_assert(sizeof(DeviceModPrototype) == sizeof(DeviousDevices::DeviceMod));
#endif

    //Device unit prototype
    struct DeviceUnitPrototype {
            std::string scriptName;

            RE::BGSKeyword* kwd = nullptr;

            RE::BGSMessage* equipMenu = nullptr;
            RE::BGSMessage* zad_DD_OnPutOnDevice = nullptr;
            RE::BGSMessage* zad_EquipRequiredFailMsg = nullptr;
            RE::BGSMessage* zad_EquipConflictFailMsg = nullptr;

            std::vector<RE::BGSKeyword*> equipConflictingDeviceKwds;
            std::vector<RE::BGSKeyword*> requiredDeviceKwds;
            std::vector<RE::BGSKeyword*> unequipConflictingDeviceKwds;

            bool lockable;
            bool canManipulate;

            RE::TESObjectARMO* deviceInventory = nullptr;
            RE::TESObjectARMO* deviceRendered = nullptr;

            //following values are set to last values found on last mod (so last overriding mod)
            uint8_t padd_A[16];                             //device handle with raw data
            std::vector<RE::BGSKeyword*> keywords;          //array of keywords loaded from esp - uses keywords from last loaded mod
            std::shared_ptr<DeviceModPrototype> deviceMod;  //device source mod

            //stack of changes by mods. Last record => previous 3 values. First record => original mod record before changes from other mods
            struct HistoryRecord {
                    std::shared_ptr<DeviceModPrototype> deviceMod;
                    uint8_t padd_CB[16];
                    std::vector<RE::BGSKeyword*> keywords;
            };
            std::vector<HistoryRecord> history;  //history stack
    };
#ifdef DDNG_FRAMEWORK
    static_assert(sizeof(DeviceUnitPrototype) == sizeof(DeviousDevices::DeviceReader::DeviceUnit));
#endif
    enum BondageState : uint32_t {
        sNone = 0x0000,        // Non bondage state
        sHandsBound = 0x0001,  // actors wears any kind of heavy bondage device
        sHandsBoundNoAnim =
            0x0002,  // actor wears heavy bondage device which hides arms. Because of that it can be to some extend used with normal animations
        sGaggedBlocking = 0x0004,     // actor wears gag which block mouth
        sChastifiedGenital = 0x0008,  // actor wears chastity belt which blocks genitals
        sChastifiedAnal = 0x0010,     // actor wears chastity belt which blocks anal
        sChastifiedBreasts = 0x0020,  // actor wears chastity bra which blocks breasts
        sBlindfolded = 0x0040,        // ...
        sMittens = 0x0080,            // ...
        sBoots = 0x0100,              // ...
        sTotal = 0x0200               // Last bit for looping
    };

    class DeviousDevicesAPI {
        public:
            // API functions
            virtual size_t GetVersion() const;

            // Device Reader
            virtual const std::map<RE::TESObjectARMO*, DeviceUnitPrototype>& GetDatabase() const;
            virtual RE::TESObjectARMO* GetDeviceRender(RE::TESObjectARMO* a_invdevice) const;
            virtual RE::TESObjectARMO* GetDeviceInventory(RE::TESObjectARMO* a_renddevice) const;
            virtual RE::TESForm* GetPropertyForm(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, RE::TESForm* a_defvalue,
                                                 int a_mode) const;
            virtual int GetPropertyInt(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_defvalue, int a_mode) const;
            virtual float GetPropertyFloat(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, float a_defvalue, int a_mode) const;
            virtual bool GetPropertyBool(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, bool a_defvalue, int a_mode) const;
            virtual std::string GetPropertyString(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, std::string a_defvalue,
                                                  int a_mode) const;
            virtual std::vector<RE::TESForm*> GetPropertyFormArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname,
                                                                   int a_mode) const;
            virtual std::vector<int> GetPropertyIntArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
            virtual std::vector<float> GetPropertyFloatArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
            virtual std::vector<bool> GetPropertyBoolArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
            virtual std::vector<std::string> GetPropertyStringArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname,
                                                                    int a_mode) const;

            // Expressions
            virtual bool ApplyExpression(RE::Actor* a_actor, const std::vector<float> a_expression, float a_strength, bool a_openMouth,
                                         int a_priority) const;
            virtual bool ResetExpression(RE::Actor* a_actor, int a_priority) const;
            virtual void UpdateGagExpression(RE::Actor* a_actor) const;
            virtual void ResetGagExpression(RE::Actor* a_actor) const;
            virtual bool IsGagged(RE::Actor* a_actor) const;
            virtual bool RegisterGagType(RE::BGSKeyword* a_keyword, std::vector<RE::TESFaction*> a_factions,
                                         std::vector<int> a_defaults) const;
            virtual bool RegisterDefaultGagType(std::vector<RE::TESFaction*> a_factions, std::vector<int> a_defaults) const;

            // Hider
            virtual void SetActorStripped(RE::Actor* a_actor, bool a_stripped, int a_armorfilter, int a_devicefilter) const;
            virtual bool IsActorStripped(RE::Actor* a_actor) const;
            virtual bool IsValidForHide(RE::TESObjectARMO* a_armor) const;

            // Lib Functions
            virtual std::vector<RE::TESObjectARMO*> GetDevices(RE::Actor* a_actor, int a_mode, bool a_worn) const;
            virtual RE::TESObjectARMO* GetWornDevice(RE::Actor* a_actor, RE::BGSKeyword* a_kw, bool a_fuzzy) const;
            virtual std::vector<RE::TESObjectARMO*> GetWornDevices(RE::Actor* a_actor) const;
            virtual RE::TESObjectARMO* GetHandRestrain(RE::Actor* a_actor) const;
            virtual BondageState GetBondageState(RE::Actor* a_actor) const;
            virtual bool IsDevice(RE::TESObjectARMO* a_obj) const;
            virtual bool ActorHasBlockingGag(RE::Actor* a_actor, RE::TESObjectARMO* a_gag = nullptr) const;
    };

    // API
    inline extern DeviousDevicesAPI* g_API = nullptr;

    // call only after kDataLoaded event!
    inline bool LoadAPI() {
        if (g_API != nullptr)
            return true;
        HINSTANCE dllHandle = LoadLibrary(TEXT("DeviousDevices.dll"));
        if (dllHandle != NULL) {
            FARPROC pGetAPI = GetProcAddress(HMODULE(dllHandle), "GetAPI");
            if (pGetAPI != NULL) {
                auto loc_api = ((DeviousDevicesAPI * (*) (void) )(pGetAPI))();

                if (loc_api != nullptr && (loc_api->GetVersion() == DD_APIVERSION)) {
                    g_API = loc_api;
                    return true;
                } else {
                    // API version is old. Update your API!!
                    return false;
                }
            } else {
                return false;
            }
        }
        return false;
    }
}  // namespace DeviousDevicesAPI
#endif