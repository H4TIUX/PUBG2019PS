// Included inside the bot manager namespace; every engine operation stays on its game thread.
#include "SkinCatalog.hpp"
#include "EquipmentSkinPolicy.hpp"
namespace SkinSelection {
    struct LoadState { int Index=-1; UClass* Class=nullptr; ULONGLONG Requested=0; int Attempts=0; };
    std::unordered_map<std::string,LoadState> Loads;
    std::unordered_map<std::string,std::vector<std::string>> Selections;
    std::unordered_map<std::string,std::string> Genders;
    std::unordered_map<std::string,std::unordered_map<std::string,std::string>> Features;
    std::unordered_set<std::string> Applied, Refused;
    std::unordered_map<std::string,std::pair<USkinItem*,int>> SkinItems;
    struct EquipFailure {int ExistingIndex; int ItemCount; float CurrentSpace; float MaxSpace;};
    std::unordered_map<std::string,EquipFailure> EquipFailures;
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
        // Resolve by its asset path. FindClassFast walks every loaded UObject;
        // with many selected skins that scan used to run for every missing
        // class on every appearance tick, even between 10-second retries.
        TSoftClassPtr<UClass> Reference{};Reference.ObjectID.AssetLongPathname=FString(Entry.Path);
        State.Class=UKismetSystemLibrary::Conv_AssetClassToClass(Reference).Get();
        if(State.Class){State.Index=State.Class->GetIndex();return State.Class;}
        const auto Now=GetTickCount64();if(State.Attempts>=3 || (State.Requested && Now-State.Requested<10000))return nullptr;
        State.Requested=Now;++State.Attempts;
        FLatentActionInfo Latent{};Latent.Linkage=-1;Latent.UUID=170000+(int)(&Entry-KnownSkins);Latent.CallbackTarget=W;
        UKismetSystemLibrary::LoadAssetClass(W,Reference,{},Latent);
        BLOG("SKIN LOAD requested="+std::string(Entry.Id));
        return nullptr;
    }
    void ReadSelections() {
        Selections.clear();Genders.clear();Features.clear();std::ifstream File(Folder()/L"LAN_Appearance.ini");std::string Line,Section;
        while(std::getline(File,Line)){
            if(Line.size()>4096)continue;if(!Line.empty()&&Line.back()=='\r')Line.pop_back();
            if(!Line.empty()&&Line.front()=='['){Section=Line;continue;}if(Section!="[Appearance]"&&Section!="[Character]"&&Section!="[Features]")continue;
            auto Eq=Line.find('=');if(Eq==std::string::npos||Eq<1||Eq>24)continue;std::string Name=Line.substr(0,Eq);bool Valid=true;
            for(unsigned char C:Name)if(!std::isalnum(C)&&C!='_'&&C!='-')Valid=false;if(!Valid)continue;
            const std::string Value=Line.substr(Eq+1),Player=Lower(Name);
            if(Section=="[Character]"){if(Value=="male"||Value=="female")Genders[Player]=Value;continue;}
            if(Section=="[Features]"){if(Value.size()>1000)continue;std::istringstream Values(Value);std::string Pair;while(std::getline(Values,Pair,',')){auto Sep=Pair.find(':');if(Sep==std::string::npos)continue;auto Group=Pair.substr(0,Sep),Id=Pair.substr(Sep+1);if(Group!="Hair"&&Group!="Face"&&Group!="NudeBody"&&Group!="Makeup"&&Group!="SkinColor"&&Group!="BodyType"&&Group!="BodyTattoos")continue;if(Id.rfind("Item_",0)!=0||Id.size()>100||Lower(Id).find("zombie")!=std::string::npos||Lower(Id).find("ghillie")!=std::string::npos)continue;bool Safe=true;for(unsigned char C:Id)if(!std::isalnum(C)&&C!='_')Safe=false;if(Safe)Features[Player][Group]=Id;}continue;}
            std::istringstream Items(Value);std::string Id;std::vector<std::string> List;
            while(std::getline(Items,Id,',')&&List.size()<64)if(Find(Id))List.push_back(Id);
            Selections[Lower(Name)]=std::move(List);
        }
    }
    const std::vector<std::string>* SelectedFor(ATslCharacter* C) {
        auto* State=C ? C->GetTslPlayerState() : nullptr;
        if (!State) return nullptr;
        const auto It=Selections.find(Lower(State->PlayerName.ToString()));
        return It==Selections.end() ? nullptr : &It->second;
    }
    bool HasAppearance(ATslCharacter* C) { return SelectedFor(C)!=nullptr; }
    bool ReservesSlot(ATslCharacter* C, EEquipSlotID Slot) {
        const auto* Selected=SelectedFor(C); if (!Selected) return false;
        const char* Category=nullptr;
        switch (Slot) {
            case EEquipSlotID::Outer: Category="Jackets"; break;
            case EEquipSlotID::Torso: Category="Tops"; break;
            case EEquipSlotID::Legs: Category="Pants"; break;
            case EEquipSlotID::Feet: Category="Footwear"; break;
            case EEquipSlotID::Head: Category="Headwear"; break;
            case EEquipSlotID::Eyes: Category="Eyewear"; break;
            case EEquipSlotID::Hands: Category="Gloves"; break;
            case EEquipSlotID::Mask: Category="Masks"; break;
            default: return false;
        }
        for (const auto& Id:*Selected) {const auto* Entry=Find(Id);if (Entry && std::string(Entry->Category)==Category) return true;}
        return false;
    }
    std::string SkinToken(std::string Value){Value=Lower(Value);std::string Out;for(unsigned char Ch:Value)if(std::isalnum(Ch))Out.push_back((char)Ch);return Out;}
    bool MatchesSkin(UEquipableItem* Item,const FKnownSkin& Entry,USkinItem* Skin){
        if(Skin->SkinParam.SkinCategory==ESkinCategory::Skin_Equipment)
            return EquipmentSkinPolicy::Matches(Entry.Id,Item->GetClass()->GetName());
        std::string Name=SkinToken(Skin->SkinParam.SkinName.ToString()),Tag=SkinToken(Item->SkinTag.ToString());
        if(Tag!="none"&&!Tag.empty()&&Name.rfind(Tag,0)==0)return true;
        std::string Target=Item->GetClass()->GetName();
        if(Skin->SkinParam.SkinCategory==ESkinCategory::Skin_Weapon || Skin->SkinParam.SkinCategory==ESkinCategory::Skin_MeleeWeapon){
            if(!Item->IsA(UBaseWeaponItem::StaticClass()))return false;
            std::string Weapon=Target;const std::string Prefix="Item_Weapon_";if(Weapon.rfind(Prefix,0)!=0)return false;Weapon=Weapon.substr(Prefix.size());if(Weapon.size()>2&&Weapon.substr(Weapon.size()-2)=="_C")Weapon.resize(Weapon.size()-2);
            Weapon=SkinToken(Weapon);if(Name.rfind(Weapon,0)==0)return true;if(Weapon=="ak47")Weapon="akm";if(Weapon=="hk416")Weapon="m416";if(Weapon=="scarl")Weapon="scar";
            return Name.rfind(Weapon,0)==0;
        }
        return false;
    }
    void Apply(ATslCharacter* C,const std::string& Name,const FKnownSkin& Entry,UClass* Class,bool Lobby) {
        if(!Class || !Class->DefaultObject)return;auto* Facade=C->GetInventoryFacade();auto* Equipment=Facade?Facade->GetEquipment():nullptr;if(!Equipment)return;
        const std::string Key=std::to_string(C->GetIndex())+":"+Entry.Id;
        if(Class->DefaultObject->IsA(UEquipableItem::StaticClass())){
            if(!Lobby || Applied.count(Key))return;
            auto* Default=static_cast<UEquipableItem*>(Class->DefaultObject);
            if(Default->DamageReductionRate>0.f || GhilliePolicy::IsGhillie(Entry.Id) || GhilliePolicy::Forbidden(GWorld->GetName(),std::string(Entry.Id)+"_C")){
                if(Refused.insert(Key).second)BLOG("SKIN REFUSED cosmetic-only or map policy id="+std::string(Entry.Id));return;
            }
            FEquipPosition Position{};Position.SlotID=Default->EquipSlotID;
            auto* Existing=Equipment->GetItem(Position);
            auto* Inventory=Facade->GetInventory();
            auto Previous=EquipFailures.find(Key);
            if(Previous!=EquipFailures.end() && Inventory) {
                const auto& Failure=Previous->second;
                if(Failure.ExistingIndex==(Existing?Existing->GetIndex():-1) && Failure.ItemCount==Inventory->Items.Num() &&
                   Failure.CurrentSpace==Inventory->GetCurrentSpace() && Failure.MaxSpace==Inventory->GetMaxSpace()) return;
                EquipFailures.erase(Previous);
            }
            const bool Belt=std::string(Entry.Category)=="Belts";
            if(Default->SpaceExtension>0.f && (!Belt || !Existing))return;
            if(Existing && Existing->GetClass()==Class){Applied.insert(Key);return;}
            // Native swapping puts the old item into the bag. Avoid triggering
            // its visible no-room notification when that swap cannot fit.
            const float ExistingSpace=Existing ? Existing->SpacePerItem*(float)(std::max)(1,Existing->StackCount) : 0.f;
            if(Existing && Inventory && std::isfinite(ExistingSpace) && ExistingSpace > (std::max)(0.f,Inventory->GetMaxSpace()-Inventory->GetCurrentSpace())+0.001f) {
                EquipFailures[Key]={Existing->GetIndex(),Inventory->Items.Num(),Inventory->GetCurrentSpace(),Inventory->GetMaxSpace()};
                if(Refused.insert(Key).second)BLOG("SKIN EQUIP deferred: no space for existing item player="+Name+" id="+Entry.Id);
                return;
            }
            auto* Item=static_cast<UItem*>(UGameplayStatics::SpawnObject(Class,Equipment));
            // Belt variants retain the worn belt's capacity; appearance selection grants no carrying space.
            if(Belt && Existing && Item)static_cast<UEquipableItem*>(Item)->SpaceExtension=Existing->SpaceExtension;
            bool Accepted=Item && Equipment->TryEquipItem(Item,false,true);
            auto* Actual=Equipment->GetItem(Position);
            if(Accepted && Actual && Actual->GetClass()==Class){
                Applied.insert(Key);C->UpdateAppearence();C->ForceNetUpdate();Equipment->ForceNetUpdate();
                BLOG("SKIN EQUIP confirmed player="+Name+" id="+Entry.Id+" slot="+std::to_string((int)Position.SlotID));
            }else {
                if(Inventory)EquipFailures[Key]={Existing?Existing->GetIndex():-1,Inventory->Items.Num(),Inventory->GetCurrentSpace(),Inventory->GetMaxSpace()};
                if(Refused.insert(Key).second)BLOG("SKIN EQUIP refused player="+Name+" id="+Entry.Id);
            }
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
    // The offline game initially names/spawns a remote player using its machine
    // nickname. Once its launcher identity arrives, create the correct native
    // gender pawn before applying the outfit. Male and female models have
    // different compiled customization tables and cannot share descriptor IDs.
    void EnsurePlayerModels(UWorld* W){
        if(!W||!W->AuthorityGameMode)return;
        const auto Now=GetTickCount64();if(Now>=NextFile){NextFile=Now+2000;ReadSelections();}
        if(Genders.empty())return;
        TArray<AActor*> Controllers;UGameplayStatics::GetAllActorsOfClass(W,APlayerController::StaticClass(),&Controllers);
        for(auto* Actor:Controllers){
            auto* PC=static_cast<APlayerController*>(Actor);if(!PC||!PC->NetConnection||PC->bIsLocalPlayerController||!PC->PlayerState)continue;
            const auto Name=PC->PlayerState->PlayerName.ToString();auto G=Genders.find(Lower(Name));if(G==Genders.end())continue;
            const bool Female=G->second=="female";auto* Pawn=PC->K2_GetPawn();if(!Pawn||!Pawn->IsA(ATslCharacter::StaticClass()))continue;
            auto* C=static_cast<ATslCharacter*>(Pawn);auto* Default=static_cast<ATslCharacter*>(C->GetClass()->DefaultObject);
            if(!Default||((uint8)Default->Gender!=0)==Female||C->Health<=0.f||C->GetZombieModePlayerState()==EZombieModePlayerState::Zombie)continue;
            const std::string Key="model:"+std::to_string(C->GetIndex());if(Applied.count(Key))continue;
            auto* Selected=UObject::FindClassFast(Female?"PlayerFemale_A_C":"PlayerMale_A_C");if(!Selected)continue;
            const auto Transform=C->GetTransform();C->SetActorEnableCollision(false);
            auto* Replacement=W->AuthorityGameMode->SpawnDefaultPawnAtTransform(PC,Transform);
            if(!Replacement||!Replacement->IsA(ATslCharacter::StaticClass())||Replacement->GetClass()!=Selected){if(Replacement)Replacement->K2_DestroyActor();C->SetActorEnableCollision(true);continue;}
            Applied.insert(Key);PC->UnPossess();PC->Possess(Replacement);W->AuthorityGameMode->RestartPlayerAtTransform(PC,Transform);C->K2_DestroyActor();
            BLOG("CHARACTER native model selected player="+Name+" class="+Replacement->GetClass()->GetName());
        }
    }
    void ApplyFeatures(ATslCharacter* C,const std::string& Name){
        auto Gender=Genders.find(Lower(Name));if(Gender==Genders.end())return;
        auto* Instance=C->CustomizableObjectInstance;auto* Model=Instance?Instance->CustomizableObject:nullptr;
        if(Refused.insert("feature-source:"+std::to_string(C->GetIndex())).second)BLOG("CHARACTER source player="+Name+" instance="+std::to_string(Instance!=nullptr)+" model="+std::to_string(Model!=nullptr)+" parameters="+std::to_string(Instance?Instance->IntParameters.Num():0));
        if(!Model)return;
        FRawArray Raw{};std::memcpy(&Raw,&C->InstanceDescriptor,sizeof(Raw));
        if(!Raw.Data||Raw.Num!=8+Instance->IntParameters.Num()*4||Instance->IntParameters.Num()<1||Instance->IntParameters.Num()>512||!Readable(Raw.Data,Raw.Num))return;
        auto* Data=static_cast<uint8*>(Raw.Data);uint32 Version=0;std::memcpy(&Version,Data,4);if(Version!=1)return;
        bool Changed=false;const int32 WantedGender=Gender->second=="female"?1:0;
        C->Gender=(EGender)WantedGender;
        auto Profile=Features.find(Lower(Name));
        std::unordered_map<std::string,std::string> Choices{{"Face","Item_Normal"},{"NudeBody","Item_Normal"},{"BodyType","Item_BodyType_Body1"},{"SkinColor","Item_SkinColor_Color1"},{"Makeup",""},{"BodyTattoos",""}};
        if(Profile!=Features.end())for(const auto& Feature:Profile->second)Choices[Feature.first]=Feature.second;
        for(const auto& Feature:Choices){
            const std::wstring Group(Feature.first.begin(),Feature.first.end());const FString Key(Group.c_str());
            const int Index=Instance->FindIntParameterNameIndex(Key),Parameter=Model->FindParameter(Key);
            if(Index<0||Index>=Instance->IntParameters.Num()||Parameter<0)continue;
            const auto& Id=Feature.second;std::string Desired;
            if(Id.empty()){const int32 Empty=-1;int32 Before=0;std::memcpy(&Before,Data+8+Index*4,4);if(Before!=Empty){std::memcpy(Data+8+Index*4,&Empty,4);Changed=true;}continue;}
            const std::string Prefix="Item_"+Feature.first+"_";
            if((Feature.first=="SkinColor"||Feature.first=="BodyType"||Feature.first=="BodyTattoos")&&Id.rfind(Prefix,0)==0)Desired=Id.substr(Prefix.size());
            else Desired=std::string(WantedGender?"F_":"M_")+Id.substr(5);
            if(Parameter>=Model->ParameterProperties.Num())continue;
            const auto& Property=Model->ParameterProperties[Parameter];
            if(Property.Name.ToString()!=Feature.first||Property.PossibleValues.Num()>5000)continue;
            for(const auto& Option:Property.PossibleValues){
                if(Option.Name.ToString()!=Desired)continue;
                const int32 Encoded=Option.Value;int32 Before=0;std::memcpy(&Before,Data+8+Index*4,4);
                if(Before!=Encoded){std::memcpy(Data+8+Index*4,&Encoded,4);Changed=true;}
                break;
            }
        }
        if(Changed){C->OnRep_InstanceDescriptor();C->ForceNetUpdate();BLOG("CHARACTER selected player="+Name+" gender="+Gender->second+" features="+std::to_string(Profile==Features.end()?0:Profile->second.size()));}
    }
    void Tick(UWorld* W) {
        const auto Now=GetTickCount64();if(!W || Now<NextTick)return;NextTick=Now+750;
        if(W!=World){World=W;Loads.clear();Applied.clear();Refused.clear();SkinItems.clear();EquipFailures.clear();}
        if(Now>=NextFile){NextFile=Now+2000;ReadSelections();}
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
            // The native equipment system can drop an outer garment when a
            // torso item is equipped afterwards. Load all classes as before,
            // but put tops on before jackets regardless of saved list order.
            for(int Pass=0;Pass<2;++Pass) for(const auto& Id:It->second){
                auto* Entry=Find(Id);if(!Entry)continue;
                const bool Outer=std::string(Entry->Category)=="Jackets";
                if(Outer!=(Pass==1))continue;
                if(Outer){
                    bool TopPending=false;
                    for(const auto& Other:It->second){
                        auto* Top=Find(Other);if(!Top || std::string(Top->Category)!="Tops")continue;
                        const auto TopKey=std::to_string(CI.Char->GetIndex())+":"+Other;
                        if(!Applied.count(TopKey)){TopPending=true;break;}
                    }
                    if(TopPending)continue;
                }
                Apply(CI.Char,Name,*Entry,Load(W,*Entry),GMatchStartedAt <= 0.0 && !CI.Char->IsInAircraft());
            }
            ApplyFeatures(CI.Char,Name);
        }
    }
}
