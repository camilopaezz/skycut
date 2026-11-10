#include "PltGuides.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <vector>

namespace {
struct Point { int x, y; };
struct Path { size_t start, end; int pen; std::vector<Point> points; };
std::string Trim(const std::string &s) {
    size_t start=s.find_first_not_of(" \t\r\n"), end=s.find_last_not_of(" \t\r\n");
    return start==std::string::npos?"":s.substr(start,end-start+1);
}
bool Same(Point a, Point b) {return a.x==b.x && a.y==b.y;}
bool Bounds(const Path &p, int &left, int &bottom, int &right, int &top) {
    if (p.pen!=1 || p.points.size()!=5 || !Same(p.points.front(),p.points.back())) return false;
    left=right=p.points[0].x; bottom=top=p.points[0].y;
    for(Point q:p.points) {
        left=std::min(left,q.x); right=std::max(right,q.x);
        bottom=std::min(bottom,q.y); top=std::max(top,q.y);
    }
    if(right-left<=800 || top-bottom<=800) return false;
    // Require four distinct corners and four axis-aligned sides.
    for(size_t i=0;i<4;++i) {
        Point a=p.points[i], b=p.points[i+1];
        if ((a.x!=left && a.x!=right) || (a.y!=bottom && a.y!=top)) return false;
        if (Same(a,b) || (a.x!=b.x && a.y!=b.y)) return false;
        for(size_t j=0;j<i;++j) if(Same(a,p.points[j])) return false;
    }
    return true;
}
}

bool CorelConvertGuides(const std::string &input, std::string &output)
{
    output=input;
    if(input.size()>64*1024*1024 || input.find('\0')!=std::string::npos) return false;
    static const std::regex move("(PU|PD)(-?[0-9]{1,8})[ ,]+(-?[0-9]{1,8})");
    static const std::regex penCommand("SP([0-9]{1,3})");
    static const std::regex setting("(VS|PW)[0-9]+(\\.[0-9]+)?,[1-8]");
    std::vector<Path> paths;
    bool initialized=false, drawing=false;
    int pen=-1;
    size_t offset=0;
    while(offset<input.size()) {
        size_t end=input.find(';',offset);
        if(end==std::string::npos) {
            if (!Trim(input.substr(offset)).empty()) return false;
            break;
        }
        size_t first=input.find_first_not_of(" \t\r\n",offset);
        std::string command=Trim(input.substr(offset,end-offset));
        std::smatch match;
        if(!initialized) {
            if(command!="IN") return false;
            initialized=true;
        } else if(std::regex_match(command,match,move)) {
            if(pen<=0) return false;
            Point p={std::stoi(match[2]),std::stoi(match[3])};
            if(match[1]=="PU") {
                paths.push_back({first,end+1,pen,{p}});
                drawing=true;
            } else {
                if(!drawing || paths.empty()) return false;
                paths.back().points.push_back(p);
                paths.back().end=end+1;
            }
        } else if(std::regex_match(command,match,penCommand)) {
            pen=std::stoi(match[1]); drawing=false;
        } else if(command=="LT" || command=="WU0" || std::regex_match(command,setting)) {
            drawing=false;
        } else return false; // Reject scaling, relative moves, arcs, labels, metadata, etc.
        offset=end+1;
    }
    size_t border=paths.size();
    int left=0,bottom=0,right=0,top=0;
    for(size_t i=0;i<paths.size();++i) {
        int l,b,r,t;
        if(!Bounds(paths[i],l,b,r,t)) continue;
        bool inside=false, encloses=true;
        for(size_t j=0;j<paths.size();++j) {
            if(i==j || paths[j].points.size()<2) continue;
            inside=true;
            for(Point p:paths[j].points)
                if(p.x<=l+40 || p.x>=r-40 || p.y<=b+40 || p.y>=t-40) encloses=false;
        }
        if(!inside || !encloses) continue;
        if(border!=paths.size()) return false;
        border=i;left=l;bottom=b;right=r;top=t;
    }
    if(border==paths.size()) return false;
    const char *nl=input.find("\r\n")!=std::string::npos?"\r\n":"\n";
    std::ostringstream marks;
    const auto emit=[&](const char *op,int x,int y) {marks<<op<<x<<' '<<y<<';'<<nl;};
    // Same 400-unit L marks and 120-unit direction mark as the client's
    // final guias PLT. Retain source coordinates and pen for the vendor engine.
    // The fixture's long axis is X; use the equivalent rotated layout for Y.
    const auto point=[&](const char *op,int across,int along) {
        if(right-left>=top-bottom) emit(op,left+along,top-across);
        else emit(op,left+across,bottom+along);
    };
    int w=std::min(right-left,top-bottom), h=std::max(right-left,top-bottom);
    point("PU",0,640);point("PD",0,520);
    point("PU",w,h-400);point("PD",w,h);point("PD",w-400,h);
    point("PU",0,h-400);point("PD",0,h);point("PD",400,h);
    point("PU",0,400);point("PD",0,0);point("PD",400,0);
    point("PU",w,400);point("PD",w,0);point("PD",w-400,0);
    std::string replacement=marks.str();
    replacement.erase(replacement.size()-std::char_traits<char>::length(nl));
    output=input.substr(0,paths[border].start)+replacement+input.substr(paths[border].end);
    return true;
}
