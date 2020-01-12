//
//  main.cpp
//  hashSketch
//
//  Created by 熊斌 on 2020/1/9.
//  Copyright © 2020 熊斌. All rights reserved.
//
#include <iostream>
#include <string>
#include <map>
#include <math.h>
using namespace std;
int main(){
   /* map<string, int> mymap;
    mymap["a"] = 1;
    mymap["b"] = 2;
    mymap["a"] = 3;
    for(map<string,int>::iterator it=mymap.begin();it != mymap.end(); it ++){
        cout<<it->first<<it->second<<endl;
    }
    map<string, int> mymap2;
    string a = "a";
    string b = "b";
    string c = "a";
    mymap2[a] = 1;
    mymap2[b] = 2;
    mymap2[c] = 3;
    for(map<string,int>::iterator it=mymap2.begin();it != mymap2.end(); it ++){
        cout<<it->first<<it->second<<endl;
    }
    string * ptr = &a;
    cout<<ptr<<endl;
    cout<<&a<<endl;
    int x = (*ptr == a);
    cout<<x<<endl;
    if(mymap.find("a") == mymap.end()){
        cout<<"It doesn't exist!"<<endl;
    }*/
    int y = (int) pow(2,ceil(log(23)/log(2))) ;
    for (int i = 0; i < 100; i++){
        cout<<rand()%y<<endl;
    }
    cout<<y<<endl;
}

