//
//  main.cpp
//  AHashFlow
//
//  Created by 熊斌 on 2020/1/12.
//  Copyright © 2020 熊斌. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
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
    string flowid;
    uint32_t fingerprint;
    int count;
    int carry_min;
    int min_idx;
    int carry_max;
    int max_idx;
}mid_val;

#define TABLE1_SIZE 8192
#define TABLE2_SIZE 4096
#define TABLE3_SIZE 4096
#define A_SIZE 16384
#define B_SIZE 16384
#define FINGERPRINT_MASK 0xffffffff
#define A_CNT_MASK 0xff
#define DIGEST_MASK 0xff
#define B_MASK 0xffff
#define GAMMA 5

int n_pkts = 1000000;
//需要处理的 flowid 列表
string * pkt_list;
item * table;
A_item * A_table;
uint16_t * B_table;







//读取 json 文件到 pkt_list
bool read_json_file(string filename);
//更新三个子表的函数
mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);


int main(int argc, const char * argv[]) {
    //读取 json 文件
    string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
    pkt_list = new string[n_pkts];
    if(!read_json_file(filename)){
        return 0;
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

    //int replace_times = 0;
    for(int i = 0; i < n_pkts; i++){
        string flowid = pkt_list[i];
        mid_val first_value;
        uint32_t fingerprint = CRC::Calculate(pkt_list[i].c_str(), pkt_list[i].length(), hash5) & FINGERPRINT_MASK;
        first_value.flowid = flowid;
        first_value.fingerprint = fingerprint;
        first_value.count = 1;
        first_value.min_idx = 0;
        first_value.carry_min = n_pkts;
        first_value.max_idx = 0;
        first_value.carry_max = 0;
        mid_val * middle_item = update(first_value, hash1, 0, TABLE1_SIZE);
        if(middle_item != 0){
            middle_item = update(*middle_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
            if(middle_item != 0){
                //cout<<"After 2: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                middle_item = update(*middle_item, hash3, TABLE1_SIZE + TABLE2_SIZE, TABLE3_SIZE);
                if(middle_item != 0){
                    //cout<<"After 3: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                    uint16_t cnt_max = middle_item->carry_max & B_MASK;
                    uint32_t idx4 = CRC::Calculate(middle_item->flowid.c_str(), middle_item->flowid.length(), hash4) % A_SIZE;
                    uint8_t digest = CRC::Calculate(middle_item->flowid.c_str(), middle_item->flowid.length(), hash6) & DIGEST_MASK;
                    if(A_table[idx4].digest == 0 && A_table[idx4].cnt == 0){
                        A_table[idx4].digest = digest;
                        A_table[idx4].cnt = 1;
                        B_table[idx4] = cnt_max;
                    }
                    else if(A_table[idx4].digest == digest){
                        int temp_value = A_table[idx4].cnt + 1;
                        if(temp_value > middle_item->carry_min){
                            table[middle_item->min_idx].fingerprint = middle_item->fingerprint;
                            table[middle_item->min_idx].count = temp_value;
                        }
                        else if(temp_value > GAMMA && cnt_max == B_table[idx4]){
                            table[middle_item->max_idx].fingerprint = middle_item->fingerprint;
                            table[middle_item->max_idx].count = temp_value;
                            table[middle_item->min_idx].fingerprint = 0;
                            table[middle_item->min_idx].count = 0;
                        }
                        else{
                            A_table[idx4].cnt = temp_value;
                        }
                    }
                    else{
                        A_table[idx4].digest = digest;
                        A_table[idx4].cnt = 1;
                        B_table[idx4] = cnt_max;
                    }
                }
            }
        }
    }
    //cout<<"Replace times: "<<replace_times<<endl;
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

mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod){
    mid_val * res = 0;
    uint32_t fingerprint = middle_value.fingerprint;
    int count = middle_value.count;
    uint32_t temp = CRC::Calculate(middle_value.flowid.c_str(), middle_value.flowid.length(), hash);
    int idx = temp % mod + base;
    
    if(table[idx].fingerprint == 0 && table[idx].count == 0){
        table[idx].fingerprint = fingerprint;
        table[idx].count = count;
    }
    else if(table[idx].fingerprint == fingerprint){
        table[idx].count += count;
    }
    else{
        res = new mid_val;
        res->flowid = middle_value.flowid;
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
    return res;
}

