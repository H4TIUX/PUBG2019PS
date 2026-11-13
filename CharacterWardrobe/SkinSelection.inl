// Included inside the bot manager namespace; every engine operation stays on its game thread.
#include "SkinCatalog.hpp"
namespace SkinSelection {
    struct LoadState { int Index=-1; UClass* Class=nullptr; ULONGLONG Requested=0; int Attempts=0; };
    std::unordered_map<std::string,LoadState> Loads;
    std::unordered_map<std::string,std::vector<std::string>> Selections;
    std::unordered_set<std::string> Applied, Refused;
    std::unordered_map<std::string,std::pair<USkinItem*,int>> SkinItems;
    ULONGLONG NextTick=0,NextFile=0;
    UWorld* World=nullptr;
    const FKnownSkin* Find(const std::string& Id) {
        static const auto Entries=[](){std::unordered_map<std::string,const FKnownSkin*> Index;for(const auto& Entry:KnownSkins)Index.emplace(Entry.Id,&Entry);return Index;}();
        auto It=Entries.find(Id);return It==Entries.end()?nullptr:It->second;
    }
    std::filesystem::path Folder() {wchar_t Path[MAX_PATH]{};GetModuleFileNameW(nullptr,Path,MAX_PATH);return std::filesystem::path(Path).parent_path();}
    UClass* Load(UWorld* W,const FKnownSkin& Entry) {
        auto& State=Loads[Entry.Id];auto* Objects=UObject::GObjects.GetTypedPtr();
        if(State.Class && State.Index>=0 && Objects && State.Index<Objects->Num() && Objects->GetByIndex(State.Index)==State.Class && State.Class->GetName()==std::string(Entry.Id)+"_C")return State.Class;
        State.Class=UObject::FindClassFast((std::string(Entry.Id)+"_C").c_str());
        if(State.Class){State.Index=State.Class->GetIndex();return State.Class;}
        const auto Now=GetTickCount64();if(State.Attempts>=3 || (State.Requested && Now-State.Requested<10000))return nullptr;
        State.Requested=Now;++State.Attempts;
        TSoftClassPtr<UClass> Reference{};Reference.ObjectID.AssetLongPathname=FString(Entry.Path);
        FLatentActionInfo Latent{};Latent.Linkage=-1;Latent.UUID=170000+(int)(&Entry-KnownSkins);Latent.CallbackTarget=W;
        UKismetSystemLibrary::LoadAssetClass(W,Reference,{},Latent);
        BLOG("SKIN LOAD requested="+std::string(Entry.Id));
        return nullptr;
    }
    void ReadSelections() {
        Selections.clear();std::ifstream File(Folder()/L"LAN_Appearance.ini");std::string Line;bool Section=false;
        while(std::getline(File,Line)){
            if(Line.size()>4096)continue;if(!Line.empty()&&Line.back()=='\r')Line.pop_back();
            if(!Line.empty()&&Line.front()=='['){Section=Line=="[Appearance]";continue;}if(!Section)continue;
            auto Eq=Line.find('=');if(Eq==std::string::npos||Eq<1||Eq>24)continue;std::string Name=Line.substr(0,Eq);bool Valid=true;
            for(unsigned char C:Name)if(!std::isalnum(C)&&C!='_')Valid=false;if(!Valid)continue;
            std::istringstream Items(Line.substr(Eq+1));std::string Id;std::vector<std::string> List;
            while(std::getline(Items,Id,',')&&List.size()<24)if(Find(Id))List.push_back(Id);
            Selections[Lower(Name)]=std::move(List);
        }
    }
    std::string SkinToken(std::string Value){Value=Lower(Value);std::string Out;for(unsigned char Ch:Value)if(std::isalnum(Ch))Out.push_back((char)Ch);return Out;}
    bool MatchesSkin(UEquipableItem* Item,const FKnownSkin& Entry,USkinItem* Skin){
        std::string Name=SkinToken(Skin->SkinParam.SkinName.ToString()),Tag=SkinToken(Item->SkinTag.ToString());
        if(Tag!="none"&&!Tag.empty()&&Name.rfind(Tag,0)==0)return true;
        std::string Target=Item->GetClass()->GetName();
        if(Skin->SkinParam.SkinCategory==ESkinCategory::Skin_Weapon || Skin->SkinParam.SkinCategory==ESkinCategory::Skin_MeleeWeapon){
            if(!Item->IsA(UBaseWeaponItem::StaticClass()))return false;
            std::string Weapon=Target;const std::string Prefix="Item_Weapon_";if(Weapon.rfind(Prefix,0)!=0)return false;Weapon=Weapon.substr(Prefix.size());if(Weapon.size()>2&&Weapon.substr(Weapon.size()-2)=="_C")Weapon.resize(Weapon.size()-2);
            Weapon=SkinToken(Weapon);if(Name.rfind(Weapon,0)==0)return true;if(Weapon=="ak47")Weapon="akm";if(Weapon=="hk416")Weapon="m416";if(Weapon=="scarl")Weapon="scar";
            return Name.rfind(Weapon,0)==0;
        }
        if(Skin->SkinParam.SkinCategory==ESkinCategory::Skin_Equipment){
            std::string Id=Lower(Entry.Id),TargetName=Lower(Target);
            for(int Level=1;Level<=3;Level++){
                std::string Lv="lv"+std::to_string(Level);
                if(Id.find("back"+Lv)!=std::string::npos)return TargetName.find("item_back_")==0&&TargetName.find(Lv)!=std::string::npos;
            }
            if(Id.find("helmet")!=std::string::npos)return TargetName.find("item_head_")==0&&TargetName.find("lv")!=std::string::npos;
        }
        return false;
    }
    void Apply(ATslCharacter* C,const std::string& Name,const FKnownSkin& Entry,UClass* Class,bool Lobby) {
        if(!Class || !Class->DefaultObject)return;auto* Facade=C->GetInventoryFacade();auto* Equipment=Facade?Facade->GetEquipment():nullptr;if(!Equipment)return;
        const std::string Key=std::to_string(C->GetIndex())+":"+Entry.Id;
        if(Class->DefaultObject->IsA(UEquipableItem::StaticClass())){
            if(!Lobby)return;
            auto* Default=static_cast<UEquipableItem*>(Class->DefaultObject);
            if(Default->DamageReductionRate>0.f || GhilliePolicy::IsGhillie(Entry.Id) || GhilliePolicy::Forbidden(GWorld->GetName(),std::string(Entry.Id)+"_C")){
                if(Refused.insert(Key).second)BLOG("SKIN REFUSED cosmetic-only or map policy id="+std::string(Entry.Id));return;
            }
            FEquipPosition Position{};Position.SlotID=Default->EquipSlotID;
            auto* Existing=Equipment->GetItem(Position);
            const bool Belt=std::string(Entry.Category)=="Belts";
            if(Default->SpaceExtension>0.f && (!Belt || !Existing))return;
            if(Existing && Existing->GetClass()==Class){Applied.insert(Key);return;}
            if(Existing && GetEnvironmentVariableW(L"PUBG2019_CHARACTER_PREVIEW",nullptr,0)) {
                BLOG("PREVIEW BEFORE id="+Existing->GetClass()->GetName()+" slot="+std::to_string((int)Position.SlotID));
                Equipment->UnequipItem(Position,Existing->GetClass()->GetFName(),false);
                auto* Remains=Equipment->GetItem(Position);
                if(Remains){Equipment->UnequipItem(Position,FName{},false);Remains=Equipment->GetItem(Position);}
                if(Remains){Equipment->UnequipItem(Position,Remains->GetFName(),false);Remains=Equipment->GetItem(Position);}
                if(Remains){Equipment->DropItem(Position,FName{},true,false);Remains=Equipment->GetItem(Position);}
                BLOG(std::string("PREVIEW UNEQUIP remains=")+(Remains?Remains->GetClass()->GetName():"none"));
            }
            auto* Item=static_cast<UItem*>(UGameplayStatics::SpawnObject(Class,Equipment));
            // Belt variants retain the worn belt's capacity; appearance selection grants no carrying space.
            if(Belt && Existing && Item)static_cast<UEquipableItem*>(Item)->SpaceExtension=Existing->SpaceExtension;
            bool Accepted=Item && Equipment->TryEquipItem(Item,false,true);
            if(!Accepted && Item && GetEnvironmentVariableW(L"PUBG2019_CHARACTER_PREVIEW",nullptr,0)) {
                Accepted=Equipment->EquipItem(Position,static_cast<UEquipableItem*>(Item),false,false);
                BLOG("PREVIEW DIRECT result="+std::to_string(Accepted)+" can="+std::to_string(static_cast<UEquipableItem*>(Item)->CanEquipTo(C)));
                if(!Accepted && Existing)for(int i=0;i<Equipment->Items.Num();++i)if(Equipment->Items[i]==Existing){
                    Existing->UnequipBy(C);Equipment->Items[i]=static_cast<UEquipableItem*>(Item);
                    Accepted=static_cast<UEquipableItem*>(Item)->EquipBy(C,Position);Equipment->Items_RepNotify();
                    BLOG("PREVIEW REPLACED result="+std::to_string(Accepted));break;
                }
            }
            auto* Actual=Equipment->GetItem(Position);
            if(Accepted && Actual && Actual->GetClass()==Class){
                Applied.insert(Key);C->UpdateAppearence();C->ForceNetUpdate();Equipment->ForceNetUpdate();
                BLOG("SKIN EQUIP confirmed player="+Name+" id="+Entry.Id+" slot="+std::to_string((int)Position.SlotID));
            }else if(Refused.insert(Key).second)BLOG("SKIN EQUIP refused player="+Name+" id="+Entry.Id);
        }else if(Class->DefaultObject->IsA(USkinItem::StaticClass())){
            auto* Default=static_cast<USkinItem*>(Class->DefaultObject);
            if(Default->SkinParam.SkinCategory==ESkinCategory::Skin_Parachute){
                auto* Sky=C->SkydivingComponent;if(!Sky)return;
                if(Sky->ParachuteSkinData.SkinItemClass.Get()!=Class){Sky->ParachuteSkinData.SkinItemClass=Class;Sky->ParachuteSkinData.SkinParam=Default->SkinParam;Sky->ParachuteSkinData.SkinOwnerPlayerId=C->GetTslPlayerState()->PlayerID;if(Sky->ParachuteSKReference)Sky->OnRep_ParachuteSkinChanged();C->ForceNetUpdate();BLOG("SKIN PARACHUTE selected player="+Name+" id="+Entry.Id);}return;
            }
            if(!C->SkinHelper)return;
            auto& SkinState=SkinItems[Key];auto* Objects=UObject::GObjects.GetTypedPtr();
            if(!SkinState.first || SkinState.second<0 || SkinState.second>=Objects->Num() || Objects->GetByIndex(SkinState.second)!=SkinState.first){
                auto* Instance=static_cast<USkinItem*>(UGameplayStatics::SpawnObject(Class,C));if(!Instance)return;
                SkinState={Instance,Instance->GetIndex()};bool Picked=Instance->PickUpBy(C,FName{},FString(L""));
                BLOG("SKIN ITEM native pickup="+std::to_string(Picked)+" id="+Entry.Id+" category="+std::to_string((int)Default->SkinParam.SkinCategory)+" name="+Default->SkinParam.SkinName.ToString());
            }
            // Native skin application validates the compatible target; this never supplies a weapon or armour.
            for(auto* Item:Equipment->Items){
                if(!Item)continue;const std::string TargetKey=Key+":"+std::to_string(Item->GetIndex());if(Applied.count(TargetKey))continue;
                const auto Before=Item->ReplicatedSkinItem;const float Reduction=Item->DamageReductionRate,Capacity=Item->SpaceExtension;
                if(!MatchesSkin(Item,Entry,Default))continue;
                C->SkinHelper->Server_DoUpdateSkinItem(Item,SkinState.first);
                if(Item->ReplicatedSkinItem.SkinItemClass.Get()!=Class){
                    Item->ReplicatedSkinItem.SkinItemClass=Class;Item->ReplicatedSkinItem.SkinOwnerPlayerId=C->GetTslPlayerState()->PlayerID;
                    Item->OnRep_ReplicatedSkinItem();
                    if(Item->IsA(UBaseWeaponItem::StaticClass())){
                        auto* Weapon=static_cast<UBaseWeaponItem*>(Item)->GetEquippedWeapon();
                        if(Weapon){Weapon->ReplicatedSkinParam.SkinItemClass=Class;Weapon->ReplicatedSkinParam.SkinParam=Default->SkinParam;Weapon->ReplicatedSkinParam.SkinOwnerPlayerId=C->GetTslPlayerState()->PlayerID;Weapon->OnRep_ReplicatedSkinParam();Weapon->ForceNetUpdate();}
                    }else C->UpdateAppearence();
                }
                if(Item->ReplicatedSkinItem.SkinItemClass.Get()==Class && Item->DamageReductionRate==Reduction && Item->SpaceExtension==Capacity){
                    Applied.insert(TargetKey);C->ForceNetUpdate();Equipment->ForceNetUpdate();
                    BLOG("SKIN COSMETIC confirmed player="+Name+" id="+Entry.Id+" target="+Item->GetClass()->GetName());
                }
            }
        }
    }
    void Tick(UWorld* W) {
        const auto Now=GetTickCount64();if(!W || Now<NextTick)return;NextTick=Now+(GetEnvironmentVariableW(L"PUBG2019_CHARACTER_PREVIEW",nullptr,0)?250:750);
        if(W!=World){World=W;Loads.clear();Applied.clear();Refused.clear();SkinItems.clear();}
        if(Now>=NextFile){NextFile=Now+(GetEnvironmentVariableW(L"PUBG2019_CHARACTER_PREVIEW",nullptr,0)?350:2000);ReadSelections();}
        #ifdef SKIN_TEST_FIXTURE
        if(GetEnvironmentVariableW(L"PUBG2019_SKIN_PROBE",nullptr,0)){
            for(const char* Id:{"Item_Body_C_01","Item_Legs_C_02","Item_Jacket_C_01","Item_Skin_AKM_Orange_01"}){
                auto* Entry=Find(Id);if(!Entry)continue;auto* Class=Load(W,*Entry);std::string Key=std::string("probe:")+Id;
                if(Class && Refused.insert(Key).second)BLOG("SKIN PROBE loaded="+std::string(Id)+" type="+Class->DefaultObject->GetClass()->GetName());
            }
        }
        #endif
        if(Selections.empty())return;
        for(const auto& CI:GChars){
            if(CI.IsBot || !CI.Char || CI.Char->Health<=0.f)continue;
            auto* State=CI.Char->GetTslPlayerState();if(!State)continue;const auto Name=State->PlayerName.ToString();auto It=Selections.find(Lower(Name));if(It==Selections.end())continue;
            #ifdef SKIN_TEST_FIXTURE
            if(GetEnvironmentVariableW(L"PUBG2019_SKIN_FIXTURE",nullptr,0)){
                for(const char* Id:{"Item_Weapon_AK47_C","Item_Back_E_01_Lv1_C"}){
                    std::string Key=std::string("fixture:")+Id+":"+std::to_string(CI.Index);if(Applied.count(Key))continue;
                    auto* Class=UObject::FindClassFast(Id);if(!Class)continue;auto* Item=static_cast<UItem*>(UGameplayStatics::SpawnObject(Class,CI.Char));
                    if(Item && (std::string(Id).find("Item_Back_")==0 ? CI.Char->GetEquipment()->TryEquipItem(Item,false,true) : Item->PickUpBy(CI.Char,FName{},FString(L"")))){Applied.insert(Key);BLOG("SKIN TEST fixture item="+std::string(Id));}
                }
            }
            #endif
            for(const auto& Id:It->second){auto* Entry=Find(Id);if(Entry)Apply(CI.Char,Name,*Entry,Load(W,*Entry),GMatchStartedAt <= 0.0 && !CI.Char->IsInAircraft());}
        }
    }
}
