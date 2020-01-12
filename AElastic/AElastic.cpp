//
//  AElastic.cpp
//  AElastic
//
//  Created by 熊斌 on 2020/1/7.
//  Copyright © 2020 熊斌. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include "../Utils/CRC.h"
using namespace std;


//******************//
//*****宏定义部分*****//
//******************//

//四个表的长度
#define HEAVY_PART_1_SIZE  8192
#define HEAVY_PART_2_SIZE  8192
#define HEAVY_PART_3_SIZE  4096
#define LIGHT_PART_SIZE  16384
//掩码
#define FINGERPRINT_MASK 0xFFFFFFFF
#define LIGHT_CNT_MASK 0xFF


//********************//
//*****全局变量部分*****//
//*******************//

//处理的数据包的数量
int n_pkts = 1000000;
//需要处理的 flowid 列表
string * pkt_list;
//三个 HEAVY 表
uint32_t ** heavy;
// light 表
uint8_t * light;
//记录换入换出的 flowid 对应在 pkt_list 的地址
string ** ids;
//记录下一个进入 ids 数组的索引
int id_count = 0;
//当前正在被处理的 flowid 对应的地址
string * flowid_now;
//(vote- / vote+) > thresh 时发生替换
uint32_t thresh = 8;
//统计 flowid 以及出现对应数据包数量
map<string,int> flows;


//********************//
//*****函数声明部分*****//
//*******************//

//读取 json 文件到 pkt_list
bool read_json_file(string filename);
//更新三个 HEAVY 表的函数
uint32_t * update(uint32_t* item, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);
//得到 flows 的值
void get_flows(CRC::Parameters<crcpp_uint32, 32> hash, CRC::Parameters<crcpp_uint32, 32> hash_next, uint8_t * l);




