//
//  main.cpp
//  AHashFlow-swNew
//
//  Created by 熊斌 on 2020/3/13.
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
#include <map>
#include <time.h>
#include "../Utils/CRC.h"

using namespace std;


#define TABLE1_SIZE 16384
#define TABLE2_SIZE 8192
#define TABLE3_SIZE 2048
#define A_SIZE 16384
#define B_SIZE 16384
#define FINGERPRINT_MASK 0xffffffff
#define A_CNT_MASK 0xff
#define DIGEST_MASK 0xff
#define B_MASK 0xffff
#define hash1 0x04C11DB7u
#define hash2 0x1EDC6F41u
#define hash3 0xA833982Bu
#define hash4 0x814141ABu
#define hash6 0x28BA08BBu

CRC::Parameters<crcpp_uint32, 32> hash5 = { 0x5a0849e7, 0, 0xFFFFFFFF, false, false };

int n_pkts = 40000000;
//需要处理的 flowid 列表
uint32_t * fp_list;
uint32_t ** table;

uint8_t ** A_table;
uint16_t * B_table;


long long fit_times = 0;
int GAMMA = 5;



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
        ofstream out("/Users/xiongbin/Desktop/AHashFlow-sw.txt");
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
        table = (uint32_t **) new uint32_t *[TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE];
        for(int i=0; i<(TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE);i++){
            table[i] = (uint32_t *) new uint32_t[2];
            table[i][0] = 0;
            table[i][1] = 0;
        }
        
        A_table = (uint8_t **) new uint8_t * [A_SIZE];
        for(int i = 0; i < A_SIZE; i++){
            A_table[i] = (uint8_t *) new uint8_t[2];
            A_table[i][0] = 0;
            A_table[i][1] = 0;
        }
        B_table = new uint16_t[B_SIZE];
        for(int i = 0; i < B_SIZE; i++){
            B_table[i] = 0;
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
            
            int idx1 = ((fp_list[i] * hash1) >> 15) % TABLE1_SIZE;
            
            if(table[idx1][0] == 0){
                table[idx1][0] = fp_list[i];
                table[idx1][1] = 1;
            }
            else if(table[idx1][0] == fp_list[i]){
                table[idx1][1] += 1;
            }
            else{
                //对第二个表进行处理
                fit_times += 1;
                int idx2 = TABLE1_SIZE + ((fp_list[i] * hash2) >> 15) % TABLE2_SIZE;
                
                if(table[idx2][0] == 0){
                    table[idx2][0] = fp_list[i];
                    table[idx2][1] = 1;
                }
                else if(table[idx2][0] == fp_list[i]){
                    table[idx2][1] += 1;
                }
                else{
                    //对第三个表进行处理
                    fit_times += 1;
                    uint32_t mid_rs = ((fp_list[i] * hash3) >> 15);
                    int idx3 = TABLE1_SIZE + TABLE2_SIZE + mid_rs % TABLE3_SIZE;
                    
                    if(table[idx3][0] == 0){
                        table[idx3][0] = fp_list[i];
                        table[idx3][1] = 1;
                    }
                    else if(table[idx3][0] == fp_list[i]){
                        table[idx3][1] += 1;
                    }
                    else{
                        //对第四个表(A, B 表)进行处理
                        fit_times += 1;
                        int idx4 = ((fp_list[i] * hash4) >> 15) % A_SIZE;
                        uint8_t digest = (uint8_t) (mid_rs);
                        if(A_table[idx4][1] == 0){
                            //flag1 ++;
                            A_table[idx4][0] = digest;
                            A_table[idx4][1] = 1;
                            
                            B_table[idx4] = (uint16_t)(table[(table[idx1][1] > table[idx2][1])?(table[idx1][1] > table[idx3][1]?idx1:idx3):(table[idx2][1] > table[idx3][1]?idx2:idx3)][1]);
                        }
                        else if(A_table[idx4][0] == digest){
                            A_table[idx4][1] += 1;
                            int idx_min;
                            int idx_max;
                            int max_flag = 0;
                            //flag2 ++;
                            if(table[idx1][1] < table[idx2][1]){
                                if(table[idx1][1] < table[idx3][1]){
                                    idx_min = idx1;
                                    max_flag = 1;
                                } //1 < 2  && 1 < 3
                                else{
                                    idx_min = idx3;
                                    idx_max = idx2;
                                } // 3 < 1 < 2
                            } // 1 < 2
                            else if(table[idx2][1] < table[idx3][1]){
                                idx_min = idx2;
                                max_flag = 2;
                            } // 1 > 2 && 2 < 3
                            else{
                                idx_min = idx3;
                                idx_max = idx1;
                            }  // 1 > 2 > 3
                            
                            
                            
                            if(A_table[idx4][1]> table[idx_min][1]){
                                //flag3++;
                                fit_times += 1;
                                
                                table[idx_min][0] = fp_list[i];
                                table[idx_min][1] = A_table[idx4][1];
                                
                                A_table[idx4][1] = 0;
                            }
                            else if(A_table[idx4][1] > GAMMA){
                                if(max_flag == 1){
                                    if(table[idx2][1] < table[idx3][1]){
                                        idx_max = idx3;
                                    }
                                    else{
                                        idx_max = idx2;
                                    }
                                }
                                else if (max_flag == 2){
                                    if(table[idx1][1] < table[idx3][1]){
                                        idx_max = idx3;
                                    }
                                    else{
                                        idx_max = idx1;
                                    }
                                }
                                if((uint16_t)(table[idx_max][1]) == B_table[idx4]){
                                    //flag4 ++;
                                    fit_times += 1;
                                    
                                    table[idx_max][0] = fp_list[i];
                                    table[idx_max][1] = A_table[idx4][1];
                                    
                                    A_table[idx4][1] = 0;
                                }
                                
                            }
                        }
                        else{
                            
                            if((rand_val[rnd_cnt++]) % ((int) pow(2,ceil(log(A_table[idx4][1])/log(2)))) == 0){
                                A_table[idx4][0] = digest;
                                A_table[idx4][1] = 1;
                                //flag5 ++;
                                B_table[idx4] = (uint16_t)(table[(table[idx1][1] > table[idx2][1])?(table[idx1][1] > table[idx3][1]?idx1:idx3):(table[idx2][1] > table[idx3][1]?idx2:idx3)][1]);
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
        for(int i=0;i<(TABLE1_SIZE + TABLE2_SIZE + TABLE3_SIZE );i++){
            delete [] table[i];
        }
        delete [] table;
        for(int i = 0; i < A_SIZE; i++){
            delete [] A_table[i];
        }
        delete [] A_table;
        delete [] B_table;
    }
    out.close();
    return 0;
}

