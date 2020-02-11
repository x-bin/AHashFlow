//
//  main.cpp
//  SpaceSaving
//
//  Created by 熊斌 on 2020/1/13.
//  Copyright © 2020 熊斌. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <string>
#include "../Utils/CRC.h"
#include "space-saving.hpp"
using namespace std;


#define TABLE_SIZE 16384


int n_pkts = 1000000;
//需要处理的 flowid 列表
string * pkt_list;


//读取 json 文件到 pkt_list
bool read_json_file(string filename);


int main(int argc, const char * argv[]) {
    //读取 json 文件
    string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
    pkt_list = new string[n_pkts];
    if(!read_json_file(filename)){
        return 0;
    }
    cout<<"read success!"<<endl;
    SpaceSaving * space_saving = new SpaceSaving(TABLE_SIZE);
    for(int i = 0; i < n_pkts; i++){
        space_saving -> Process(pkt_list[i]);
    }
}



//读取 json 文件到 pkt_list
bool read_json_file(string filename){
    ifstream file(filename);
    if(!file.is_open()){
        cout<<"Can't open the file!"<<endl;
        return false;
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
            if(c=='"'){
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
            if(c=='"'){
                flag_end = true;
            }
            else if (c == 't' || c == '\\'){
                if(c=='t'){
                    pkt_list[count].append(1,'\t');
                }
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
    /*
    for(int i = 0; i < n_pkts; i++){
        cout<<pkt_list[i]<<endl;
        cout<<pkt_list[i].length()<<endl;
    }*/
    return true;
}
/*
void process(string flowid){
    uint32_t fingerprint = CRC::Calculate(flowid.c_str(), flowid.length(), hash1);
    int index = fingerprint % TABLE_SIZE;
    int start = index;
    while (true) {
        if(flowid_key[index] == "0"){
            flowid_key[index] = flowid;
            cnt[index] = 1;
            if(min_value != 1){
                sub_min_value = min_value;
                for(int j = 0; j < min_num; j++){
                    sub_min_idx[j] = min_idx[j];
                }
                sub_min_num = min_num;
                min_value = 1;
                min_idx[0] = index;
                min_num = 1;
            }
            else{
                min_idx[min_num] = index;
                min_num ++;
            }
            break;
        }
        else if(flowid_key[index] == flowid){
            if(cnt[index] == min_value){
                if(min_num == 1){
                    min_value +=1;
                    if(min_value == sub_min_value){
                        
                    }
                }
                else{
                    
                }
            }
            else if(cnt[index] == sub_min_value){
                
            }
            cnt[index] += 1;
            break;
        }
        index = (index == TABLE_SIZE - 1) ? 0 : index + 1;
        if(index == start){
            full_table = true;
            break;
        }
    }
    return;
}*/
