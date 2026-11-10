#include "PltGuides.h"
#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>
#include <utility>
#include <vector>

namespace {
const double tolerance = 40;
const double arm = 400;
struct Point { double x, y; };
struct Path { size_t start, end; std::vector<Point> points; };
bool Edge(Point p, double w, double h)
{
    return p.x >= -tolerance && p.x <= w+tolerance &&
           p.y >= -tolerance && p.y <= h+tolerance &&
           std::min(std::min(std::abs(p.x), std::abs(p.x-w)),
                    std::min(std::abs(p.y), std::abs(p.y-h))) <= tolerance;
}
bool IsFrame(const Path &path, double w, double h)
{
    const auto &p = path.points;
    if (p.size() < 5 || std::abs(p.front().x-p.back().x) > tolerance ||
        std::abs(p.front().y-p.back().y) > tolerance)
        return false;
    std::vector<std::pair<double, double> > sides[4];
    for (size_t i=0; i<p.size(); ++i) {
        if (!Edge(p[i],w,h)) return false;
        if (!i) continue;
        Point a=p[i-1], b=p[i];
        if (!Edge({(a.x+b.x)/2, (a.y+b.y)/2},w,h)) return false;
        for (int j=0; j<2; ++j) {
            double x=j?w:0, y=j?h:0;
            if (std::abs(a.x-x)<=tolerance && std::abs(b.x-x)<=tolerance)
                sides[j].push_back(std::minmax(a.y,b.y));
            if (std::abs(a.y-y)<=tolerance && std::abs(b.y-y)<=tolerance)
                sides[j+2].push_back(std::minmax(a.x,b.x));
        }
    }
    for (int j=0; j<4; ++j) {
        std::sort(sides[j].begin(),sides[j].end());
        double reach=tolerance;
        for (const auto &interval:sides[j]) {
            if (interval.first>reach+tolerance) return false;
            reach=std::max(reach,interval.second);
        }
        if (reach < (j<2?h:w)-tolerance) return false;
    }
    return true;
}
}

bool PltConvertGuides(const std::string &input, std::string &output)
{
    if (CorelConvertGuides(input, output)) return true;
    output=input;
    // Bound parsing work and reject binary or appended metadata conservatively.
    if (input.size()>64*1024*1024 || input.find('\0')!=std::string::npos) return false;
    std::istringstream stream(input);
    std::vector<std::string> tokens;
    for (std::string t; stream>>t;) tokens.push_back(t);
    if (tokens.size()<4 || tokens[0]!="IN" || tokens[2]!="CT1") return false;
    std::smatch match;
    static const std::regex frame("TB26,([0-9]{1,9}),([0-9]{1,9})");
    static const std::regex move("([UD])(-?[0-9]{1,9}),(-?[0-9]{1,9})");
    if (!std::regex_match(tokens[1],match,frame)) return false;
    int w=std::stoi(match[1]), h=std::stoi(match[2]);
    if (std::min(w,h)<=2*arm) return false;
    std::vector<Path> paths;
    bool ended=false;
    for (size_t i=3;i<tokens.size();++i) {
        if (tokens[i]=="@") { ended=true; continue; }
        if (ended || !std::regex_match(tokens[i],match,move)) return false;
        Point p={double(std::stoi(match[2])),double(std::stoi(match[3]))};
        if (match[1]=="U") paths.push_back({i,i+1,{p}});
        else {
            if (paths.empty()) return false;
            paths.back().end=i+1;
            paths.back().points.push_back(p);
        }
    }
    if (!ended) return false;
    size_t border=paths.size();
    for (size_t i=0;i<paths.size();++i) {
        if (!IsFrame(paths[i],w,h)) continue;
        if (border!=paths.size()) return false;
        border=i;
    }
    if (border==paths.size()) return false;
    bool inside=false;
    for (size_t i=0;i<paths.size();++i) {
        if (i==border) continue;
        for (Point p:paths[i].points) {
            if (paths[i].points.size()>1) {
                if (p.x<0 || p.x>w || p.y<0 || p.y>h) return false;
                inside |= p.x>tolerance && p.x<w-tolerance &&
                          p.y>tolerance && p.y<h-tolerance;
            }
            if (p.x==0 && p.y>=0 && p.y<=tolerance) continue;
            if ((std::min(p.x,w-p.x)<=tolerance && std::min(p.y,h-p.y)<=arm) ||
                (std::min(p.y,h-p.y)<=tolerance && std::min(p.x,w-p.x)<=arm))
                return false;
        }
    }
    if (!inside) return false;
    std::ostringstream guides;
    guides << "U0,640 D0,520 U" << w << ',' << h-400 << " D" << w << ',' << h
           << " D" << w-400 << ',' << h << " U0," << h-400 << " D0," << h
           << " D400," << h << " U0,400 D0,0 D400,0 U" << w << ",400 D"
           << w << ",0 D" << w-400 << ",0";
    std::ostringstream result;
    result << " IN ";
    for (size_t i=3;i<tokens.size();++i) {
        if (i==paths[border].start) {
            result<<guides.str()<<' ';
            i=paths[border].end-1;
        } else if (tokens[i]!="@" || i==tokens.size()-1) result<<tokens[i]<<' ';
    }
    output=result.str();
    return true;
}
