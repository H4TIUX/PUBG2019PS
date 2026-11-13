// Session-only visibility for the actual studio character, without changing
// other lobby actors the game intentionally keeps hidden.
namespace StudioCharacterVisibility {
static bool visible=true,saving=false;
static ALobbyCharacter* controlled=nullptr;static int controlledIndex=-1;
static ATslLobbyWeapon* controlledWeapon=nullptr;
static bool Visible(){return visible;}
static void Tick(AHUD* raw){
 if(std::filesystem::exists(L"studio-save.request"))saving=true;
 bool wanted=true;std::ifstream file("studio-character-visible.txt");std::string value;std::getline(file,value);if(!value.empty()&&value.back()=='\r')value.pop_back();if(value=="0"&&!saving)wanted=false;
 if(visible!=wanted){visible=wanted;out<<"STUDIO CHARACTER VISIBLE "<<(visible?1:0)<<std::endl;}
 if(controlled&&!StudioPresentation::ValidCharacter(controlled,controlledIndex)){controlled=nullptr;controlledIndex=-1;}
 auto* current=StudioPresentation::placement.actor;int index=StudioPresentation::placement.index;
 if(!visible&&StudioPresentation::ValidCharacter(current,index)){
  if(controlled!=current){controlled=nullptr;controlledIndex=-1;}
  if(!current->bHidden){current->SetActorHiddenInGame(true);controlled=current;controlledIndex=index;}
 }else if(visible&&controlled){controlled->SetActorHiddenInGame(false);controlled=nullptr;controlledIndex=-1;}
 if(raw&&raw->IsA(ALobbyHUD::StaticClass())){auto* hud=static_cast<ALobbyHUD*>(raw);auto* display=hud->LobbyWeaponDisplay;auto* weapon=display?display->SpawnedWeapon:nullptr;if(weapon!=controlledWeapon)controlledWeapon=nullptr;if(weapon&&!weapon->bActorIsBeingDestroyed){if(!visible&&!weapon->bHidden){weapon->SetActorHiddenInGame(true);controlledWeapon=weapon;}else if(visible&&controlledWeapon==weapon){weapon->SetActorHiddenInGame(false);auto* mesh=weapon->GetWeaponMeshComponent();if(mesh){mesh->SetHiddenInGame(false,true);mesh->SetVisibility(true,true);}controlledWeapon=nullptr;}}}
}
}
