#include <cstdio>
#include <iostream>
using namespace std;

template<class T>
bool Isequal(T& p1, T& p2){
  return p1 == p2;
}

class Parent{

  public:
  int b;
  Parent():b(2){
    cout << "father" << endl;
  }
   void echo(){
    cout << "hello Parent" << endl;
  }
  void echofuck(){
    cout << "fuck Parent" << endl;
  }
};
class Toy{
  public:
  Toy(){
    cout << "toy" << endl;;
  }
};
class Child:public Parent{
  Toy t;
  
  public:
  int a;
  int vi[10];
  Child():t(){
    cout << "child" << endl;
  }

   void echo(){
    cout << "hello child" << endl;
  }

};
int* foo(){
  static int a[] = {1,2,3};
  return a;
}
// 程序的主函数
int main(int argc, char** argv){
  int a[10];
  int b[10] = {};
  for(int i: a){
    cout << i;
  }
  cout << endl;
  for(int i : b){
    cout << i;
  }
  return 0;
}

