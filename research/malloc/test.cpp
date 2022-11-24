#ifdef WIN32
#include <QCoreApplication>
#include <windows.h>
#else
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include <cstring>
#include <vector>
using std::vector;
int main(int argc, char *argv[])
{

#ifdef WIN32
    QCoreApplication a(argc, argv);
#endif
    vector<char*> char_vec;
    for (int i = 0; i < 100; i++) {
       char* a = (char*)malloc(1024*1024*1);
       memset(a, '0', 1024*1024*1);
       printf("malloc once\n");
#ifdef WIN32
       Sleep(1000);
#else
       sleep(1);
#endif
       char_vec.push_back(a);

    }
    for (size_t i = 0; i < char_vec.size(); i++) {
#ifdef WIN32
        Sleep(1000);
#else
       sleep(1);
#endif
        printf("sleep 1s\n");
        free(char_vec[i]);
    }
#ifdef WIN32
    return a.exec();
#endif
    return 0;
}
