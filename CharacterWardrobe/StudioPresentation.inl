// Embedded editor presentation only. No gameplay/profile changes.
namespace StudioPresentation {
static WNDPROC previousWindowProc=nullptr;
static HWND embeddedWindow=nullptr;
static bool enterHeld=false,f11Held=false;
#include "StudioMouse.inl"

static void RequestFullscreen(){
 EndNativeDrag(embeddedWindow);
 HANDLE file=CreateFileW(L"studio-fullscreen.request",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);}
}
static LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM key,LPARAM data){
 if(HandleNativeDrag(window,message,key,data))return 0;
 // Swallow the engine's Alt+Enter handling: it must resize its parent, not
 // detach the embedded viewport or enter exclusive DirectX fullscreen.
 bool down=message==WM_KEYDOWN||message==WM_SYSKEYDOWN;
 bool up=message==WM_KEYUP||message==WM_SYSKEYUP;
 bool alt=(data&(1LL<<29))!=0;
 if(key==VK_RETURN&&((down&&alt)||((down||up)&&enterHeld))){
  if(down&&!enterHeld&&(data&(1LL<<30))==0)RequestFullscreen();
  enterHeld=down;return 0;
 }
 if(key==VK_F11&&(down||up)){
  if(down&&!f11Held&&(data&(1LL<<30))==0)RequestFullscreen();
  f11Held=down;return 0;
 }
 if(message==WM_SYSCHAR&&key==VK_RETURN)return 0;
 if(message==WM_KILLFOCUS){enterHeld=false;f11Held=false;}
 auto result=CallWindowProcW(previousWindowProc,window,message,key,data);
 if(message==WM_NCDESTROY){embeddedWindow=nullptr;previousWindowProc=nullptr;enterHeld=false;f11Held=false;}
 return result;
}
static void AttachShortcuts(HWND window){
 if(!window||window==embeddedWindow||embeddedWindow)return;
 DWORD parentPid=0;HWND parent=GetParent(window);
 if(!parent||!GetWindowThreadProcessId(parent,&parentPid)||parentPid==GetCurrentProcessId())return;
 SetLastError(0);
 auto previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&WindowProc)));
 if(previous){previousWindowProc=previous;embeddedWindow=window;out<<"EMBEDDED SHORTCUTS F11 ALT_ENTER"<<std::endl;}
}

// Keep the game's original lobby/studio world and lighting visible.
static FVector Subtract(FVector a,FVector b){return {a.X-b.X,a.Y-b.Y,a.Z-b.Z};}
static float Dot(FVector a,FVector b){return a.X*b.X+a.Y*b.Y+a.Z*b.Z;}

struct CharacterPlacement {
 ALobbyCharacter* actor=nullptr;
 int index=-1;
 std::string name;
 FVector originalLocation{};
};
static CharacterPlacement placement;

