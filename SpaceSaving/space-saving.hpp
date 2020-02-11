//
//  space-saving.hpp
//  SpaceSaving
//
//  Created by 熊斌 on 2020/2/9.
//  Copyright © 2020 熊斌. All rights reserved.
//

#ifndef space_saving_hpp
#define space_saving_hpp

#include "hash-map.h"

class Child;

class Parent {
 public:
  Parent();
  void Add(Child* c);
  Parent* left_;
  Parent* right_;
  Child* child_;
  unsigned long long value_;
};

class Child {
 public:
  Child();
  void Detach(Parent** smallest, HashMap* map);
  Parent* parent_;
  Child* next_;
  string element_;
};

class SpaceSaving {
 public:
  SpaceSaving(const int size);
  ~SpaceSaving();
  void Process(const string element);
  void Increment(Child* bucket);
  HashMap* map_;
  Parent* smallest_;
  Parent* largest_;
};

#endif /* space_saving_hpp */
