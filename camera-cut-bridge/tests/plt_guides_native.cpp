#include "PltGuides.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <regex>
#include <sstream>
std::string Read(const std::string &path) {
    std::ifstream f(path.c_str(),std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open fixture: "+path);
    return std::string(std::istreambuf_iterator<char>(f),{});
}
int main(int argc,char **argv) {
    if (argc!=2) return 2;
    std::string dir=argv[1], original=Read(dir+"/corteLa.plt"), expected=Read(dir+"/corteLaGuias.plt"), out;
    if (!PltConvertGuides(original,out) || out!=expected) return 1;
    if (PltConvertGuides(expected,out) || out!=expected) return 1;
    std::string malformed=original;
    malformed.replace(malformed.find("D4813,40800"),12,"D2400,20400");
    if (PltConvertGuides(malformed,out) || out!=malformed) return 1;
    std::string other="IN;PU0,0;PD100,100;";
    if (PltConvertGuides(other,out) || out!=other) return 1;
    size_t start=original.find("U0,20400"), end=original.find("U402,38399");
    std::string duplicate=original.substr(0,end)+original.substr(start,end-start)+original.substr(end);
    if (PltConvertGuides(duplicate,out) || out!=duplicate) return 1;
    std::string job=Read(dir+"/skycutexport.job");
    if (!PltConvertGuides(job,out)) {
        std::cerr<<"Corel HPGL rectangle was not converted\n";
        return 1;
    }
    if (out.substr(0,out.find("PU"))!=job.substr(0,job.find("PU"))) return 1;
    size_t artwork=job.find("LT;\r\nSP5;");
    if (out.substr(out.find("LT;\r\nSP5;"))!=job.substr(artwork)) return 1;
    if (out.find("PD20399 2399;\r\nPD20399 -2401;")!=std::string::npos) return 1;
    if (out.find("PU-19761 2399;\r\nPD-19881 2399;")==std::string::npos) return 1;
    // Rotate/translate Corel coordinates into the final cutter coordinate frame.
    // All replacement marks must match the user's actual manual PLT geometry.
    std::string marks=out.substr(out.find("PU"),out.find("LT;\r\nSP5;")-out.find("PU"));
    std::regex hpglMove("P([UD])(-?[0-9]+) (-?[0-9]+);");
    std::ostringstream normalized;
    for(std::sregex_iterator it(marks.begin(),marks.end(),hpglMove),end;it!=end;++it) {
        normalized<<(*it)[1].str()<<2399-std::stoi((*it)[3])<<','
                  <<std::stoi((*it)[2])+20401<<' ';
    }
    size_t firstMark=expected.find("U0,640"), firstCut=expected.find("U402,38399");
    if(normalized.str()!=expected.substr(firstMark,firstCut-firstMark)) return 1;
    std::string guided=out;
    if (PltConvertGuides(guided,out) || out!=guided) return 1;
    std::string escaped=job;
    escaped.insert(escaped.find("PU-20401"),"PR;\r\n");
    if (PltConvertGuides(escaped,out) || out!=escaped) return 1;
    std::string notFrame=job;
    notFrame.replace(notFrame.find("PD20399 2399"),12,"PD10000 1000");
    if (PltConvertGuides(notFrame,out) || out!=notFrame) return 1;
    std::string outside=job;
    outside.replace(outside.find("PU17998 1997"),12,"PU30000 1997");
    if (PltConvertGuides(outside,out) || out!=outside) return 1;
    std::string isolated=job.substr(0,job.find("LT;\r\nSP5;"))+"SP0;";
    if (PltConvertGuides(isolated,out) || out!=isolated) return 1;
    std::cout<<"Native converter: Corel HPGL and preservation, existing guides, relative moves, malformed/outside/isolated frames;  manual fixture, existing guides, malformed, unsupported, ambiguous passed.\n";
}
