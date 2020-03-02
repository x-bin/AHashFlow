//
//  AElastic.cpp
//  AElastic
//
//  Created by 熊斌 on 2020/1/7.
//  Copyright © 2020 熊斌. All rights reserved.
//
#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sched.h>
#include<unistd.h>
#include<pthread.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <time.h>
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

typedef struct {
    uint32_t fingerprint;
    string fingerprint_str;
    int count;
}mid_val;



//处理的数据包的数量
int n_pkts = 20000000;

int count1 = 0;
//需要处理的 flowid 列表
string * pkt_list;
//三个 HEAVY 表
uint32_t ** heavy;
// light 表
uint8_t * light;
//(vote- / vote+) > thresh 时发生替换
uint32_t thresh = 8;
//统计 flowid 以及出现对应数据包数量
map<string,int> flows;

long long fit_times = 0;

//********************//
//*****函数声明部分*****//
//*******************//

//读取 json 文件到 pkt_list
bool read_json_file(string filename);
bool read_hgc_file(string filename);
//更新三个 HEAVY 表的函数
mid_val * update(mid_val item, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);
//得到 flows 的值
void get_flows(CRC::Parameters<crcpp_uint32, 32> hash, CRC::Parameters<crcpp_uint32, 32> hash_next, uint8_t * l);




//main 函数
int main(){
    
    for(int index = 1; index < 11; index ++){
        fit_times = 0;
        //读取 json 文件
        count1 = 0;
        string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
        //filename = "/Users/xiongbin/CAIDA/HGC.20080415000.dict.json";
        
        if(index == 1){
            filename = "/Users/xiongbin/CAIDA/trace1.json";
        }
        else{
            stringstream s1;
            s1 << index;
            string temp_str = s1.str();
            filename = "/Users/xiongbin/CAIDA/trace" + temp_str;
        }
        cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;
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
        //记录最终统计的流以及对应的估计次数
        map<string, int> flows;
        //开始对每一个数据包进行处理
        clock_t st_time = clock();
        for(int i=0;i<n_pkts;i++){
            //得到 flowid 对应的 fingerprint
            uint32_t fingerprint = CRC::Calculate(pkt_list[i].c_str(), pkt_list[i].length(), hash5) & FINGERPRINT_MASK;
            stringstream ss;
            ss << fingerprint;
            string fingerprint_str = ss.str();

            //对第一个 heavy 表进行处理
            mid_val first_item;
            first_item.fingerprint = fingerprint;
            first_item.fingerprint_str = fingerprint_str;
            first_item.count = 1;
            fit_times += 1;
            mid_val * middle_item = update(first_item,hash1,0,HEAVY_PART_1_SIZE);
            if(middle_item != 0){
                fit_times += 1;
                middle_item = update(*middle_item,hash2,HEAVY_PART_1_SIZE,HEAVY_PART_2_SIZE);
                if(middle_item != 0){
                    fit_times += 1;
                    middle_item = update(*middle_item, hash3, HEAVY_PART_1_SIZE+HEAVY_PART_2_SIZE, HEAVY_PART_3_SIZE);
                    if(middle_item != 0){
                        fit_times += 1;
                        uint32_t hash_value =CRC::Calculate(middle_item->fingerprint_str.c_str(), middle_item->fingerprint_str.length(), hash4);
                        int idx = hash_value % LIGHT_PART_SIZE;
                        light[idx] =(uint8_t) ((light[idx] + middle_item->count) & LIGHT_CNT_MASK);
                    }
                }
            }
        }
        clock_t end_time = clock();
        double real_time = ((double)(end_time - st_time)) / CLOCKS_PER_SEC;
        cout<<"程序运行时间为 "<<real_time<<" 秒。"<<endl;
        cout<<"总计匹配次数： "<<fit_times<<"  平均匹配次数： "<<((double)fit_times) / ((double)n_pkts)<<endl;
        delete [] pkt_list;
        for(int i=0;i<(HEAVY_PART_1_SIZE+HEAVY_PART_2_SIZE+HEAVY_PART_3_SIZE);i++){
            delete [] heavy[i];
        }
        delete [] heavy;
        delete [] light;
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

//读取 json 文件到 pkt_list
bool read_hgc_file(string filename){
    ifstream file(filename);
    if(!file.is_open()){
        cout<<"Can't open the file!"<<endl;
        return false;
    }
    while(count1 < n_pkts && !file.eof()){
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
                pkt_list[count1].append(1, c);
            }
        }
        //如果添加成功
        if(pkt_list[count1].length()!=0){
            count1 ++;
        }
        if(file.eof())
        break;
    }
    file.close();
    if(count1 < n_pkts){
        cout<<"File "<<filename<<" don't have "<<n_pkts<<" packets!"<<endl;
        filename = "/Users/xiongbin/CAIDA/HGC.20080415001.dict.json";
        ifstream file1(filename);
        if(!file1.is_open()){
            cout<<"Can't open the file!"<<endl;
            return false;
        }
        while(count1 < n_pkts && !file1.eof()){
            char c;
            bool flag_begin = false;
            bool flag_end = false;
            
            //找到每一个 flowid 开始的地方
            //并设置 flag_begin 为 true
            while(!flag_begin){
                file1 >> c;
                if(file1.eof())
                    break;
                if(c=='{'){
                    flag_begin = true;
                }
            }
            if(file1.eof())
            break;
            
            //将 flowid 保存到 pkt_list[count] 中
            while(!flag_end){
                file1 >> c;
                if(file1.eof())
                    break;
                if(c=='}'){
                    flag_end = true;
                }
                else{
                    pkt_list[count1].append(1, c);
                }
            }
            //如果添加成功
            if(pkt_list[count1].length()!=0){
                count1 ++;
            }
            if(file1.eof())
            break;
        }
        file1.close();
    }
    if(count1 < n_pkts){
        cout<<"File "<<filename<<" don't have "<<n_pkts<<" packets!"<<endl;
        n_pkts = count1;
    }
    
    //test
    
    for(int i = 0; i < n_pkts; i++){
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
    }
    cout<<n_pkts<<endl;
    return true;
}

