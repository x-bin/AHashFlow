//
//  main.cpp
//  hashSketch
//
//  Created by 熊斌 on 2020/1/9.
//  Copyright © 2020 熊斌. All rights reserved.
//
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <math.h>
using namespace std;
int main(){
    string filename =  "/Users/xiongbin/CAIDA/HGC.20080415000.dict.json";
    int n_pkts = 100;
    string * pkt_list = new string[n_pkts];
   ifstream file(filename);
   if(!file.is_open()){
       cout<<"Can't open the file!"<<endl;
   }
   int count = 0;
   while(count < n_pkts && !file.eof()){
       char c;
       bool flag_begin = false;
       bool flag_end = false;
       
       //找到每一个 flowid 开始的地方
       //并设置 flag_begin 为 true
       while(!flag_begin){
           file >> c;
           if(file.eof())
               break;
           if(c=='{'){
               flag_begin = true;
           }
       }
       if(file.eof())
       break;
       
       //将 flowid 保存到 pkt_list[count] 中
       while(!flag_end){
           file >> c;
           if(file.eof())
               break;
           if(c=='}'){
               flag_end = true;
           }
           else{
               pkt_list[count].append(1, c);
           }
       }
       //如果添加成功
       if(pkt_list[count].length()!=0){
           count ++;
       }
       if(file.eof())
       break;
   }
   if(count!=n_pkts){
       cout<<"File "<<filename<<" don't have "<<n_pkts<<" packets!"<<endl;
       n_pkts = count;
   }
   file.close();
   
   //test
   
   for(int i = 0; i < n_pkts; i++){
       cout<<pkt_list[i]<<endl;
       int dstip_pos = (int)pkt_list[i].find("\"dstip\":\"");
       int proto_pos = (int)pkt_list[i].find("\",\"proto\":\"");
       int time_pos = (int)pkt_list[i].find("\",\"timestamp\":\"");
       int srcport_pos = (int)pkt_list[i].find("\",\"srcport\":\"");
       int srcip_pos = (int)pkt_list[i].find("\",\"srcip\":\"");
       int dstport_pos = (int)pkt_list[i].find("\",\"dstport\":\"");
       
       string dstip = pkt_list[i].substr(dstip_pos + 9, proto_pos - dstip_pos - 9);
       string proto = pkt_list[i].substr(proto_pos + 11, time_pos - proto_pos - 11);
       string srcport = pkt_list[i].substr(srcport_pos + 13, srcip_pos - srcport_pos - 13);
       string srcip = pkt_list[i].substr(srcip_pos + 11, dstport_pos - srcip_pos - 11);
       string dstport = pkt_list[i].substr(dstport_pos + 13, pkt_list[i].length() - dstport_pos - 14);
       pkt_list[i] = srcip + '\t' + dstip + '\t' + proto + '\t' + srcport + '\t' + dstport;
       cout<<pkt_list[i]<<endl;
       cout<<pkt_list[i].length()<<endl;
   }
}

