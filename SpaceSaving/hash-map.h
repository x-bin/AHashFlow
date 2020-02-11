//
//  hash-map.h
//  SpaceSaving
//
//  Created by 熊斌 on 2020/2/9.
//  Copyright © 2020 熊斌. All rights reserved.
//

#ifndef hash_map_h
#define hash_map_h
#include <string>
using namespace std;
#include "../Utils/CRC.h"
class Child;

class HashMap {
 public:
  HashMap(const int table_size);
  ~HashMap();
  void Insert(const string key, Child* value);
  void Remove(const string key);
  bool Find(const string key, int* index);
  string * keys_;
  Child** values_;
  int size_;
  int half_;
  CRC::Parameters<crcpp_uint32, 32> hash1 = { 0x04C11DB7, 0, 0xFFFFFFFF, false, false };
};

#endif /* hash_map_h */
