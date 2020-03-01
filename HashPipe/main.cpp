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






//读取 json 文件到 pkt_list
bool read_json_file(string filename);
bool read_hgc_file(string filename);
//更新三个子表的函数
mid_val * update(mid_val middle_item, CRC::Parameters<crcpp_uint32, 32> hash, int base, int mod);


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
                mid_val * middle_item = update(stage1_item, hash2, TABLE1_SIZE, TABLE2_SIZE);
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
        clock_t end_time = clock();
        double real_time = ((double)(end_time - st_time)) / CLOCKS_PER_SEC;
        cout<<"程序运行时间为 "<<real_time<<" 秒。"<<endl;
        delete [] pkt_list;
        delete [] table;
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
