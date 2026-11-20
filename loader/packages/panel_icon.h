#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace panel_icon {

inline bool space(char value) {
 return value==' ' || value=='\t' || value=='\n' || value=='\r';
}

inline bool valid_svg(std::string_view value) {
 return value.size()>4 && value.size()<=2048 && value.substr(0,4)=="<svg" &&
  (space(value[4]) || value[4]=='>' || value[4]=='/') && value.find('\0')==value.npos;
}

inline std::string_view trim(std::string_view value) {
 while(!value.empty() && space(value.front())) value.remove_prefix(1);
 while(!value.empty() && space(value.back())) value.remove_suffix(1);
 return value;
}

enum class Paint { empty, black, white, authored };

inline Paint paint(std::string_view value) {
 std::string color;
 for(char ch:trim(value)) {
  if(!space(ch)) color+=ch>='A'&&ch<='Z' ? static_cast<char>(ch-'A'+'a') : ch;
 }
 if(color=="none" || color=="transparent" || color=="inherit") return Paint::empty;
 if(color=="black" || color=="#000" || color=="#000000" || color=="rgb(0,0,0)") return Paint::black;
 if(color=="white" || color=="#fff" || color=="#ffffff" || color=="rgb(255,255,255)") return Paint::white;
 return Paint::authored;
}

struct Span { size_t first, size; };

inline std::string matching_mask(std::string_view svg) {
 std::vector<Span> white;
 bool black=false;
 auto record=[&](std::string_view key,std::string_view value) {
  if(key=="color" || key=="stop-color" || key=="flood-color" || key=="lighting-color") return false;
  if(key!="fill" && key!="stroke") return true;
  value=trim(value);
  switch(paint(value)) {
   case Paint::empty: return true;
   case Paint::black: black=true; return true;
   case Paint::white: white.push_back({static_cast<size_t>(value.data()-svg.data()),value.size()}); return true;
   case Paint::authored: return false;
  }
  return false;
 };
 size_t at=0;
 while((at=svg.find('<',at))!=svg.npos) {
  if(svg.substr(at,4)=="<!--") {
   at=svg.find("-->",at+4);
   if(at==svg.npos) return std::string(svg);
   at+=3;
   continue;
  }
  const size_t name_start=++at;
  if(at<svg.size() && svg[at]=='/') {
   at=svg.find('>',at+1);
   if(at==svg.npos) return std::string(svg);
   ++at;
   continue;
  }
  while(at<svg.size() && !space(svg[at]) && svg[at]!='>' && svg[at]!='/') ++at;
  if(svg.substr(name_start,at-name_start)=="style") return std::string(svg);
  while(at<svg.size() && svg[at]!='>') {
   if(space(svg[at]) || svg[at]=='/') { ++at; continue; }
   const size_t key_start=at;
   while(at<svg.size() && !space(svg[at]) && svg[at]!='=' && svg[at]!='>') ++at;
   auto key=svg.substr(key_start,at-key_start);
   while(at<svg.size() && space(svg[at])) ++at;
   if(at==svg.size() || svg[at++]!='=') return std::string(svg);
   while(at<svg.size() && space(svg[at])) ++at;
   if(at==svg.size() || (svg[at]!='\'' && svg[at]!='"')) return std::string(svg);
   const char quote=svg[at++];
   const auto end=svg.find(quote,at);
   if(end==svg.npos) return std::string(svg);
   auto value=svg.substr(at,end-at);
   if(!record(key,value)) return std::string(svg);
   if(key=="style") {
    while(!value.empty()) {
     const auto delimiter=value.find(';');
     const auto declaration=value.substr(0,delimiter);
     const auto colon=declaration.find(':');
     if(colon!=declaration.npos && !record(trim(declaration.substr(0,colon)),declaration.substr(colon+1))) return std::string(svg);
     if(delimiter==value.npos) break;
     value.remove_prefix(delimiter+1);
    }
   }
   at=end+1;
  }
 }
 std::string result(svg);
 if(!black) for(auto it=white.rbegin();it!=white.rend();++it) result.replace(it->first,it->size,"black");
 return result;
}

inline std::string resolve(std::string_view normal,std::string_view explicit_active,bool match_colors,bool active) {
 if(active && !explicit_active.empty()) return std::string(explicit_active);
 return match_colors ? matching_mask(normal) : std::string(normal);
}
}
