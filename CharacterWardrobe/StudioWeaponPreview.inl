// Weapon display assets are loaded only for the currently selected item.
namespace StudioWeaponPreview {
static std::unordered_map<std::string,std::string> assets,bases;
static bool initialized=false,shown=false,requested=false,issued=false,scaled=false;
static std::string desired,skinName;
static ULONGLONG started=0,issuedAt=0;
static int uuid=830000;
static std::string Lower(std::string s){for(char& c:s)c=char(std::tolower((unsigned char)c));return s;}
static bool SafeId(const std::string& s){if(s.rfind("Item_Skin_",0)!=0||s.size()>100)return false;for(unsigned char c:s)if(!std::isalnum(c)&&c!='_'&&c!='-')return false;return true;}
// Unreal names are case-insensitive; the SDK's FindClassFast is not.
static UClass* FindSkinClass(const std::string& id){auto it=assets.find(id);if(it!=assets.end()){TSoftClassPtr<UClass> reference{};auto path=Wide(it->second);reference.ObjectID.AssetLongPathname=FString(path.c_str());auto resolved=UKismetSystemLibrary::Conv_AssetClassToClass(reference);if(resolved.Get())return resolved.Get();}const auto want=id+"_C";if(auto* exact=UObject::FindClassFast(want))return exact;auto* objects=UObject::GObjects.GetTypedPtr();if(objects)for(int i=0;i<objects->Num();++i){auto* object=objects->GetByIndex(i);if(object&&object->HasTypeFlag(EClassCastFlags::Class)&&_stricmp(object->GetName().c_str(),want.c_str())==0)return static_cast<UClass*>(object);}return nullptr;}
static void Initialize(){
 if(initialized)return;initialized=true;
 std::ifstream catalog("studio-weapon-catalog.tsv");std::string line;
 while(std::getline(catalog,line)){std::istringstream row(line);std::string category,id,path;if(!std::getline(row,category,'\t')||!std::getline(row,id,'\t')||!std::getline(row,path))continue;if(!path.empty()&&path.back()=='\r')path.pop_back();const std::string expected="/Game/Item/Skin/Weapon/"+id+"."+id+"_C";if(category=="Weapon skins"&&SafeId(id)&&path==expected)assets[id]=path;}
 std::ifstream mapping("studio-weapon-bases.tsv");while(std::getline(mapping,line)){auto p=line.find('\t');if(p==std::string::npos)continue;auto name=line.substr(0,p),tags=line.substr(p+1);if(!tags.empty()&&tags.back()=='\r')tags.pop_back();if(name.size()<100&&tags.size()<250)bases[Lower(name)]=tags;}
 out<<"WEAPON CATALOG "<<assets.size()<<" bases="<<bases.size()<<std::endl;
}
static void HideForPortrait(APlayerController* pc){if(!pc||!shown)return;auto* raw=pc->GetHUD();if(raw&&raw->IsA(ALobbyHUD::StaticClass())){auto* hud=static_cast<ALobbyHUD*>(raw);if(hud->LobbyWeaponDisplay)hud->LobbyWeaponDisplay->SetActorTickEnabled(true);hud->WMHideWeapon();shown=false;}}
static void SuppressUnposed(AHUD* raw){
 if(!shown||scaled||!raw||!raw->IsA(ALobbyHUD::StaticClass()))return;
 auto* display=static_cast<ALobbyHUD*>(raw)->LobbyWeaponDisplay;auto* weapon=display?display->SpawnedWeapon:nullptr;
 if(weapon&&!weapon->bActorIsBeingDestroyed&&!weapon->bHidden){weapon->SetActorHiddenInGame(true);out<<"WEAPON PREVIEW hidden while loading "<<desired<<std::endl;}
}
static void Tick(UWorld* world,AHUD* raw,APlayerController* pc,ULONGLONG now){
 if(!world||!raw||!raw->IsA(ALobbyHUD::StaticClass()))return;Initialize();auto* hud=static_cast<ALobbyHUD*>(raw);
 std::ifstream file("studio-weapon-item.txt");std::string item;std::getline(file,item);if(!item.empty()&&item.back()=='\r')item.pop_back();if(!item.empty()&&(!SafeId(item)||assets.find(item)==assets.end()))item.clear();
 if(item!=desired){if(shown){if(hud->LobbyWeaponDisplay)hud->LobbyWeaponDisplay->SetActorTickEnabled(true);hud->WMHideWeapon();shown=false;}desired=item;requested=false;issued=false;scaled=false;skinName.clear();started=now;out<<"WEAPON SELECT "<<desired<<std::endl;}
 if(desired.empty())return;
 if(!requested){TSoftClassPtr<UClass> ref{};auto path=Wide(assets[desired]);ref.ObjectID.AssetLongPathname=FString(path.c_str());FLatentActionInfo latent{};latent.UUID=uuid++;latent.Linkage=-1;latent.CallbackTarget=world;UKismetSystemLibrary::LoadAssetClass(world,ref,{},latent);requested=true;out<<"WEAPON PREVIEW loading "<<desired<<std::endl;return;}
 if(!issued){if(now-started>30000){if(now-started<32000)out<<"WEAPON PREVIEW timeout "<<desired<<std::endl;return;}auto* cls=FindSkinClass(desired);if(!cls||!cls->DefaultObject||!cls->DefaultObject->IsA(USkinItem::StaticClass())){static std::string noted;if(now-started>8000&&noted!=desired){noted=desired;out<<"WEAPON PREVIEW waiting "<<desired<<" class="<<(void*)cls<<std::endl;}return;}auto* skin=static_cast<USkinItem*>(cls->DefaultObject);if(skin->SkinParam.SkinCategory!=ESkinCategory::Skin_Weapon&&skin->SkinParam.SkinCategory!=ESkinCategory::Skin_MeleeWeapon)return;skinName=skin->SkinParam.SkinName.ToString();
 // Two shipped item classes refer to misspelled database keys. Correct only
 // this temporary studio process, before asking the game's display to load it.
 const wchar_t* corrected=nullptr;
 if(desired=="Item_Skin_Machete_HarleyQuinn_01"&&skinName=="Machete_HarleyQuinn_01")corrected=L"Cowbar_HarleyQuinn_01";
 if(desired=="Item_Skin_Scar-L_Rosewood_01"&&Lower(skinName)=="scar-l_rosewood_01")corrected=L"Scar_L_Rosewood_01";
 if(corrected){skin->SkinParam.SkinName=UKismetStringLibrary::Conv_StringToName(FString(corrected));skinName=skin->SkinParam.SkinName.ToString();out<<"WEAPON PREVIEW corrected database key "<<desired<<" => "<<skinName<<std::endl;}
 auto found=bases.find(Lower(skinName));if(found==bases.end()){out<<"WEAPON PREVIEW no base "<<desired<<" skin="<<skinName<<std::endl;started=0;return;}std::string tags=found->second,base=tags.substr(0,tags.find(','));if(desired.find("Machete_")!=std::string::npos&&tags.find("Item_Weapon_Machete")!=std::string::npos)base="Item_Weapon_Machete";auto wideBase=Wide(base+"_C"),widePath=Wide(assets[desired]);if(hud->LobbyWeaponDisplay){hud->LobbyWeaponDisplay->SetActorTickEnabled(true);hud->WMHideWeapon();}hud->WMShowWeapon(FString(wideBase.c_str()),FString(L""),FString(L""),FString(widePath.c_str()),false);shown=true;issued=true;issuedAt=now;out<<"WEAPON PREVIEW show "<<desired<<" base="<<base<<" skin="<<skinName<<std::endl;return;}
 if(!scaled&&now-issuedAt<30000){auto* display=hud->LobbyWeaponDisplay;if(display&&display->SpawnedWeapon){SuppressUnposed(raw);auto* mesh=display->SpawnedWeapon->GetWeaponMeshComponent();if(mesh&&mesh->ReplicatedSkinParam.SkinParam.SkinName.ToString()==skinName){auto* actor=display->SpawnedWeapon;display->SetActorTickEnabled(false);for(int i=0;i<display->AttachmentMeshes.Num()&&i<64;++i){auto* overlay=display->AttachmentMeshes[i];if(overlay&&overlay->bRenderCustomDepth)overlay->SetRenderCustomDepth(false);}actor->SetActorScale3D(FVector{0.35f,0.35f,0.35f});
 if(pc&&pc->PlayerCameraManager){auto loc=actor->K2_GetActorLocation();FVector centerWorld{},extent{};float radius=0;UKismetSystemLibrary::GetComponentBounds(mesh,&centerWorld,&extent,&radius);
 // Keep small weapons readable while bounding large rifles by the available space.
 int32 vw=0,vh=0;pc->GetViewportSize(&vw,&vh);float minX=1e9f,minY=1e9f,maxX=-1e9f,maxY=-1e9f;int projected=0;
 for(int bits=0;bits<8;++bits){FVector corner{centerWorld.X+((bits&1)?extent.X:-extent.X),centerWorld.Y+((bits&2)?extent.Y:-extent.Y),centerWorld.Z+((bits&4)?extent.Z:-extent.Z)};FVector2D point{};if(pc->ProjectWorldLocationToScreen(corner,&point,false)){minX=std::min(minX,point.X);maxX=std::max(maxX,point.X);minY=std::min(minY,point.Y);maxY=std::max(maxY,point.Y);++projected;}}
 if(projected==8&&vh>0){float size=std::max(maxX-minX,maxY-minY);if(size>1.f&&std::isfinite(size)){float factor=std::clamp(vh*.33f/size,.55f,3.2f);float scale=.35f*factor;actor->SetActorScale3D(FVector{scale,scale,scale});UKismetSystemLibrary::GetComponentBounds(mesh,&centerWorld,&extent,&radius);out<<"WEAPON SCALE "<<desired<<" scale="<<scale<<" projected="<<size<<std::endl;}}
 auto rotation=pc->PlayerCameraManager->GetCameraRotation();auto right=UKismetMathLibrary::GetRightVector(rotation);auto up=UKismetMathLibrary::GetUpVector(rotation);FVector2D center{},rightScreen{},upScreen{};FVector r{centerWorld.X+right.X*100,centerWorld.Y+right.Y*100,centerWorld.Z+right.Z*100};FVector u{centerWorld.X+up.X*100,centerWorld.Y+up.Y*100,centerWorld.Z+up.Z*100};int32 width=0,height=0;pc->GetViewportSize(&width,&height);if(width>0&&height>0&&pc->ProjectWorldLocationToScreen(centerWorld,&center,false)&&pc->ProjectWorldLocationToScreen(r,&rightScreen,false)&&pc->ProjectWorldLocationToScreen(u,&upScreen,false)){float dx=rightScreen.X-center.X,dy=upScreen.Y-center.Y;if(std::abs(dx)>0.1f&&std::abs(dy)>0.1f){float side=std::clamp((width*0.23f-center.X)*100.f/dx,-300.f,300.f);float vertical=std::clamp((height*0.45f-center.Y)*100.f/dy,-300.f,300.f);FVector moved{loc.X+right.X*side+up.X*vertical,loc.Y+right.Y*side+up.Y*vertical,loc.Z+right.Z*side+up.Z*vertical};FHitResult hit{};actor->K2_SetActorLocation(moved,false,&hit,true);out<<"WEAPON POSITION "<<center.X<<","<<center.Y<<" viewport="<<width<<"x"<<height<<" offset="<<side<<","<<vertical<<std::endl;}}}
 scaled=true;out<<"WEAPON PREVIEW ready "<<desired<<std::endl;}}}
}
}
