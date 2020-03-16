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
#define hash1 0x04C11DB7
#define hash2 0x1EDC6F41
#define hash3 0xA833982B
#define hash4 0x814141AB
//********************//
//*****全局变量部分*****//
//*******************//

CRC::Parameters<crcpp_uint32, 32> hash5 = { 0x5a0849e7, 0, 0xFFFFFFFF, false, false };

//处理的数据包的数量
int n_pkts = 40000000;
//需要处理的 flowid 列表
uint32_t * fingerprint_list;
//三个 HEAVY 表
uint32_t ** heavy;
// light 表
uint8_t * light;
//(vote- / vote+) > thresh 时发生替换
uint32_t thresh = 8;

long long fit_times = 0;

//********************//
//*****函数声明部分*****//
//*******************//


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
            fingerprint_list[count] = CRC::Calculate(temp_str.c_str(), temp_str.length(), hash5) & FINGERPRINT_MASK;
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
        cout<<fingerprint_list[i]<<endl;
    }*/
    return true;
}


//main 函数
int main(){
    ofstream out("/Users/xiongbin/Desktop/Elastic.txt");
    if(out.fail()){
        cout<<"error"<<endl;
        return 0;
    }
    for(int index = 9; index < 11; index ++){
        fit_times = 0;
        long long change_times = 0;
        
        //文件名称
        string filename;
        stringstream s1;
        s1 << index;
        string temp_str = s1.str();
        filename = "/Users/xiongbin/newtrace2/trace" + temp_str;
        cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;
        
        //读取指纹数据
        fingerprint_list = new uint32_t[n_pkts];
        if(!new_read_json_file(filename)){
            return 0;
        }
        
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
        
        
        //开始对每一个数据包进行处理
        clock_t st_time = clock();
        for(int i=0;i<n_pkts;i++){

            //对第一个 heavy 表进行处理
            fit_times += 1;
            int idx = ((fingerprint_list[i] * hash1) >> 15) % HEAVY_PART_1_SIZE;
            if(heavy[idx][0] == 0){
                heavy[idx][0] = fingerprint_list[i];
                heavy[idx][1] = 1;
            }
            else if(fingerprint_list[i] == heavy[idx][0]){
                heavy[idx][1] += 1;
            }
            else{
                heavy[idx][3] += 1;
                uint32_t fp_now;
                uint32_t cnt_now;
                if(heavy[idx][3] >= thresh * heavy[idx][1]){
                    change_times ++;
                    fp_now = heavy[idx][0];
                    cnt_now = heavy[idx][1];
                    heavy[idx][0] = fingerprint_list[i];
                    heavy[idx][1] = 1;
                    heavy[idx][2] = 1;
                    heavy[idx][3] = 0;
                }
                else{
                    fp_now = fingerprint_list[i];
                    cnt_now = 1;
                }
                
                //对第二个 heavy 表进行处理
                fit_times += 1;
                idx = HEAVY_PART_1_SIZE + ((fp_now * hash2) >> 15) % HEAVY_PART_2_SIZE;
                if(heavy[idx][0] == 0){
                    heavy[idx][0] = fp_now;
                    heavy[idx][1] = cnt_now;
                }
                else if(fp_now == heavy[idx][0]){
                    heavy[idx][1] += cnt_now;
                }
                else{
                    heavy[idx][3] += cnt_now;
                    if(heavy[idx][3] >= thresh * heavy[idx][1]){
                        change_times ++;
                        uint32_t fp_tmp = fp_now;
                        uint32_t cnt_tmp = cnt_now;
                        fp_now = heavy[idx][0];
                        cnt_now = heavy[idx][1];
                        heavy[idx][0] = fp_tmp;
                        heavy[idx][1] = cnt_tmp;
                        heavy[idx][2] = 1;
                        heavy[idx][3] = 0;
                    }
                    
                    //对第三个 heavy 表进行处理
                    fit_times += 1;
                    idx = HEAVY_PART_1_SIZE + HEAVY_PART_2_SIZE + ((fp_now * hash3) >> 15) % HEAVY_PART_3_SIZE;
                    if(heavy[idx][0] == 0){
                        heavy[idx][0] = fp_now;
                        heavy[idx][1] = cnt_now;
                    }
                    else if(fp_now == heavy[idx][0]){
                        heavy[idx][1] += cnt_now;
                    }
                    else{
                        heavy[idx][3] += cnt_now;
                        if(heavy[idx][3] >= thresh * heavy[idx][1]){
                            change_times ++;
                            uint32_t fp_tmp = fp_now;
                            uint32_t cnt_tmp = cnt_now;
                            fp_now = heavy[idx][0];
                            cnt_now = heavy[idx][1];
                            heavy[idx][0] = fp_tmp;
                            heavy[idx][1] = cnt_tmp;
                            heavy[idx][2] = 1;
                            heavy[idx][3] = 0;
                        }
                        
                        //对 light 表进行处理
                        fit_times += 1;
                        idx = ((fp_now * hash4) >> 15) % LIGHT_PART_SIZE;
                        light[idx] += (uint8_t)(cnt_now);
                    }
                }
            }
        }
        clock_t end_time = clock();
        double real_time = ((double)(end_time - st_time)) / CLOCKS_PER_SEC;
        cout<<"程序运行时间为 "<<real_time<<" 秒。"<<endl;
        cout<<"吞吐率： "<<((double)n_pkts) / (real_time)<<endl;
        cout<<"总计匹配次数： "<<fit_times<<"  平均匹配次数： "<<((double)fit_times) / ((double)n_pkts)<<endl;
        cout<<"总计交换次数： "<<change_times<<"  平均交换次数： "<<((double)change_times) / ((double)n_pkts)<<endl;
        out<<filename<<"\t"<<real_time<<"\t"<<((double)n_pkts) / (real_time)<<"\t"<<fit_times<<"\t"<<((double)fit_times) / ((double)n_pkts)<<"\t"<<change_times<<"\t"<<((double)change_times) / ((double)n_pkts)<<endl;
        delete [] fingerprint_list;
        for(int i=0;i<(HEAVY_PART_1_SIZE+HEAVY_PART_2_SIZE+HEAVY_PART_3_SIZE);i++){
            delete [] heavy[i];
        }
        delete [] heavy;
        delete [] light;
    }
    out.close();
}

