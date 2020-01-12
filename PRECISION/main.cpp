//
//  main.cpp
//  PRECISION
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
    string flowid;
    int count;
}item;

typedef struct{
    string flowid;
    int count;
    int carry_mean;
    int carry_idx;
}mid_val;

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
mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);


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
    //int replace_times = 0;
    for(int i = 0; i < n_pkts; i++){
        string flowid = pkt_list[i];
        mid_val first_value;
        first_value.flowid = flowid;
        first_value.count = 1;
        first_value.carry_idx = 0;
        first_value.carry_mean = n_pkts;
        mid_val * middle_item = update(first_value, hash1, 0, TABLE1_SIZE);
        if(middle_item != 0){
            middle_item = update(*middle_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
            if(middle_item != 0){
                //cout<<"After 2: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                middle_item = update(*middle_item, hash3, TABLE1_SIZE + TABLE2_SIZE, TABLE3_SIZE);
                if(middle_item != 0){
                    //cout<<"After 3: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                    middle_item = update(*middle_item, hash4, TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE, TABLE4_SIZE);
                    if(middle_item != 0){
                        int new_value = (int) pow(2,ceil(log(middle_item->carry_mean)/log(2)));
                        int R = rand() % new_value;
                        if(R == 0){
                            //cout<<middle_item->carry_idx<<"  "<<middle_item->carry_mean<<endl;
                            replace_times ++;
                            table[middle_item->carry_idx].flowid = flowid;
                            table[middle_item->carry_idx].count = new_value;
                        }
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
    string flowid = middle_value.flowid;
    int count = middle_value.count;
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
        res = new mid_val;
        res->flowid = flowid;
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