// Resolve the visible character once. The array slot is captured during the
// search because UObject::Index is not a reliable slot in this game build.
static void Layout(APlayerController* pc,ULONGLONG now){
 static ULONGLONG nextLayout=0;
 if(now<nextLayout||!pc||!pc->PlayerCameraManager)return;
 nextLayout=now+750;
 if(ValidCharacter(placement.actor,placement.index)&&placement.actor->GetName()==placement.name)return;
 int width=0,height=0;pc->GetViewportSize(&width,&height);
 if(width<320||height<240)return;
 auto* manager=pc->PlayerCameraManager;auto camera=manager->GetCameraLocation();
 auto forward=UKismetMathLibrary::GetForwardVector(manager->GetCameraRotation());
 ALobbyCharacter* preview=nullptr;int previewIndex=-1;float nearest=600.f;
 auto* objects=UObject::GObjects.GetTypedPtr();
 if(objects)for(int i=0;i<objects->Num();++i){
  auto* object=objects->GetByIndex(i);
  if(!object||object->IsDefaultObject()||!object->IsA(ALobbyCharacter::StaticClass()))continue;
  auto* candidate=static_cast<ALobbyCharacter*>(object);
  if(candidate->bActorIsBeingDestroyed||!candidate->Mesh||!candidate->Mesh->SkeletalMesh)continue;
  float depth=Dot(Subtract(candidate->K2_GetActorLocation(),camera),forward);FVector2D screen{};
  if(depth>25&&depth<nearest&&UGameplayStatics::ProjectWorldToScreen(pc,candidate->K2_GetActorLocation(),&screen,false)&&
     screen.X>=0&&screen.X<width&&screen.Y>=0&&screen.Y<height){preview=candidate;previewIndex=i;nearest=depth;}
 }
 if(!preview)return;
 placement.actor=preview;placement.index=previewIndex;placement.name=preview->GetName();
 placement.originalLocation=preview->K2_GetActorLocation();
 // Move horizontally for the left category rail while preserving world height.
 {auto loc=placement.originalLocation;auto right=UKismetMathLibrary::GetRightVector(manager->GetCameraRotation());FVector shifted{loc.X+right.X*100.f,loc.Y+right.Y*100.f,loc.Z};FVector2D here{},probe{};if(pc->ProjectWorldLocationToScreen(loc,&here,false)&&pc->ProjectWorldLocationToScreen(shifted,&probe,false)&&std::abs(probe.X-here.X)>0.1f){float side=std::clamp((width*0.335f-here.X)*100.f/(probe.X-here.X),-200.f,200.f);FVector next{loc.X+right.X*side,loc.Y+right.Y*side,loc.Z};FHitResult hit{};preview->K2_SetActorLocation(next,false,&hit,true);out<<"STUDIO POSITION "<<next.X<<","<<next.Y<<","<<next.Z<<" originalZ="<<loc.Z<<std::endl;}}
 // Final calibration selected in the studio: +63 px horizontally and -9 px
 // vertically at 1920x1080, scaled to the active viewport. The actor keeps
 // its original size and the existing portrait capture remains centered.
 {auto base=preview->K2_GetActorLocation();auto rotation=manager->GetCameraRotation();auto right=UKismetMathLibrary::GetRightVector(rotation);auto up=UKismetMathLibrary::GetUpVector(rotation);
  FVector rr{base.X+right.X*100,base.Y+right.Y*100,base.Z+right.Z*100},uu{base.X+up.X*100,base.Y+up.Y*100,base.Z+up.Z*100};FVector2D p{},r{},u{};
  if(pc->ProjectWorldLocationToScreen(base,&p,false)&&pc->ProjectWorldLocationToScreen(rr,&r,false)&&pc->ProjectWorldLocationToScreen(uu,&u,false)){
   float a=r.X-p.X,b=u.X-p.X,c=r.Y-p.Y,d=u.Y-p.Y,det=a*d-b*c,dx=width*(63.f/1920.f),dy=height*(-9.f/1080.f);
   if(std::isfinite(det)&&std::abs(det)>.01f){float side=100.f*(dx*d-dy*b)/det,vertical=100.f*(dy*a-dx*c)/det;FVector target{base.X+right.X*side+up.X*vertical,base.Y+right.Y*side+up.Y*vertical,base.Z+right.Z*side+up.Z*vertical};FHitResult hit{};
    if(std::isfinite(target.X)&&std::isfinite(target.Y)&&std::isfinite(target.Z)&&preview->K2_SetActorLocation(target,false,&hit,true))out<<"STUDIO FINAL POSITION dx="<<dx<<" dy="<<dy<<std::endl;
   }
  }
 }
 TrackRotationTarget(preview,previewIndex);
 // Honor the window size without adding a synthetic background or hiding floor.
 auto* view=pc->GetViewTarget();
 if(view&&view->IsA(ACameraActor::StaticClass())){
  auto* cameraActor=static_cast<ACameraActor*>(view);
  if(cameraActor->CameraComponent)cameraActor->CameraComponent->SetConstraintAspectRatio(false);
 }
 out<<"STUDIO ORIGINAL SCENE "<<placement.name<<" slot="<<previewIndex<<std::endl;
}

static bool FaceCameraForPortrait(APlayerController* pc){
 if(!pc||!pc->PlayerCameraManager||!ValidCharacter(placement.actor,placement.index))return false;
 auto* actor=placement.actor;
 if(!portraitCapture){EndNativeDrag(embeddedWindow);portraitCapture=true;StudioWeaponPreview::HideForPortrait(pc);StudioParachutePreview::HideForPortrait();FHitResult hit{};actor->K2_SetActorLocation(placement.originalLocation,false,&hit,true);}
 auto facing=UKismetMathLibrary::FindLookAtRotation(actor->K2_GetActorLocation(),pc->PlayerCameraManager->GetCameraLocation());
 if(!std::isfinite(facing.Yaw))return false;
 // The lobby mesh faces +Y relative to the actor's +X forward direction.
 // Align the visible face, rather than the actor's right-facing axis.
 facing.Yaw-=90.f;
 facing.Pitch=0;facing.Roll=0;
 actor->ToRotate=facing;
 auto current=actor->K2_GetActorRotation();
 if(std::abs(std::remainder(current.Yaw-facing.Yaw,360.f))>.02f)actor->K2_SetActorRotation(facing,false);
 current=actor->K2_GetActorRotation();
 return std::abs(std::remainder(current.Yaw-facing.Yaw,360.f))<.5f;
}
}