//更新三个 HEAVY 表的函数
mid_val * update(mid_val item, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod){
    //暂存 flowid 的 fingerprint 以及 count
    uint32_t fingerprint = item.fingerprint;
    uint32_t count = item.count;
    
    uint32_t hash_value =CRC::Calculate(item.fingerprint_str.c_str(), item.fingerprint_str.length(), hash);
    int idx = base + hash_value % mod;

    mid_val * res = 0;
    if((heavy[idx][0] == 0 && heavy[idx][1] == 0)&& (heavy[idx][2] == 0 && heavy[idx][3] == 0)){
        heavy[idx][0] = fingerprint;
        heavy[idx][1] = count;
        heavy[idx][2] = (uint32_t) 0;
        heavy[idx][3] = (uint32_t) 0;
    }
    else if(fingerprint == heavy[idx][0]){
        heavy[idx][1] += count;
    }
    else{
        heavy[idx][3] += count;
        if(heavy[idx][3] >= thresh * heavy[idx][1]){
            res = new mid_val;
            res->fingerprint = heavy[idx][0];
            res->count = heavy[idx][1];
            stringstream ss;
            ss << res->fingerprint;
            res->fingerprint_str = ss.str();

            heavy[idx][0] = fingerprint;
            heavy[idx][1] = count;
            heavy[idx][2] = 1;
            heavy[idx][3] = 0;
        }
        else{
            res = &item;
        }
    }
    
    return res;
}
/*
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
    
    for(map<string,int>::iterator it = flows.begin(); it!=flows.end(); ++it){
        cout<<it->first<<"\t"<<it->second<<endl;
    }
    cout<<flows.size()<<endl;
}
*/
