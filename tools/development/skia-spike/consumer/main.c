#include <evaluation.h>
#include <stdio.h>
int main(int argc,char** argv) {
    if(argc!=2 || skia_evaluation_version()!=1)return 1;
    if(skia_evaluation_render(argv[1],144)){fprintf(stderr,"%s\n",skia_evaluation_error());return 1;}
    puts("Installed C consumer passed.");return 0;
}
