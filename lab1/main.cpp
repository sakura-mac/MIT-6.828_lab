#include <stdlib.h>
#include <iostream>
using  std::cout;
// #include <iostream>
// using namespace std;

// template<class T>
// bool Isequal(T& p1, T& p2){
//   return p1 == p2;
// }

// class Parent{

//   public:
//   int b;
//   Parent():b(2){
//     cout << "father" << endl;
//   }
//    void echo(){
//     cout << "hello Parent" << endl;
//   }
//   void echofuck(){
//     cout << "fuck Parent" << endl;
//   }
// };
// class Toy{
//   public:
//   Toy(){
//     cout << "toy" << endl;;
//   }
// };
// class Child:public Parent{
//   Toy t;
  
//   public:
//   int a;
//   int vi[10];
//   Child():t(){
//     cout << "child" << endl;
//   }

//    void echo(){
//     cout << "hello child" << endl;
//   }

// };
// int* foo(){
//   static int a[] = {1,2,3};
//   return a;
// }
// 程序的主函数
bool lsearch(void* array, int *target, int n, int array_size){
    for(int i = 0 ; i < n; i++){
      void* addr = (char*)array + i*(array_size);
      if(memcmp(target,addr,array_size)==0)return true;
    }
    return false;
}
int main(int argc, char** argv){
  
  int ogawa[10]={1,2,3,4,5,6,7,8,9,10};
  int target = 7;
  cout << lsearch(&ogawa,&target,10,sizeof(int));
  return 0;

}

