// Included within StudioPresentation. Only the embedded Unreal window uses
// this input path; there are no desktop-wide hooks or cursor clipping calls.
#include "StudioHitTest.h"
static bool dragging=false;
static int dragStartX=0,dragStartAngle=0;
static volatile LONG nativeAngle=-1;
static ULONGLONG lastAngleWrite=0;
static bool portraitCapture=false;

static void PersistNativeAngle(bool force){
 int angle=int(InterlockedCompareExchange(&nativeAngle,0,0));
 auto now=GetTickCount64();
 if(angle<0||(!force&&now-lastAngleWrite<150))return;
 const auto text=std::to_string(angle);
 HANDLE file=CreateFileW(L"studio-angle.native.next",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return;
 DWORD written=0;bool ok=WriteFile(file,text.data(),DWORD(text.size()),&written,nullptr)!=FALSE&&written==text.size();CloseHandle(file);
 if(ok&&MoveFileExW(L"studio-angle.native.next",L"studio-angle.txt",MOVEFILE_REPLACE_EXISTING))lastAngleWrite=now;
}
static void EndNativeDrag(HWND window){
 if(!dragging)return;
 dragging=false;PersistNativeAngle(true);
 if(GetCapture()==window)ReleaseCapture();
}
static bool HandleNativeDrag(HWND window,UINT message,WPARAM keys,LPARAM data){
 if(portraitCapture&&(message==WM_LBUTTONDOWN||message==WM_MOUSEMOVE||message==WM_LBUTTONUP))return true;
 int x=static_cast<short>(LOWORD(data)),y=static_cast<short>(HIWORD(data));
 if(message==WM_LBUTTONDOWN){
  RECT client{};if(!GetClientRect(window,&client))return false;
  const int width=client.right,height=client.bottom;
  // Capture only the visible character area; all wardrobe controls receive input.
  if(!StudioDragHit(x,y,width,height))return false;
  int angle=int(InterlockedCompareExchange(&nativeAngle,0,0));
  if(angle<0&&!ReadStudioAngle(angle))angle=0;
  InterlockedExchange(&nativeAngle,angle);dragStartAngle=angle;dragStartX=x;
  dragging=true;SetCapture(window);return true;
 }
 if(message==WM_MOUSEMOVE&&dragging){
  if(!(keys&MK_LBUTTON)){EndNativeDrag(window);return true;}
  int angle=(dragStartAngle+int(std::lround((x-dragStartX)*.45)))%360;if(angle<0)angle+=360;
  // Mouse moves only publish memory; mouse-up/focus loss persists the angle.
  InterlockedExchange(&nativeAngle,angle);return true;
 }
 if(message==WM_LBUTTONUP&&dragging){
  int angle=(dragStartAngle+int(std::lround((x-dragStartX)*.45)))%360;if(angle<0)angle+=360;
  InterlockedExchange(&nativeAngle,angle);EndNativeDrag(window);return true;
 }
 if(message==WM_KILLFOCUS||message==WM_CAPTURECHANGED||message==WM_CANCELMODE)EndNativeDrag(window);
 return false;
}

struct RotationTarget {int index=-1;ALobbyCharacter* actor=nullptr;float yaw=0;};
static RotationTarget rotationTarget;
static int previousAngle=-1;
static bool ValidCharacter(ALobbyCharacter* actor,int index){
 auto* objects=UObject::GObjects.GetTypedPtr();
 return actor&&objects&&index>=0&&index<objects->Num()&&objects->GetByIndex(index)==actor&&
        !actor->bActorIsBeingDestroyed&&actor->IsA(ALobbyCharacter::StaticClass());
}
static void TrackRotationTarget(ALobbyCharacter* actor,int objectIndex){
 if(!actor||(rotationTarget.actor==actor&&rotationTarget.index==objectIndex))return;
 rotationTarget={objectIndex,actor,actor->K2_GetActorRotation().Yaw};previousAngle=-1;
}
static void UpdateRotation(ULONGLONG now){
 static ULONGLONG next=0,nextFileRead=0;static int fileAngle=0;
 if(now<next)return;next=now+16;
 if(portraitCapture||!ValidCharacter(rotationTarget.actor,rotationTarget.index))return;
 int angle=int(InterlockedCompareExchange(&nativeAngle,0,0));
 if(angle<0){
  if(now>=nextFileRead){nextFileRead=now+150;int value=0;if(ReadStudioAngle(value))fileAngle=value;}
  angle=fileAngle;
 }
 if(angle==previousAngle)return;
 auto* actor=rotationTarget.actor;
 auto rotation=actor->K2_GetActorRotation();rotation.Yaw=rotationTarget.yaw+angle;
 // Keep the native interpolation target in sync with the displayed angle.
 actor->ToRotate=rotation;actor->K2_SetActorRotation(rotation,false);
 previousAngle=angle;
}
