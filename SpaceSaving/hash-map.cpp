//
//  hash-map.cpp
//  SpaceSaving
//
//  Created by 熊斌 on 2020/2/9.
//  Copyright © 2020 熊斌. All rights reserved.
//

#include "hash-map.h"
HashMap::HashMap(const int size) {
  size_ = size;
  keys_ = new string[size_];
  values_ = new Child*[size_];
  half_ = size_ / 2;
  for (int i = 0; i < size_; ++i) {
    keys_[i] = "0";
  }
}

HashMap::~HashMap() {
  delete[] keys_;
  delete[] values_;
}

void HashMap::Insert(const string key, Child* value) {
    uint32_t fingerprint = CRC::Calculate(key.c_str(), key.length(), hash1);
    int i = fingerprint % size_;
    while (keys_[i] != "0")
    {
        i = (i == size_ - 1) ? 0 : i + 1;
    }
    keys_[i] = key;
    values_[i] = value;
}

void HashMap::Remove(const string key) {
  if(key == "0")
      return;
  uint32_t fingerprint = CRC::Calculate(key.c_str(), key.length(), hash1);
  int i = fingerprint % size_;
  while (keys_[i] != key) {
    i = (i == size_ - 1) ? 0 : i + 1;
  }
  keys_[i] = "0";
  // Fill in the deletion gap.
  int j = (i == size_ - 1) ? 0 : i + 1;
  while (keys_[j] != "0") {
    uint32_t fingerprint_j = CRC::Calculate(keys_[j].c_str(), keys_[j].length(), hash1);
    int dis = (fingerprint_j % size_) - i;
    if (dis < -half_) dis += size_;
    if (dis > half_) dis -= size_;
    if (dis <= 0) {
      keys_[i] = keys_[j];
      values_[i] = values_[j];
      keys_[j] = "0";
      i = j;
    }
    j = (j == size_ - 1) ? 0 : j + 1;
  }
}

bool HashMap::Find(const string key, int* index) {
  uint32_t fingerprint = CRC::Calculate(key.c_str(), key.length(), hash1);
  int i = fingerprint % size_;
  while (true) {
    if (keys_[i] == "0") {
      return false;
    }
    if (keys_[i] == key) {
      *index = i;
      return true;
    }
    i = (i == size_ - 1) ? 0 : i + 1;
  }
  return false;
}
