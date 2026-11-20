#include <cassert>
#include "../loader/windows/vendor/json.hpp"
#include "../loader/packages/package_model.h"

int main() {
 const std::string white=R"(<svg><path fill="none" stroke="#FFF"/></svg>)";
 const std::string black=R"(<svg><path fill="none" stroke="black"/></svg>)";
 const std::string red=R"(<svg><path fill="#e61010"/></svg>)";
 bool native=false;
 assert(panel_icon::resolve(white,{},true,true,&native)==black && native);
 assert(panel_icon::resolve(white,red,true,true,&native)==red && !native);
 assert(panel_icon::resolve(white,{},true,false)==black);
 assert(panel_icon::resolve(white,{},true,true)==black);
 assert(panel_icon::resolve(white,red,true,true)==red);
 assert(panel_icon::resolve(white,red,true,false)==black);
 assert(panel_icon::resolve(white,{},false,false)==white);
 assert(panel_icon::resolve(red,{},true,false)==red);
 assert(panel_icon::matching_mask(R"(<svg><path style='stroke: RGB(255, 255, 255); fill:none' /></svg>)") ==
        R"(<svg><path style='stroke: black; fill:none' /></svg>)");
 const std::string two_tones=R"(<svg><path fill="white"/><path fill="black"/></svg>)";
 assert(panel_icon::matching_mask(two_tones)==two_tones);
 assert(panel_icon::matching_mask(two_tones,&native)==two_tones && !native);
 const std::string authored=R"(<svg><path fill="red"/><path fill="black"/></svg>)";
 assert(panel_icon::resolve(authored,{},true,true,&native)==authored && !native);
 const std::string stylesheet=R"(<svg><style>path {fill:red}</style><path fill="white"/></svg>)";
 assert(panel_icon::matching_mask(stylesheet)==stylesheet);
 const std::string comment=R"(<svg><!-- fill="red" --><path fill="white"/></svg>)";
 assert(panel_icon::matching_mask(comment)==R"(<svg><!-- fill="red" --><path fill="black"/></svg>)");

 package_model::Manifest manifest;
 nlohmann::json definition={{"schema",1},{"id","test.icon"},{"nativeLibraries",nlohmann::json::object()},{"icon",white}};
 assert(package_model::parse(definition,manifest).empty());
 assert(manifest.icon==white && manifest.active_icon.empty() && manifest.match_icon_colors);
 definition["activeIcon"]=red;
 definition["matchIconColors"]=false;
 assert(package_model::parse(definition,manifest).empty());
 assert(manifest.active_icon==red && !manifest.match_icon_colors);
 definition["activeIcon"]="<svgfile/>";
 assert(package_model::parse(definition,manifest)=="activeIcon");
 definition["activeIcon"]=red;
 definition["matchIconColors"]=1;
 assert(package_model::parse(definition,manifest)=="matchIconColors");
}
