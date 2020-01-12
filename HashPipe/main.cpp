//
//  main.cpp
//  HashPipe
//
//  Created by 熊斌 on 2020/1/12.
//  Copyright © 2020 熊斌. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <string>
#include "../Utils/CRC.h"
using namespace std;


typedef struct {
    string flowid;
    int count;
}item;


#define TABLE1_SIZE 4096
#define TABLE2_SIZE 4096
#define TABLE3_SIZE 4096
#define TABLE4_SIZE 4096
#define TABLE_NUM 4


int n_pkts = 1000000;
//需要处理的 flowid 列表
string * pkt_list;
item * table;







//读取 json 文件到 pkt_list
bool read_json_file(string filename);
//更新三个子表的函数
item * update(item middle_item, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);


int main(int argc, const char * argv[]) {
    //读取 json 文件
    string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
    pkt_list = new string[n_pkts];
    if(!read_json_file(filename)){
        return 0;
    }
    table = new item[TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE+TABLE4_SIZE];
    for(int i = 0; i < TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE+TABLE4_SIZE; i++){
        table[i].flowid = "0";
        table[i].count = 0;
    }
    //定义 4 个 hash 函数
    CRC::Parameters<crcpp_uint32, 32> hash1 = { 0x04C11DB7, 0, 0xFFFFFFFF, false, false };
    CRC::Parameters<crcpp_uint32, 32> hash2 = { 0x1EDC6F41, 0, 0xFFFFFFFF, false, false };
    CRC::Parameters<crcpp_uint32, 32> hash3 = { 0xA833982B, 0, 0xFFFFFFFF, false, false };
    CRC::Parameters<crcpp_uint32, 32> hash4 = { 0x814141AB, 0, 0xFFFFFFFF, false, false };
    
    for(int i = 0; i < n_pkts; i++){
        string flowid = pkt_list[i];
        uint32_t fingerprint = CRC::Calculate(flowid.c_str(), flowid.length(), hash1);
        bool stage1_succeed = true;
        item stage1_item;
        int idx1 = fingerprint % TABLE1_SIZE;
        
        if(table[idx1].flowid == "0" && table[idx1].count == 0){
            table[idx1].flowid = flowid;
            table[idx1].count = 1;
            //cout<<flowid<<"  1  yes"<<endl;
        }
        else if(table[idx1].flowid == flowid){
            table[idx1].count +=1;
           // cout<<flowid<<"  "<<table[idx1].count<<"  plus"<<endl;
        }
        else{
            stage1_succeed = false;
            stage1_item.flowid = table[idx1].flowid;
            stage1_item.count = table[idx1].count;
            table[idx1].flowid = flowid;
            table[idx1].count = 1;
            //cout<< flowid <<" changed "<<table[idx1].flowid<<" with count = "<<stage1_item.count<<endl;
        }
        
        if(!stage1_succeed){
            item * middle_item = update(stage1_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
            if(middle_item != 0){
                //cout<<"After 2: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                middle_item = update(*middle_item, hash3, TABLE1_SIZE + TABLE2_SIZE, TABLE3_SIZE);
                if(middle_item != 0){
                    //cout<<"After 3: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                    middle_item = update(*middle_item, hash4, TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE, TABLE4_SIZE);
                }
            }
        }
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

item * update(item middle_item, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod){
    item * res = 0;
    string flowid = middle_item.flowid;
    int count = middle_item.count;
    uint32_t fingerprint = CRC::Calculate(flowid.c_str(), flowid.length(), hash);
    bool update_succeed = true;
    int idx = fingerprint % mod + base;
    
    if(table[idx].flowid == "0" && table[idx].count == 0){
        table[idx].flowid = flowid;
        table[idx].count = count;
    }
    else if(table[idx].flowid == flowid){
        table[idx].count += count;
    }
    else{
        update_succeed = false;
        res = new item;
        if(table[idx].count < count){
            res->flowid = table[idx].flowid;
            res->count = table[idx].count;
            table[idx].flowid = flowid;
            table[idx].count = count;
        }
        else{
            res->flowid = flowid;
            res->count = count;
        }
    }
    return res;
}
