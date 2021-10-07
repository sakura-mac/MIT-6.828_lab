#include <stdlib.h>
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
int main(int argc, char** argv){
  int a[3][4]={};
  a[1][1]=2;
  //find a[1][1]:1*4+1
  printf("%d %d %d %d",**(a+1),&a[1],a+1,*(&a[1][0]+1));
  // cout << a << " " << a+1 << endl; 
  // cout << &a[1] <<" "<< a[1] << endl;
  // cout <<*(a+1) << " "<< a[1] <<endl;
  // cout <<*(a+5) << " " << a+5 << endl;
  return 0;

}

