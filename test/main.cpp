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

bool new_read_json_file(string filename){
    ifstream file(filename);
    if(!file.is_open()){
        cout<<"Can't open the file!"<<endl;
        return false;
    }
    int count = 0;
    int len = 1000;
    char * temp = new char[len];
    while (file.getline(temp, len) && count < n_pkts) {
        string temp_str = temp;
        if(temp_str.length() != 0){
            pkt_list[count] = temp_str;
            count ++;
        }
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


//main 函数
int main(){
    ofstream out("/Users/xiongbin/Desktop/HashPipe.txt");
    if(out.fail()){
        cout<<"error"<<endl;
        return 0;
    }
    for(int index = 1; index < 11; index ++){
        fit_times = 0;
        //读取 json 文件
        count1 = 0;
        string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
        //filename = "/Users/xiongbin/CAIDA/HGC.20080415000.dict.json";
        
        if(index == 0){
            filename = "/Users/xiongbin/CAIDA/trace3.json";
        }
        else{
            stringstream s1;
            s1 << index;
            string temp_str = s1.str();
            filename = "/Users/xiongbin/newtrace/trace" + temp_str;
        }
        cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;
        pkt_list = new string[n_pkts];
        if(!new_read_json_file(filename)){
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
        cout<<"吞吐率： "<<((double)n_pkts) / (real_time)<<endl;
        cout<<"总计匹配次数： "<<fit_times<<"  平均匹配次数： "<<((double)fit_times) / ((double)n_pkts)<<endl;
        out<<filename<<"\t"<<real_time<<"\t"<<((double)n_pkts) / (real_time)<<"\t"<<fit_times<<"\t"<<((double)fit_times) / ((double)n_pkts)<<endl;
        delete [] pkt_list;
        for(int i=0;i<(HEAVY_PART_1_SIZE+HEAVY_PART_2_SIZE+HEAVY_PART_3_SIZE);i++){
            delete [] heavy[i];
        }
        delete [] heavy;
        delete [] light;
    }
    out.close();
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
#include <sstream>
#include <time.h>
#include "../Utils/CRC.h"
using namespace std;


typedef struct {
    uint32_t fingerprint;
    int count;
}item;

typedef struct {
    uint32_t fingerprint;
    string fingerprint_str;
    int count;
}mid_val;


#define TABLE1_SIZE 8192
#define TABLE2_SIZE 8192
#define TABLE3_SIZE 8192
#define TABLE4_SIZE 8192
#define TABLE_NUM 4


int n_pkts = 20000000;
//需要处理的 flowid 列表
string * pkt_list;
item * table;
int count1 = 0;

long long fit_times = 0;




//读取 json 文件到 pkt_list
bool read_json_file(string filename);
bool read_hgc_file(string filename);
//更新三个子表的函数
mid_val * update(mid_val middle_item, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);


bool new_read_json_file(string filename){
    ifstream file(filename);
    if(!file.is_open()){
        cout<<"Can't open the file!"<<endl;
        return false;
    }
    int count = 0;
    int len = 1000;
    char * temp = new char[len];
    while (file.getline(temp, len) && count < n_pkts) {
        string temp_str = temp;
        if(temp_str.length() != 0){
            pkt_list[count] = temp_str;
            count ++;
        }
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

int main(int argc, const char * argv[]) {
    ofstream out("/Users/xiongbin/Desktop/HashPipe.txt");
    if(out.fail()){
        cout<<"error"<<endl;
        return 0;
    }
    for(int index = 1; index < 11; index ++){
        fit_times = 0;
        count1 = 0;
        //读取 json 文件
        string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
        //filename = "/Users/xiongbin/CAIDA/HGC.20080415000.dict.json";
        
        if(index == 0){
            filename = "/Users/xiongbin/CAIDA/trace3.json";
        }
        else if (index != 0){
            stringstream s1;
            s1 << index;
            string temp_str = s1.str();
            filename = "/Users/xiongbin/newtrace/trace" + temp_str;
        }
        cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;
        pkt_list = new string[n_pkts];
        if(!new_read_json_file(filename)){
            return 0;
        }
        table = new item[TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE+TABLE4_SIZE];
        for(int i = 0; i < TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE+TABLE4_SIZE; i++){
            table[i].fingerprint = 0;
            table[i].count = 0;
        }
        //定义 5 个 hash 函数
        CRC::Parameters<crcpp_uint32, 32> hash1 = { 0x04C11DB7, 0, 0xFFFFFFFF, false, false };
        CRC::Parameters<crcpp_uint32, 32> hash2 = { 0x1EDC6F41, 0, 0xFFFFFFFF, false, false };
        CRC::Parameters<crcpp_uint32, 32> hash3 = { 0xA833982B, 0, 0xFFFFFFFF, false, false };
        CRC::Parameters<crcpp_uint32, 32> hash4 = { 0x814141AB, 0, 0xFFFFFFFF, false, false };
        CRC::Parameters<crcpp_uint32, 32> hash5 = { 0x5A0849E7, 0, 0xFFFFFFFF, false, false };
        
        clock_t st_time = clock();

        for(int i = 0; i < n_pkts; i++){
            string flowid = pkt_list[i];
            uint32_t fingerprint = CRC::Calculate(flowid.c_str(), flowid.length(), hash5);
            
            stringstream ss;
            ss << fingerprint;
            string fingerprint_str = ss.str();
            uint32_t hash_value =CRC::Calculate(fingerprint_str.c_str(), fingerprint_str.length(), hash1);
            int idx1 = hash_value % TABLE1_SIZE;
            fit_times += 1;
            if(table[idx1].fingerprint == 0 && table[idx1].count == 0){
                table[idx1].fingerprint = fingerprint;
                table[idx1].count = 1;
                //cout<<flowid<<"  1  yes"<<endl;
            }
            else if(table[idx1].fingerprint == fingerprint){
                table[idx1].count +=1;
               // cout<<flowid<<"  "<<table[idx1].count<<"  plus"<<endl;
            }
            else{
                mid_val stage1_item;
                stage1_item.fingerprint = table[idx1].fingerprint;
                stringstream ss1;
                ss1 << stage1_item.fingerprint;
                string fingerprint_str1 = ss1.str();
                stage1_item.fingerprint_str = fingerprint_str1;
                stage1_item.count = table[idx1].count;
                
                table[idx1].fingerprint = fingerprint;
                table[idx1].count = 1;
                fit_times += 1;
                mid_val * middle_item = update(stage1_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
                if(middle_item != 0){
                    fit_times += 1;
                    //cout<<"After 2: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                    middle_item = update(*middle_item, hash3, TABLE1_SIZE + TABLE2_SIZE, TABLE3_SIZE);
                    if(middle_item != 0){
                        fit_times += 1;
                        //cout<<"After 3: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                        middle_item = update(*middle_item, hash4, TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE, TABLE4_SIZE);
                    }
                }
            }
        }
        clock_t end_time = clock();
        double real_time = ((double)(end_time - st_time)) / CLOCKS_PER_SEC;
        cout<<"程序运行时间为 "<<real_time<<" 秒。"<<endl;
        cout<<"总计匹配次数： "<<fit_times<<"  平均匹配次数： "<<((double)fit_times) / ((double)n_pkts)<<endl;
        out<<filename<<"\t"<<real_time<<"\t"<<fit_times<<"\t"<<((double)fit_times) / ((double)n_pkts)<<endl;
        delete [] pkt_list;
        delete [] table;
    }
    out.close();
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

mid_val * update(mid_val middle_item, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod){
    mid_val * res = 0;
    int count = middle_item.count;
    uint32_t fingerprint = middle_item.fingerprint;
    
    uint32_t hash_value =CRC::Calculate(middle_item.fingerprint_str.c_str(), middle_item.fingerprint_str.length(), hash);
    int idx = hash_value % mod + base;
    
    if(table[idx].fingerprint == 0 && table[idx].count == 0){
        table[idx].fingerprint = fingerprint;
        table[idx].count = count;
    }
    else if(table[idx].fingerprint == fingerprint){
        table[idx].count += count;
    }
    else{
        res = new mid_val;
        if(table[idx].count < count){
            res->fingerprint = table[idx].fingerprint;
            res->count = table[idx].count;
            stringstream ss;
            ss << res->fingerprint;
            res->fingerprint_str = ss.str();
            table[idx].fingerprint = fingerprint;
            table[idx].count = count;
        }
        else{
            res->fingerprint = fingerprint;
            res->fingerprint_str = middle_item.fingerprint_str;
            res->count = count;
        }
    }
    return res;
}


//
//  main.cpp
//  PRECISION
//
//  Created by 熊斌 on 2020/1/12.
//  Copyright © 2020 熊斌. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <math.h>
#include <time.h>
#include "../Utils/CRC.h"

using namespace std;


typedef struct {
    uint32_t fingerprint;
    int count;
}item;

typedef struct{
    string fingerprint_str;
    uint32_t fingerprint;
    int count;
    int carry_mean;
    int carry_idx;
}mid_val;

#define TABLE1_SIZE 8192
#define TABLE2_SIZE 8192
#define TABLE3_SIZE 8192
#define TABLE4_SIZE 8192
#define TABLE_NUM 4


int n_pkts = 20000000;
//需要处理的 flowid 列表
string * pkt_list;
item * table;
int count1 = 0;

long long fit_times = 0;




//读取 json 文件到 pkt_list
bool read_json_file(string filename);
bool read_hgc_file(string filename);
//更新三个子表的函数
mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);

bool new_read_json_file(string filename){
    ifstream file(filename);
    if(!file.is_open()){
        cout<<"Can't open the file!"<<endl;
        return false;
    }
    int count = 0;
    int len = 1000;
    char * temp = new char[len];
    while (file.getline(temp, len) && count < n_pkts) {
        string temp_str = temp;
        if(temp_str.length() != 0){
            pkt_list[count] = temp_str;
            count ++;
        }
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


int main(int argc, const char * argv[]) {
    ofstream out("/Users/xiongbin/Desktop/PRECISION.txt");
    if(out.fail()){
        cout<<"error"<<endl;
        return 0;
    }
    for(int index = 1; index < 11; index ++){
        fit_times = 0;
        count1 = 0;
        //读取 json 文件
        string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
        //filename = "/Users/xiongbin/CAIDA/HGC.20080415000.dict.json";
        
        if(index == 0){
            filename = "/Users/xiongbin/CAIDA/trace3.json";
        }
        else if (index != 0){
            stringstream s1;
            s1 << index;
            string temp_str = s1.str();
            filename = "/Users/xiongbin/newtrace/trace" + temp_str;
        }
        cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;

        pkt_list = new string[n_pkts];
        if(!new_read_json_file(filename)){
            return 0;
        }
        table = new item[TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE+TABLE4_SIZE];
        for(int i = 0; i < TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE+TABLE4_SIZE; i++){
            table[i].fingerprint = 0;
            table[i].count = 0;
        }
        //定义 5 个 hash 函数
        CRC::Parameters<crcpp_uint32, 32> hash1 = { 0x04C11DB7, 0, 0xFFFFFFFF, false, false };
        CRC::Parameters<crcpp_uint32, 32> hash2 = { 0x1EDC6F41, 0, 0xFFFFFFFF, false, false };
        CRC::Parameters<crcpp_uint32, 32> hash3 = { 0xA833982B, 0, 0xFFFFFFFF, false, false };
        CRC::Parameters<crcpp_uint32, 32> hash4 = { 0x814141AB, 0, 0xFFFFFFFF, false, false };
        CRC::Parameters<crcpp_uint32, 32> hash5 = { 0x5A0849E7, 0, 0xFFFFFFFF, false, false };

        clock_t st_time = clock();
        for(int i = 0; i < n_pkts; i++){
            string flowid = pkt_list[i];
            uint32_t fingerprint = CRC::Calculate(flowid.c_str(), flowid.length(), hash5);
            stringstream ss;
            ss << fingerprint;
            string fingerprint_str = ss.str();
            mid_val first_value;
            first_value.fingerprint = fingerprint;
            first_value.fingerprint_str = fingerprint_str;
            first_value.count = 1;
            first_value.carry_idx = 0;
            first_value.carry_mean = n_pkts;
            fit_times += 1;
            mid_val * middle_item = update(first_value, hash1, 0, TABLE1_SIZE);
            if(middle_item != 0){
                fit_times += 1;
                middle_item = update(*middle_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
                if(middle_item != 0){
                    fit_times += 1;
                    //cout<<"After 2: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                    middle_item = update(*middle_item, hash3, TABLE1_SIZE + TABLE2_SIZE, TABLE3_SIZE);
                    if(middle_item != 0){
                        fit_times += 1;
                        //cout<<"After 3: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                        middle_item = update(*middle_item, hash4, TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE, TABLE4_SIZE);
                        if(middle_item != 0){
                            int new_value = (int) pow(2,ceil(log(middle_item->carry_mean)/log(2)));
                            if(new_value == 0)
                                new_value = 1;
                            int R = rand() % new_value;
                            if(R == 0){
                                table[middle_item->carry_idx].fingerprint = fingerprint;
                                table[middle_item->carry_idx].count = new_value;
                            }
                        }
                    }
                }
            }
        }
        clock_t end_time = clock();
        double real_time = ((double)(end_time - st_time)) / CLOCKS_PER_SEC;
        cout<<"程序运行时间为 "<<real_time<<" 秒。"<<endl;
        cout<<"总计匹配次数： "<<fit_times<<"  平均匹配次数： "<<((double)fit_times) / ((double)n_pkts)<<endl;
        out<<filename<<"\t"<<real_time<<"\t"<<fit_times<<"\t"<<((double)fit_times) / ((double)n_pkts)<<endl;
        delete [] pkt_list;
        delete [] table;
    }
    out.close();
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



mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod){
    mid_val * res = 0;
    uint32_t fingerprint = middle_value.fingerprint;
    int count = middle_value.count;
    uint32_t hash_value =CRC::Calculate(middle_value.fingerprint_str.c_str(), middle_value.fingerprint_str.length(), hash);
    int idx = hash_value % mod + base;
    
    if(table[idx].fingerprint == 0 && table[idx].count == 0){
        table[idx].fingerprint = fingerprint;
        table[idx].count = count;
    }
    else if(table[idx].fingerprint == fingerprint){
        table[idx].count += count;
    }
    else{
        res = new mid_val;
        res->fingerprint = fingerprint;
        res->fingerprint_str = middle_value.fingerprint_str;
        res->count = count;
        if(table[idx].count < middle_value.carry_mean){
            res->carry_mean = table[idx].count;
            res->carry_idx = idx;
        }
        else{
            res->carry_mean = middle_value.carry_mean;
            res->carry_idx = middle_value.carry_idx;
        }
    }
    return res;
}



//
//  main.cpp
//  AHashFlow
//
//  Created by 熊斌 on 2020/1/12.
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
#include <sstream>
#include <string>
#include <math.h>
#include <time.h>
#include "../Utils/CRC.h"

using namespace std;


typedef struct {
    uint32_t fingerprint;
    int count;
}item;

typedef struct {
    uint8_t digest;
    uint8_t cnt;
}A_item;

typedef struct{
    string fingerprint_str;
    uint32_t fingerprint;
    int count;
    int carry_min;
    int min_idx;
    int carry_max;
    int max_idx;
}mid_val;

#define TABLE1_SIZE 16384
#define TABLE2_SIZE 8192
#define TABLE3_SIZE 2048
#define A_SIZE 16384
#define B_SIZE 16384
#define FINGERPRINT_MASK 0xffffffff
#define A_CNT_MASK 0xff
#define DIGEST_MASK 0xff
#define B_MASK 0xffff


int n_pkts = 20000000;
//需要处理的 flowid 列表
string * pkt_list;
item * table;
A_item * A_table;
uint16_t * B_table;

int count1 = 0;

uint32_t M_hash_value;
long long fit_times = 0;
long long diff_times = 0;
int GAMMA = 5;
//读取 json 文件到 pkt_list
bool read_json_file(string filename);
bool read_hgc_file(string filename);
//更新三个子表的函数
mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);

bool new_read_json_file(string filename){
    ifstream file(filename);
    if(!file.is_open()){
        cout<<"Can't open the file!"<<endl;
        return false;
    }
    int count = 0;
    int len = 1000;
    char * temp = new char[len];
    while (file.getline(temp, len) && count < n_pkts) {
        string temp_str = temp;
        if(temp_str.length() != 0){
            pkt_list[count] = temp_str;
            count ++;
        }
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


void * AHashFlow(void * ptr){
    ofstream out("/Users/xiongbin/Desktop/AHashFlow-sw.txt");
    if(out.fail()){
        cout<<"error"<<endl;
        return 0;
    }
    for(GAMMA = 5; GAMMA < 6; GAMMA ++){
        for(int index = 1; index < 11; index ++){
            fit_times = 0;
            diff_times = 0;
            count1 = 0;
            //读取 json 文件
            string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
            
            if(index == 0){
                filename = "/Users/xiongbin/CAIDA/trace3.json";
            }
            else if (index == 11){
                filename = "/Users/xiongbin/CAIDA/HGC.20080415000.dict.json";
            }
            else if (index != 0){
                stringstream s1;
                s1 << index;
                string temp_str = s1.str();
                filename = "/Users/xiongbin/newtrace/trace" + temp_str;
            }
            cout<<"GAMMA 值为： "<<GAMMA<<endl;
            cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;
            
            pkt_list = new string[n_pkts];
            if(index != 11){
                if(!new_read_json_file(filename)){
                    return 0;
                }
            }
            else{
                if(!read_hgc_file(filename)){
                    return 0;
                }
            }
            table = new item[TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE];
            for(int i = 0; i < TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE; i++){
                table[i].fingerprint = 0;
                table[i].count = 0;
            }
            A_table = new A_item[A_SIZE];
            for(int i = 0; i < A_SIZE; i++){
                A_table[i].digest = 0;
                A_table[i].cnt = 0;
            }
            B_table = new uint16_t[B_SIZE];
            for(int i = 0; i < B_SIZE; i++){
                B_table[i] = 0;
            }
            //定义 6 个 hash 函数
            CRC::Parameters<crcpp_uint32, 32> hash1 = { 0x04C11DB7, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash2 = { 0x1EDC6F41, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash3 = { 0xA833982B, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash4 = { 0x814141AB, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash5 = { 0x5A0849E7, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash6 = { 0x28BA08BB, 0, 0xFFFFFFFF, false, false };
            clock_t st_time = clock();
            //int replace_times = 0;
            for(int i = 0; i < n_pkts; i++){
                string flowid = pkt_list[i];
                mid_val first_value;
                uint32_t fingerprint = CRC::Calculate(pkt_list[i].c_str(), pkt_list[i].length(), hash5) & FINGERPRINT_MASK;
                stringstream ss;
                ss << fingerprint;
                string fingerprint_str = ss.str();
                first_value.fingerprint_str = fingerprint_str;
                first_value.fingerprint = fingerprint;
                first_value.count = 1;
                first_value.min_idx = 0;
                first_value.carry_min = n_pkts;
                first_value.max_idx = 0;
                first_value.carry_max = 0;
                fit_times += 1;
                mid_val * middle_item = update(first_value, hash1, 0, TABLE1_SIZE);
                if(middle_item != 0){
                    fit_times += 1;
                    middle_item = update(*middle_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
                    if(middle_item != 0){
                        fit_times += 1;
                        //cout<<"After 2: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                        middle_item = update(*middle_item, hash3, TABLE1_SIZE + TABLE2_SIZE, TABLE3_SIZE);
                        if(middle_item != 0){
                            fit_times += 1;
                            //cout<<"After 3: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                            uint16_t cnt_max = middle_item->carry_max & B_MASK;
                            uint32_t idx4 = CRC::Calculate(middle_item->fingerprint_str.c_str(), middle_item->fingerprint_str.length(), hash4) % A_SIZE;
                            uint8_t digest = M_hash_value & DIGEST_MASK;
                            if(A_table[idx4].digest == 0 && A_table[idx4].cnt == 0){
                                A_table[idx4].digest = digest;
                                A_table[idx4].cnt = 1;
                                B_table[idx4] = cnt_max;
                            }
                            else if(A_table[idx4].digest == digest){
                                uint8_t temp_value = A_table[idx4].cnt + 1;
                                if(temp_value > middle_item->carry_min){
                                    table[middle_item->min_idx].fingerprint = middle_item->fingerprint;
                                    table[middle_item->min_idx].count = temp_value;
                                    A_table[idx4].digest = 0;
                                    A_table[idx4].cnt = 0;
                                }
                                else if(temp_value > GAMMA && cnt_max == B_table[idx4]){
                                    table[middle_item->max_idx].fingerprint = middle_item->fingerprint;
                                    table[middle_item->max_idx].count = temp_value;
                                    A_table[idx4].digest = 0;
                                    A_table[idx4].cnt = 0;
                                }
                                else{
                                    A_table[idx4].cnt = temp_value;
                                }
                            }
                            else{
                                diff_times += 1;
                                int new_value = (int) pow(2,ceil(log(A_table[idx4].cnt)/log(2)));
                                if(new_value == 0)
                                {
                                    new_value = 1;
                                }
                                int R = rand() % new_value;
                                if(R == 0){
                                    A_table[idx4].digest = digest;
                                    A_table[idx4].cnt = 1;
                                    B_table[idx4] = cnt_max;
                                }
                            }
                        }
                    }
                }
            }
            clock_t end_time = clock();
            double real_time = ((double)(end_time - st_time)) / CLOCKS_PER_SEC;
            cout<<"程序运行时间为 "<<real_time<<" 秒。"<<endl;
            cout<<"总计匹配次数： "<<fit_times<<"  平均匹配次数： "<<((double)fit_times) / ((double)n_pkts)<<endl;
            cout<<"总计替换次数： "<<diff_times<<"  平均替换次数： "<<((double)diff_times) / ((double)n_pkts)<<endl;
            out<<GAMMA<<"\t"<<filename<<"\t"<<real_time<<"\t"<<fit_times<<"\t"<<((double)fit_times) / ((double)n_pkts)<<"\t"<<diff_times<<"\t"<<((double)diff_times) / ((double)n_pkts)<<endl;
            delete [] pkt_list;
            delete [] table;
            delete [] A_table;
            delete [] B_table;
        }
    }
    out.close();
    return 0;
}

int main(int argc, const char * argv[]) {
    pthread_attr_t attr;
    int rs = pthread_attr_init (&attr);
    if (rs != 0)
    {
        cout<<"Initialize wrong!"<<endl;
        return 0;
    }
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    rs = pthread_attr_setschedpolicy (&attr, SCHED_FIFO);
    if (rs != 0)
    {
        cout<<"Setting wrong!"<<endl;
        return 0;
    }
    struct sched_param param;
    rs = pthread_attr_getschedparam(&attr, &param);
    if (rs != 0)
    {
        cout<<"Getting wrong!"<<endl;
        return 0;
    }
    int priority = sched_get_priority_max (SCHED_FIFO);
    param.sched_priority = priority;
    rs = pthread_attr_setschedparam(&attr, &param);
    if (rs != 0)
    {
        cout<<"Setting priority wrong!"<<endl;
        return 0;
    }
    
    pthread_t tid;
    pthread_create(&tid, &attr, AHashFlow, NULL);
    pthread_join(tid, NULL);
    
    rs = pthread_attr_destroy (&attr);
    if (rs != 0)
    {
        cout<<"Destroy wrong!"<<endl;
        return 0;
    }
    return 0;
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




mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod){
    //clock_t func_begin = clock();
    mid_val * res = 0;
    uint32_t fingerprint = middle_value.fingerprint;
    int count = middle_value.count;
    uint32_t temp = CRC::Calculate(middle_value.fingerprint_str.c_str(), middle_value.fingerprint_str.length(), hash);
    int idx = temp % mod + base;
    
    if(table[idx].fingerprint == 0 && table[idx].count == 0){
        table[idx].fingerprint = fingerprint;
        table[idx].count = count;
    }
    else if(table[idx].fingerprint == fingerprint){
        table[idx].count += count;
    }
    else{
        M_hash_value = temp;
        res = new mid_val;
        res->fingerprint_str = middle_value.fingerprint_str;
        res->fingerprint = fingerprint;
        res->count = count;
        if(table[idx].count < middle_value.carry_min){
            res->carry_min = table[idx].count;
            res->min_idx = idx;
        }
        else{
            res->carry_min = middle_value.carry_min;
            res->min_idx = middle_value.min_idx;
        }
        if(table[idx].count > middle_value.carry_max){
            res->carry_max = table[idx].count;
            res->max_idx = idx;
        }
        else{
            res->carry_max = middle_value.carry_max;
            res->max_idx = middle_value.max_idx;
        }
    }
    /*
    clock_t end_time = clock();
    func_time += ((double)(end_time - func_begin)) / CLOCKS_PER_SEC;*/
    return res;
}

//
//  main.cpp
//  AHashFlow-rw
//
//  Created by 熊斌 on 2020/2/29.
//  Copyright © 2020 熊斌. All rights reserved.
//


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <math.h>
#include <time.h>
#include "../Utils/CRC.h"

using namespace std;


typedef struct {
    uint32_t fingerprint;
    int count;
}item;

typedef struct {
    uint8_t digest;
    uint8_t cnt;
}A_item;

typedef struct{
    string fingerprint_str;
    uint32_t fingerprint;
    int count;
    int carry_min;
    int min_idx;
    int carry_max;
    int max_idx;
}mid_val;

#define TABLE1_SIZE 16384
#define TABLE2_SIZE 8192
#define TABLE3_SIZE 2048
#define A_SIZE 16384
#define B_SIZE 16384
#define FINGERPRINT_MASK 0xffffffff
#define A_CNT_MASK 0xff
#define DIGEST_MASK 0xff
#define B_MASK 0xffff

int n_pkts = 20000000;
//需要处理的 flowid 列表
string * pkt_list;
item * table;
A_item * A_table;
uint16_t * B_table;

int count1 = 0;
int GAMMA = 5;
uint32_t M_hash_value;

long long fit_times = 0;
long long diff_times = 0;

//读取 json 文件到 pkt_list
bool read_json_file(string filename);
bool read_hgc_file(string filename);
//更新三个子表的函数
mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);

bool new_read_json_file(string filename){
    ifstream file(filename);
    if(!file.is_open()){
        cout<<"Can't open the file!"<<endl;
        return false;
    }
    int count = 0;
    int len = 1000;
    char * temp = new char[len];
    while (file.getline(temp, len) && count < n_pkts) {
        string temp_str = temp;
        if(temp_str.length() != 0){
            pkt_list[count] = temp_str;
            count ++;
        }
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


void * AHashFlow(void * ptr){
    ofstream out("/Users/xiongbin/Desktop/AHashFlow-rw.txt");
    if(out.fail()){
        cout<<"error"<<endl;
        return 0;
    }
    for(GAMMA = 5; GAMMA < 6; GAMMA ++){
        for(int index = 1; index < 11; index ++){
            fit_times = 0;
            diff_times = 0;
            count1 = 0;
            //读取 json 文件
            string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
            
            if(index == 0){
                filename = "/Users/xiongbin/CAIDA/trace3.json";
            }
            else if (index == 11){
                filename = "/Users/xiongbin/CAIDA/HGC.20080415000.dict.json";
            }
            else if (index != 0){
                stringstream s1;
                s1 << index;
                string temp_str = s1.str();
                filename = "/Users/xiongbin/newtrace/trace" + temp_str;
            }
            cout<<"GAMMA 值为： "<<GAMMA<<endl;
            cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;
            
            pkt_list = new string[n_pkts];
            if(index != 11){
                if(!new_read_json_file(filename)){
                    return 0;
                }
            }
            else{
                if(!read_hgc_file(filename)){
                    return 0;
                }
            }
            table = new item[TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE];
            for(int i = 0; i < TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE; i++){
                table[i].fingerprint = 0;
                table[i].count = 0;
            }
            A_table = new A_item[A_SIZE];
            for(int i = 0; i < A_SIZE; i++){
                A_table[i].digest = 0;
                A_table[i].cnt = 0;
            }
            B_table = new uint16_t[B_SIZE];
            for(int i = 0; i < B_SIZE; i++){
                B_table[i] = 0;
            }
            //定义 6 个 hash 函数
            CRC::Parameters<crcpp_uint32, 32> hash1 = { 0x04C11DB7, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash2 = { 0x1EDC6F41, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash3 = { 0xA833982B, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash4 = { 0x814141AB, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash5 = { 0x5A0849E7, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash6 = { 0x28BA08BB, 0, 0xFFFFFFFF, false, false };
            clock_t st_time = clock();
            //int replace_times = 0;
            for(int i = 0; i < n_pkts; i++){
                string flowid = pkt_list[i];
                mid_val first_value;
                uint32_t fingerprint = CRC::Calculate(pkt_list[i].c_str(), pkt_list[i].length(), hash5) & FINGERPRINT_MASK;
                stringstream ss;
                ss << fingerprint;
                string fingerprint_str = ss.str();
                first_value.fingerprint_str = fingerprint_str;
                first_value.fingerprint = fingerprint;
                first_value.count = 1;
                first_value.min_idx = 0;
                first_value.carry_min = n_pkts;
                first_value.max_idx = 0;
                first_value.carry_max = 0;
                fit_times += 1;
                mid_val * middle_item = update(first_value, hash1, 0, TABLE1_SIZE);
                if(middle_item != 0){
                    fit_times += 1;
                    middle_item = update(*middle_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
                    if(middle_item != 0){
                        fit_times += 1;
                        //cout<<"After 2: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                        middle_item = update(*middle_item, hash3, TABLE1_SIZE + TABLE2_SIZE, TABLE3_SIZE);
                        if(middle_item != 0){
                            fit_times += 1;
                            //cout<<"After 3: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                            uint16_t cnt_max = middle_item->carry_max & B_MASK;
                            uint32_t idx4 = CRC::Calculate(middle_item->fingerprint_str.c_str(), middle_item->fingerprint_str.length(), hash4) % A_SIZE;
                            uint8_t digest = M_hash_value & DIGEST_MASK;
                            if(A_table[idx4].digest == 0 && A_table[idx4].cnt == 0){
                                A_table[idx4].digest = digest;
                                A_table[idx4].cnt = 1;
                                B_table[idx4] = cnt_max;
                            }
                            else if(A_table[idx4].digest == digest){
                                uint8_t temp_value = A_table[idx4].cnt + 1;
                                if(temp_value > middle_item->carry_min){
                                    table[middle_item->min_idx].fingerprint = middle_item->fingerprint;
                                    table[middle_item->min_idx].count = temp_value;
                                    A_table[idx4].digest = 0;
                                    A_table[idx4].cnt = 0;
                                }
                                else if(temp_value > GAMMA && cnt_max == B_table[idx4]){
                                    table[middle_item->max_idx].fingerprint = middle_item->fingerprint;
                                    table[middle_item->max_idx].count = temp_value;
                                    A_table[idx4].digest = 0;
                                    A_table[idx4].cnt = 0;
                                }
                                else{
                                    A_table[idx4].cnt = temp_value;
                                }
                            }
                            else{
                                diff_times += 1;
                                A_table[idx4].digest = digest;
                                A_table[idx4].cnt = 1;
                                B_table[idx4] = cnt_max;
                            }
                        }
                    }
                }
            }
            clock_t end_time = clock();
            double real_time = ((double)(end_time - st_time)) / CLOCKS_PER_SEC;
            cout<<"程序运行时间为 "<<real_time<<" 秒。"<<endl;
            cout<<"总计匹配次数： "<<fit_times<<"  平均匹配次数： "<<((double)fit_times) / ((double)n_pkts)<<endl;
            cout<<"总计替换次数： "<<diff_times<<"  平均替换次数： "<<((double)diff_times) / ((double)n_pkts)<<endl;
            out<<GAMMA<<"\t"<<filename<<"\t"<<real_time<<"\t"<<fit_times<<"\t"<<((double)fit_times) / ((double)n_pkts)<<"\t"<<diff_times<<"\t"<<((double)diff_times) / ((double)n_pkts)<<endl;
            delete [] pkt_list;
            delete [] table;
            delete [] A_table;
            delete [] B_table;
        }
    }
    out.close();
    return 0;
}





int main(int argc, const char * argv[]) {
    pthread_attr_t attr;
    int rs = pthread_attr_init (&attr);
    if (rs != 0)
    {
        cout<<"Initialize wrong!"<<endl;
        return 0;
    }
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    rs = pthread_attr_setschedpolicy (&attr, SCHED_FIFO);
    if (rs != 0)
    {
        cout<<"Setting wrong!"<<endl;
        return 0;
    }
    struct sched_param param;
    rs = pthread_attr_getschedparam(&attr, &param);
    if (rs != 0)
    {
        cout<<"Getting wrong!"<<endl;
        return 0;
    }
    int priority = sched_get_priority_max (SCHED_FIFO);
    param.sched_priority = priority;
    rs = pthread_attr_setschedparam(&attr, &param);
    if (rs != 0)
    {
        cout<<"Setting priority wrong!"<<endl;
        return 0;
    }
    
    pthread_t tid;
    pthread_create(&tid, &attr, AHashFlow, NULL);
    pthread_join(tid, NULL);
    
    rs = pthread_attr_destroy (&attr);
    if (rs != 0)
    {
        cout<<"Destroy wrong!"<<endl;
        return 0;
    }
    return 0;
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




mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod){
    //clock_t func_begin = clock();
    mid_val * res = 0;
    uint32_t fingerprint = middle_value.fingerprint;
    int count = middle_value.count;
    uint32_t temp = CRC::Calculate(middle_value.fingerprint_str.c_str(), middle_value.fingerprint_str.length(), hash);
    int idx = temp % mod + base;
    
    if(table[idx].fingerprint == 0 && table[idx].count == 0){
        table[idx].fingerprint = fingerprint;
        table[idx].count = count;
    }
    else if(table[idx].fingerprint == fingerprint){
        table[idx].count += count;
    }
    else{
        M_hash_value = temp;
        res = new mid_val;
        res->fingerprint_str = middle_value.fingerprint_str;
        res->fingerprint = fingerprint;
        res->count = count;
        if(table[idx].count < middle_value.carry_min){
            res->carry_min = table[idx].count;
            res->min_idx = idx;
        }
        else{
            res->carry_min = middle_value.carry_min;
            res->min_idx = middle_value.min_idx;
        }
        if(table[idx].count > middle_value.carry_max){
            res->carry_max = table[idx].count;
            res->max_idx = idx;
        }
        else{
            res->carry_max = middle_value.carry_max;
            res->max_idx = middle_value.max_idx;
        }
    }
    /*
    clock_t end_time = clock();
    func_time += ((double)(end_time - func_begin)) / CLOCKS_PER_SEC;*/
    return res;
}



//
//  main.cpp
//  AHashFlow-hw
//
//  Created by 熊斌 on 2020/2/29.
//  Copyright © 2020 熊斌. All rights reserved.
//


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <math.h>
#include <time.h>
#include "../Utils/CRC.h"

using namespace std;


typedef struct {
    uint32_t fingerprint;
    int count;
}item;

typedef struct {
    uint8_t digest;
    uint8_t cnt;
}A_item;

typedef struct{
    string fingerprint_str;
    uint32_t fingerprint;
    int count;
    int carry_min;
    int min_idx;
    int carry_max;
    int max_idx;
}mid_val;

#define TABLE1_SIZE 16384
#define TABLE2_SIZE 8192
#define TABLE3_SIZE 2048
#define A_SIZE 16384
#define B_SIZE 16384
#define FINGERPRINT_MASK 0xffffffff
#define A_CNT_MASK 0xff
#define DIGEST_MASK 0xff
#define B_MASK 0xffff
int GAMMA = 5;

int n_pkts = 20000000;
//需要处理的 flowid 列表
string * pkt_list;
item * table;
A_item * A_table;
uint16_t * B_table;

int count1 = 0;

uint32_t M_hash_value;
long long fit_times = 0;
long long diff_times = 0;


//读取 json 文件到 pkt_list
bool read_json_file(string filename);
bool read_hgc_file(string filename);
//更新三个子表的函数
mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);

bool new_read_json_file(string filename){
    ifstream file(filename);
    if(!file.is_open()){
        cout<<"Can't open the file!"<<endl;
        return false;
    }
    int count = 0;
    int len = 1000;
    char * temp = new char[len];
    while (file.getline(temp, len) && count < n_pkts) {
        string temp_str = temp;
        if(temp_str.length() != 0){
            pkt_list[count] = temp_str;
            count ++;
        }
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


void * AHashFlow(void * ptr){
    ofstream out("/Users/xiongbin/Desktop/AHashFlow-hw.txt");
    if(out.fail()){
        cout<<"error"<<endl;
        return 0;
    }
    for(GAMMA = 5; GAMMA < 6; GAMMA ++){
        for(int index = 1; index < 11; index ++){
            fit_times = 0;
            diff_times = 0;
            count1 = 0;
            //读取 json 文件
            string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
            
            if(index == 0){
                filename = "/Users/xiongbin/CAIDA/trace3.json";
            }
            else if (index == 11){
                filename = "/Users/xiongbin/CAIDA/HGC.20080415000.dict.json";
            }
            else if (index != 0){
                stringstream s1;
                s1 << index;
                string temp_str = s1.str();
                filename = "/Users/xiongbin/newtrace/trace" + temp_str;
            }
            cout<<"GAMMA 值为： "<<GAMMA<<endl;
            cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;
            
            pkt_list = new string[n_pkts];
            if(index != 11){
                if(!new_read_json_file(filename)){
                    return 0;
                }
            }
            else{
                if(!read_hgc_file(filename)){
                    return 0;
                }
            }
            table = new item[TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE];
            for(int i = 0; i < TABLE1_SIZE+TABLE2_SIZE+TABLE3_SIZE; i++){
                table[i].fingerprint = 0;
                table[i].count = 0;
            }
            A_table = new A_item[A_SIZE];
            for(int i = 0; i < A_SIZE; i++){
                A_table[i].digest = 0;
                A_table[i].cnt = 0;
            }
            B_table = new uint16_t[B_SIZE];
            for(int i = 0; i < B_SIZE; i++){
                B_table[i] = 0;
            }
            //定义 6 个 hash 函数
            CRC::Parameters<crcpp_uint32, 32> hash1 = { 0x04C11DB7, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash2 = { 0x1EDC6F41, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash3 = { 0xA833982B, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash4 = { 0x814141AB, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash5 = { 0x5A0849E7, 0, 0xFFFFFFFF, false, false };
            CRC::Parameters<crcpp_uint32, 32> hash6 = { 0x28BA08BB, 0, 0xFFFFFFFF, false, false };
            clock_t st_time = clock();
            //int replace_times = 0;
            for(int i = 0; i < n_pkts; i++){
                string flowid = pkt_list[i];
                mid_val first_value;
                uint32_t fingerprint = CRC::Calculate(pkt_list[i].c_str(), pkt_list[i].length(), hash5) & FINGERPRINT_MASK;
                stringstream ss;
                ss << fingerprint;
                string fingerprint_str = ss.str();
                first_value.fingerprint_str = fingerprint_str;
                first_value.fingerprint = fingerprint;
                first_value.count = 1;
                first_value.min_idx = 0;
                first_value.carry_min = n_pkts;
                first_value.max_idx = 0;
                first_value.carry_max = 0;
                fit_times += 1;
                mid_val * middle_item = update(first_value, hash1, 0, TABLE1_SIZE);
                if(middle_item != 0){
                    fit_times += 1;
                    middle_item = update(*middle_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
                    if(middle_item != 0){
                        fit_times += 1;
                        //cout<<"After 2: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                        middle_item = update(*middle_item, hash3, TABLE1_SIZE + TABLE2_SIZE, TABLE3_SIZE);
                        if(middle_item != 0){
                            fit_times += 1;
                            //cout<<"After 3: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                            uint16_t cnt_max = middle_item->carry_max & B_MASK;
                            uint32_t idx4 = CRC::Calculate(middle_item->fingerprint_str.c_str(), middle_item->fingerprint_str.length(), hash4) % A_SIZE;
                            uint8_t digest = M_hash_value & DIGEST_MASK;
                            if(A_table[idx4].digest == 0 && A_table[idx4].cnt == 0){
                                A_table[idx4].digest = digest;
                                A_table[idx4].cnt = 1;
                                B_table[idx4] = cnt_max;
                            }
                            else if(A_table[idx4].digest == digest){
                                uint8_t temp_value = A_table[idx4].cnt + 1;
                                if(temp_value > middle_item->carry_min){
                                    table[middle_item->min_idx].fingerprint = middle_item->fingerprint;
                                    table[middle_item->min_idx].count = temp_value;
                                    A_table[idx4].digest = 0;
                                    A_table[idx4].cnt = 0;
                                }
                                else if(temp_value > GAMMA && cnt_max == B_table[idx4]){
                                    table[middle_item->max_idx].fingerprint = middle_item->fingerprint;
                                    table[middle_item->max_idx].count = temp_value;
                                    A_table[idx4].digest = 0;
                                    A_table[idx4].cnt = 0;
                                }
                                else{
                                    A_table[idx4].cnt = temp_value;
                                }
                            }
                            else{
                                diff_times += 1;
                                if(A_table[idx4].cnt == 1){
                                    A_table[idx4].digest = digest;
                                    A_table[idx4].cnt = 1;
                                    B_table[idx4] = cnt_max;
                                }
                                else{
                                    A_table[idx4].cnt -= 1;
                                }
                            }
                        }
                    }
                }
            }
            clock_t end_time = clock();
            double real_time = ((double)(end_time - st_time)) / CLOCKS_PER_SEC;
            cout<<"程序运行时间为 "<<real_time<<" 秒。"<<endl;
            cout<<"总计匹配次数： "<<fit_times<<"  平均匹配次数： "<<((double)fit_times) / ((double)n_pkts)<<endl;
            cout<<"总计替换次数： "<<diff_times<<"  平均替换次数： "<<((double)diff_times) / ((double)n_pkts)<<endl;
            out<<GAMMA<<"\t"<<filename<<"\t"<<real_time<<"\t"<<fit_times<<"\t"<<((double)fit_times) / ((double)n_pkts)<<"\t"<<diff_times<<"\t"<<((double)diff_times) / ((double)n_pkts)<<endl;
            delete [] pkt_list;
            delete [] table;
            delete [] A_table;
            delete [] B_table;
        }
    }
    out.close();
    return 0;
}

int main(int argc, const char * argv[]) {
    pthread_attr_t attr;
    int rs = pthread_attr_init (&attr);
    if (rs != 0)
    {
        cout<<"Initialize wrong!"<<endl;
        return 0;
    }
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    rs = pthread_attr_setschedpolicy (&attr, SCHED_FIFO);
    if (rs != 0)
    {
        cout<<"Setting wrong!"<<endl;
        return 0;
    }
    struct sched_param param;
    rs = pthread_attr_getschedparam(&attr, &param);
    if (rs != 0)
    {
        cout<<"Getting wrong!"<<endl;
        return 0;
    }
    int priority = sched_get_priority_max (SCHED_FIFO);
    param.sched_priority = priority;
    rs = pthread_attr_setschedparam(&attr, &param);
    if (rs != 0)
    {
        cout<<"Setting priority wrong!"<<endl;
        return 0;
    }
    
    pthread_t tid;
    pthread_create(&tid, &attr, AHashFlow, NULL);
    pthread_join(tid, NULL);
    
    rs = pthread_attr_destroy (&attr);
    if (rs != 0)
    {
        cout<<"Destroy wrong!"<<endl;
        return 0;
    }
    return 0;
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




mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod){
    //clock_t func_begin = clock();
    mid_val * res = 0;
    uint32_t fingerprint = middle_value.fingerprint;
    int count = middle_value.count;
    uint32_t temp = CRC::Calculate(middle_value.fingerprint_str.c_str(), middle_value.fingerprint_str.length(), hash);
    int idx = temp % mod + base;
    
    if(table[idx].fingerprint == 0 && table[idx].count == 0){
        table[idx].fingerprint = fingerprint;
        table[idx].count = count;
    }
    else if(table[idx].fingerprint == fingerprint){
        table[idx].count += count;
    }
    else{
        M_hash_value = temp;
        res = new mid_val;
        res->fingerprint_str = middle_value.fingerprint_str;
        res->fingerprint = fingerprint;
        res->count = count;
        if(table[idx].count < middle_value.carry_min){
            res->carry_min = table[idx].count;
            res->min_idx = idx;
        }
        else{
            res->carry_min = middle_value.carry_min;
            res->min_idx = middle_value.min_idx;
        }
        if(table[idx].count > middle_value.carry_max){
            res->carry_max = table[idx].count;
            res->max_idx = idx;
        }
        else{
            res->carry_max = middle_value.carry_max;
            res->max_idx = middle_value.max_idx;
        }
    }
    /*
    clock_t end_time = clock();
    func_time += ((double)(end_time - func_begin)) / CLOCKS_PER_SEC;*/
    return res;
}


