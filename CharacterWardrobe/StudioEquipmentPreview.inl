// Extracted from this build's EquipSkinDB; mutable options are validated by SetClothing.
struct StudioEquipmentSpec { const char* id; const wchar_t* slot; const wchar_t* base; const wchar_t* group; const wchar_t* option; };
static const StudioEquipmentSpec kStudioEquipment[] = {
    {"Item_Skin_BackPack_01", L"Backpack", L"Back_Lv2", L"Back_Lv2_Group", L"Back_Ellie_01"},
    {"Item_Skin_Helmet_Biker_001", L"Head", L"Head_E_01", nullptr, nullptr},
    {"Item_Skin_Helmet_G_02", L"Head", L"Head_G_02", nullptr, nullptr},
    {"Item_Skin_BackLV2_A_02", L"Backpack", L"BackLv2_A", L"Back_BackLv2_Group", L"BackLv2_A_02"},
    {"Item_Skin_BackLV2_A_03", L"Backpack", L"BackLv2_A", L"Back_BackLv2_Group", L"BackLv2_A_03"},
    {"Item_Skin_BackLV2_A_04", L"Backpack", L"BackLv2_A", L"Back_BackLv2_Group", L"BackLv2_A_04"},
    {"Item_Skin_BackLV3_A_01", L"Backpack", L"BackLv3_A", L"Back_BackLv3_Group", L"BackLv3_A_01"},
    {"Item_Skin_BackLV3_A_02", L"Backpack", L"BackLv3_A", L"Back_BackLv3_Group", L"BackLv3_A_02"},
    {"Item_Skin_BackLV3_B_01", L"Backpack", L"BackLv3_B", L"Back_BackLv3_B_Group", L"BackLv3_B_01"},
    {"Item_Skin_BackLv1_A_01", L"Backpack", L"BackLv1_A", L"Back_BackLv1_Group", L"BackLv1_A_01"},
    {"Item_Skin_BackLv1_B_01", L"Backpack", L"BackLv1_B", L"Back_BackLv1_B_Group", L"BackLv1_B_01"},
    {"Item_Skin_BackLv1_B_02", L"Backpack", L"BackLv1_B", L"Back_BackLv1_B_Group", L"BackLv1_B_02"},
    {"Item_Skin_BackLv2_A_01", L"Backpack", L"BackLv2_A", L"Back_BackLv2_Group", L"BackLv2_A_01"},
    {"Item_Skin_Back_A_01", L"Backpack", L"Back_A", L"Back_A_Group", L"Back_A_01"},
    {"Item_Skin_Back_A_02", L"Backpack", L"Back_A", L"Back_A_Group", L"Back_A_02"},
    {"Item_Skin_Helmet_999", L"Head", L"Head_Biker_B", L"Head_Biker_B_Group", L"Head_Biker_B_01"},
    {"Item_Skin_Helmet_Biker_002", L"Head", L"Head_Biker_B", L"Head_Biker_B_Group", L"Head_Biker_B_01"},
    {"Item_Skin_Helmet_Biker_A_02", L"Head", L"Head_Biker_A", L"Head_Biker_A_Group", L"Head_Biker_A_02"},
    {"Item_Skin_Helmet_Biker_A_03", L"Head", L"Head_Biker_A", L"Head_Biker_A_Group", L"Head_Biker_A_03"},
    {"Item_Skin_Helmet_Biker_A_04", L"Head", L"Head_Biker_A", L"Head_Biker_A_Group", L"Head_Biker_A_04"},
    {"Item_Skin_Helmet_Biker_A_06", L"Head", L"Head_Biker_A", L"Head_Biker_A_Group", L"Head_Biker_A_06"},
    {"Item_Skin_Helmet_E_03", L"Head", L"Head_E", L"Head_E_Group", L"Head_E_03"},
    {"Item_Skin_Helmet_F_03", L"Head", L"Head_F", L"Head_F_Group", L"Head_F_03"},
    {"Item_Skin_Helmet_F_04", L"Head", L"Head_F", L"Head_F_Group", L"Head_F_04"},
    {"Item_Skin_Helmet_F_05", L"Head", L"Head_F", L"Head_F_Group", L"Head_F_05"},
    {"Item_Skin_Helmet_F_06", L"Head", L"Head_F", L"Head_F_Group", L"Head_F_06"},
    {"Item_Skin_Helmet_F_07", L"Head", L"Head_F", L"Head_F_Group", L"Head_F_07"},
    {"Item_Skin_Helmet_G_03", L"Head", L"Head_G", L"Head_G_Group", L"Head_G_03"},
    {"Item_Skin_Parachute_Skin_001", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Back_B_01_Skin001"},
    {"Item_Skin_Parachute_Skin_002", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Back_B_01_Skin002"},
    {"Item_Skin_Parachute_Skin_005", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_Pilot_01"},
    {"Item_Skin_Parachute_Skin_006", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_Kakao_01"},
    {"Item_Skin_Parachute_Skin_007", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_Halloween_01"},
    {"Item_Skin_Parachute_Skin_008", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_PKL_01"},
    {"Item_Skin_Parachute_Skin_009", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_Dolphin_01"},
    {"Item_Skin_Parachute_Skin_010", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_03"},
    {"Item_Skin_Parachute_Skin_011", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_04"},
    {"Item_Skin_Parachute_Skin_012", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_05"},
    {"Item_Skin_Parachute_Skin_013", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_ChocoTaco_01"},
    {"Item_Skin_Parachute_Skin_014", L"Backpack", L"Parachute_Option_02", L"Back_B_02_Skin_Group", L"Para_Skin_JennaJulien_01"},
    {"Item_Skin_Parachute_Skin_015", L"Backpack", L"Parachute_Option", L"Back_B_Skin_Group", L"Para_Skin_06"},
};
static int ApplyEquipmentPreview(ALobbyCharacter* character,UCustomizableObjectInstance* inst,const std::string& selection,std::unordered_set<std::string>& matchedIds){
    const StudioEquipmentSpec* backpack=nullptr; const StudioEquipmentSpec* helmet=nullptr; const StudioEquipmentSpec* chute=nullptr;
    std::ifstream focusFile("studio-preview-equipment.txt");std::string focus;std::getline(focusFile,focus);
    std::istringstream in(selection); std::string id;
    while(std::getline(in,id,',')) for(const auto& spec:kStudioEquipment) if(id==spec.id){ if(id.rfind("Item_Skin_Parachute_",0)==0) chute=&spec; else if(wcscmp(spec.slot,L"Backpack")==0){if(!backpack||id==focus)backpack=&spec;} else if(!helmet||id==focus)helmet=&spec; break; }
    std::ifstream preview("studio-preview-category.txt"); std::string category; std::getline(preview,category);
    if(chute && (category=="chute" || !backpack)) backpack=chute;
    if(category=="appearance") helmet=nullptr;
    int matched=0;
    for(const auto* spec:{backpack,helmet}) if(spec){
        std::wstring base=((uint8)character->Gender!=0?L"F_":L"M_"); base+=spec->base;
        bool validBase=SetClothing(inst,spec->slot,base.c_str());
        bool validSkin=validBase && (!spec->group || SetClothing(inst,spec->group,spec->option));
        if(validSkin){matchedIds.insert(spec->id);++matched;}
        out<<"STUDIO EQUIPMENT "<<spec->id<<" base="<<validBase<<" skin="<<validSkin<<std::endl;
    }
    return matched;
}
