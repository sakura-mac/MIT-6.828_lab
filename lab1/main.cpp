#include <cstdio>
#include <algorithm>
using namespace std;
 void print(int*, int*);

int main(int argc, char const *argv[])
{
    /* code */

    int a = 1;
    int b  = a;
    a = 2;
    printf("%d,%d",a,b);
    return 0;
}

