#include <evaluation.h>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2 || skia_evaluation_version()!=1)return 1;
    if(skia_evaluation_render(argv[1],192)){std::cerr<<skia_evaluation_error()<<'\n';return 1;}
    std::cout<<"Installed C++ consumer passed.\n";return 0;
}