//main 函数
int main(){
    //读取 json 文件
    string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
    pkt_list = new string[n_pkts];
    if(!read_json_file(filename)){
        return 0;
    }
    //定义 5 个 hash 函数
    CRC::Parameters<crcpp_uint32, 32> hash1 = { 0x04C11DB7, 0, 0xFFFFFFFF, false, false };
    CRC::Parameters<crcpp_uint32, 32> hash2 = { 0x1EDC6F41, 0, 0xFFFFFFFF, false, false };
    CRC::Parameters<crcpp_uint32, 32> hash3 = { 0xA833982B, 0, 0xFFFFFFFF, false, false };
    CRC::Parameters<crcpp_uint32, 32> hash4 = { 0x814141AB, 0, 0xFFFFFFFF, false, false };
    CRC::Parameters<crcpp_uint32, 32> hash5 = { 0x5a0849e7, 0, 0xFFFFFFFF, false, false };
    
    //为 heavy 表 和 light 表动态分配空间并初始化值
    heavy = (uint32_t **) new uint32_t *[HEAVY_PART_1_SIZE+HEAVY_PART_2_SIZE+HEAVY_PART_3_SIZE];
    for(int i=0;i<(HEAVY_PART_1_SIZE+HEAVY_PART_2_SIZE+HEAVY_PART_3_SIZE);i++){
        heavy[i] = (uint32_t *) new uint32_t[4];
        heavy[i][0] = 0;
        heavy[i][1] = 0;
        heavy[i][2] = 0;
        heavy[i][3] = 0;
    }
    uint8_t * light = (uint8_t *) new uint8_t[LIGHT_PART_SIZE];
    for(int i=0;i<LIGHT_PART_SIZE;i++){
        light[i] = 0;
    }
    //为 ids 表分配空间（分配了极端情况的最大空间）
    ids = (string **)new string* [n_pkts];
    id_count = 0;
    //记录最终统计的流以及对应的估计次数
    map<string, int> flows;
    //开始对每一个数据包进行处理
    
    for(int i=0;i<n_pkts;i++){
        //记录正在处理的 flowid 对应的地址
        flowid_now = &pkt_list[i];
        //得到 flowid 对应的 fingerprint
        uint32_t fingerprint = CRC::Calculate(pkt_list[i].c_str(), pkt_list[i].length(), hash5) & FINGERPRINT_MASK;
        //对第一个 heavy 表进行处理
        uint32_t item[2] = {fingerprint,1};
        uint32_t * middle_item = update(item,hash1,0,HEAVY_PART_1_SIZE);
        middle_item = update(middle_item,hash2,HEAVY_PART_1_SIZE,HEAVY_PART_2_SIZE);
        middle_item = update(middle_item, hash3, HEAVY_PART_1_SIZE+HEAVY_PART_2_SIZE, HEAVY_PART_3_SIZE);
        if(middle_item != 0){
            //开始将 fingerprint 转换为 string 并计算对应表的位置索引 idx
            stringstream ss;
            ss << middle_item[0];
            string fingerprint_str = ss.str();
            uint32_t hash_value =CRC::Calculate(fingerprint_str.c_str(), fingerprint_str.length(), hash4);
            int idx = hash_value % LIGHT_PART_SIZE;
            light[idx] =(uint8_t) ((light[idx] + middle_item[1]) & LIGHT_CNT_MASK);
        }
    }
    get_flows(hash5, hash4,light);
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



//更新三个 HEAVY 表的函数
uint32_t * update(uint32_t* item, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod){
    if(item == 0){
        return 0;
    }
    //暂存 flowid 的 fingerprint 以及 count
    uint32_t fingerprint = item[0];
    uint32_t count = item[1];
    
    //开始将 fingerprint 转换为 string 并计算对应表的位置索引 idx
    stringstream ss;
    ss << fingerprint;
    string fingerprint_str = ss.str();
    uint32_t hash_value =CRC::Calculate(fingerprint_str.c_str(), fingerprint_str.length(), hash);
    int idx = base + hash_value % mod;

    uint32_t * res = 0;
    if((heavy[idx][0] == 0 && heavy[idx][1] == 0)&& (heavy[idx][2] == 0 && heavy[idx][3] == 0)){
        heavy[idx][0] = fingerprint;
        heavy[idx][1] = count;
        heavy[idx][2] = (uint32_t) 0;
        heavy[idx][3] = (uint32_t) 0;
        ids[id_count] = flowid_now;
        id_count ++;
    }
    else if(fingerprint == heavy[idx][0]){
        heavy[idx][1] += count;
    }
    else{
        heavy[idx][3] += count;
        if(heavy[idx][3] >= thresh * heavy[idx][1]){
            res = new uint32_t[2];
            res[0] = heavy[idx][0];
            res[1] = heavy[idx][1];
            heavy[idx][0] = fingerprint;
            heavy[idx][1] = count;
            heavy[idx][2] = 1;
            heavy[idx][3] = 0;
            ids[id_count] = flowid_now;
            id_count ++;
        }
        else{
            res = item;
        }
    }
    
    return res;
}

void get_flows(CRC::Parameters<crcpp_uint32, 32> hash, CRC::Parameters<crcpp_uint32, 32> hash_next, uint8_t *  l){
    map<uint32_t, uint32_t> tempflows;
    for(int i = 0; i < HEAVY_PART_1_SIZE + HEAVY_PART_2_SIZE + HEAVY_PART_3_SIZE; i++){
        if(!((heavy[i][0] == 0 && heavy[i][1] == 0)&& (heavy[i][2] == 0 && heavy[i][3] == 0))){
            if(tempflows.find(heavy[i][0]) == tempflows.end()){
                tempflows[heavy[i][0]] = heavy[i][1];
            }
            else{
                tempflows[heavy[i][0]] += heavy[i][1];
            }
        }
    }
    for(int i=0; i<id_count; i++){
        string flowid = *(ids[i]);
        uint32_t fingerprint = CRC::Calculate(flowid.c_str(), flowid.length(), hash) & FINGERPRINT_MASK;
        //开始将 fingerprint 转换为 string 并计算对应表的位置索引 idx
        stringstream ss;
        ss << fingerprint;
        string fingerprint_str = ss.str();
        uint32_t hash_value =CRC::Calculate(fingerprint_str.c_str(), fingerprint_str.length(), hash_next);
        int idx = hash_value % LIGHT_PART_SIZE;
        int count_num = l[idx];
        if(tempflows.find(fingerprint) != tempflows.end()){
            count_num = tempflows[fingerprint];
        }
        flows[flowid] = count_num;
    }
    /*
    for(map<string,int>::iterator it = flows.begin(); it!=flows.end(); ++it){
        cout<<it->first<<"\t"<<it->second<<endl;
    }*/
    cout<<flows.size()<<endl;
}
