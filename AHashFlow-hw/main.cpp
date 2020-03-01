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
#define GAMMA 5

int n_pkts = 20000000;
//需要处理的 flowid 列表
string * pkt_list;
item * table;
A_item * A_table;
uint16_t * B_table;

int count1 = 0;

uint32_t M_hash_value;

//读取 json 文件到 pkt_list
bool read_json_file(string filename);
bool read_hgc_file(string filename);
//更新三个子表的函数
mid_val * update(mid_val middle_value, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);


int main(int argc, const char * argv[]) {
    for(int index = 1; index < 2; index ++){
        count1 = 0;
        //读取 json 文件
        string filename = "/Users/xiongbin/CAIDA/CAIDA.equinix-nyc.dirA.20180315-125910.UTC.anon.clean.json";
        filename = "/Users/xiongbin/CAIDA/HGC.20080415000.dict.json";
        /*
        if(index == 1){
            filename = "/Users/xiongbin/CAIDA/trace1.json";
        }
        else{
            stringstream s1;
            s1 << index;
            string temp_str = s1.str();
            filename = "/Users/xiongbin/CAIDA/trace" + temp_str;
        }*/
        cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;

        pkt_list = new string[n_pkts];
        if(!read_hgc_file(filename)){
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
            mid_val * middle_item = update(first_value, hash1, 0, TABLE1_SIZE);
            if(middle_item != 0){
                middle_item = update(*middle_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
                if(middle_item != 0){
                    //cout<<"After 2: "<<middle_item->flowid<<"  "<<middle_item->count<<endl;
                    middle_item = update(*middle_item, hash3, TABLE1_SIZE + TABLE2_SIZE, TABLE3_SIZE);
                    if(middle_item != 0){
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
        delete [] pkt_list;
        delete [] table;
        delete [] A_table;
        delete [] B_table;
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

