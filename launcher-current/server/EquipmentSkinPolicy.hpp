#pragma once
#include <algorithm>
#include <cctype>
#include <string>
namespace EquipmentSkinPolicy {
inline std::string Lower(std::string s){for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
inline int Level(const std::string& id){
 const auto s=Lower(id);
 for(int level=1;level<=3;++level)if(s.find("item_skin_backlv"+std::to_string(level)+"_")==0)return level;
 if(s=="item_skin_backpack_01")return 2;
 if(s.find("item_skin_back_a_")==0)return 3;
 if(s.find("item_skin_helmet_biker")==0||s.find("item_skin_helmet_e_")==0||s=="item_skin_helmet_999")return 1;
 if(s.find("item_skin_helmet_f_")==0)return 2;
 if(s.find("item_skin_helmet_g_")==0)return 3;
 return 0;
}
inline bool Matches(const std::string& id,const std::string& item){
 const auto skin=Lower(id),target=Lower(item);const int level=Level(skin);if(!level)return false;
 const bool helmet=skin.find("item_skin_helmet_")==0,back=skin.find("item_skin_back")==0;
 if(!(helmet&&target.find("item_head_")==0)&&!(back&&target.find("item_back_")==0))return false;
 return target.find("_lv"+std::to_string(level))!=std::string::npos;
}
}
