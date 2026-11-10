#include "PltGuides.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
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
    std::cout<<"Native converter: manual fixture, existing guides, malformed, unsupported, ambiguous passed.\n";
}
