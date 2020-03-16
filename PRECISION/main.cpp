//
//  main.cpp
//  APRECISION
//
//  Created by 熊斌 on 2020/3/13.
//  Copyright © 2020 熊斌. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <math.h>
#include <time.h>
#include <map>
#include "../Utils/CRC.h"

using namespace std;


#define TABLE1_SIZE 8192
#define TABLE2_SIZE 8192
#define TABLE3_SIZE 8192
#define TABLE4_SIZE 8192
#define FINGERPRINT_MASK 0xFFFFFFFF
#define hash1 0x04C11DB7
#define hash2 0x1EDC6F41
#define hash3 0xA833982B
#define hash4 0x814141AB

CRC::Parameters<crcpp_uint32, 32> hash5 = { 0x5a0849e7, 0, 0xFFFFFFFF, false, false };


int n_pkts = 40000000;
//需要处理的 flowid 列表
uint32_t * fp_list;
uint32_t ** table;

long long fit_times = 0;


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
            fp_list[count] = CRC::Calculate(temp_str.c_str(), temp_str.length(), hash5) & FINGERPRINT_MASK;
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
        cout<<fp_list[i]<<endl;
    }*/
    return true;
}


int main(int argc, const char * argv[]) {
    ofstream out("/Users/xiongbin/Desktop/APRECISION.txt");
    if(out.fail()){
        cout<<"error"<<endl;
        return 0;
    }
    for(int index = 1; index < 11; index ++){
        fit_times = 0;
        
        //文件名称
        string filename;
        stringstream s1;
        s1 << index;
        string temp_str = s1.str();
        filename = "/Users/xiongbin/newtrace2/trace" + temp_str;
        cout<<"第 "<<index<<" 次统计开始。文件名为："<<filename<<endl;
        
        //读取指纹数据
        fp_list = new uint32_t[n_pkts];
        if(!new_read_json_file(filename)){
            return 0;
        }
        
        //为 table 分配空间
        table = (uint32_t **) new uint32_t *[TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE + TABLE4_SIZE];
        for(int i=0; i<(TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE + TABLE4_SIZE);i++){
            table[i] = (uint32_t *) new uint32_t[2];
            table[i][0] = 0;
            table[i][1] = 0;
        }
        
        //新建随机数序列
        srand(1);
        int * rand_val = new int[n_pkts];
        for(int i = 0; i < n_pkts; i ++){
            rand_val[i] = rand();
        }
        int rnd_cnt = 0;
        
        
        //开始对每一个数据包进行处理
        clock_t st_time = clock();
        for(int i = 0; i < n_pkts; i++){
            //对第一个表进行处理
            fit_times += 1;
            int idx = ((fp_list[i] * hash1) >> 15) % TABLE1_SIZE;
            
            if(table[idx][0] == 0){
                table[idx][0] = fp_list[i];
                table[idx][1] = 1;
            }
            else if(table[idx][0] == fp_list[i]){
                table[idx][1] += 1;
            }
            else{
                int idx_min = idx;
                
                //对第二个表进行处理
                fit_times += 1;
                idx = TABLE1_SIZE + ((fp_list[i] * hash2) >> 15) % TABLE2_SIZE;
                
                if(table[idx][0] == 0){
                    table[idx][0] = fp_list[i];
                    table[idx][1] = 1;
                }
                else if(table[idx][0] == fp_list[i]){
                    table[idx][1] += 1;
                }
                else{
                    if(table[idx][1] < table[idx_min][1]){
                        idx_min = idx;
                    }
                    //对第三个表进行处理
                    fit_times += 1;
                    idx = TABLE1_SIZE + TABLE2_SIZE + ((fp_list[i] * hash3) >> 15) % TABLE3_SIZE;
                    
                    if(table[idx][0] == 0){
                        table[idx][0] = fp_list[i];
                        table[idx][1] = 1;
                    }
                    else if(table[idx][0] == fp_list[i]){
                        table[idx][1] += 1;
                    }
                    else{
                        if(table[idx][1] < table[idx_min][1]){
                            idx_min = idx;
                        }
                        
                        //对第四个表进行处理
                        fit_times += 1;
                        idx = TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE + ((fp_list[i] * hash4) >> 15) % TABLE4_SIZE;
                        
                        if(table[idx][1] == 0){
                            table[idx][0] = fp_list[i];
                            table[idx][1] = 1;
                        }
                        else if(table[idx][0] == fp_list[i]){
                            table[idx][1] += 1;
                        }
                        else{
                            if(table[idx][1] < table[idx_min][1]){
                                idx_min = idx;
                            }
                            int new_value = (int) pow(2,ceil(log(table[idx_min][1])/log(2)));
                            if((rand_val[rnd_cnt++]) % new_value == 0){
                                fit_times += 1;
                                table[idx_min][0] = fp_list[i];
                                table[idx_min][1] = new_value;
                            }
                        }
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
        delete [] fp_list;
        for(int i=0;i<(TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE + TABLE4_SIZE);i++){
            delete [] table[i];
        }
        delete [] table;
    }
    out.close();
}


