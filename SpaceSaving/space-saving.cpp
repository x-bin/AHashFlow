//
//  space-saving.cpp
//  SpaceSaving
//
//  Created by 熊斌 on 2020/2/9.
//  Copyright © 2020 熊斌. All rights reserved.
//

#include "space-saving.hpp"

Parent::Parent() : left_(NULL), right_(NULL), child_(NULL), value_(0) {}

void Parent::Add(Child* c) {
  c->parent_ = this;
  if (child_ == NULL) {
    child_ = c;
    c->next_ = c;
    return;
  }
  c->next_ = child_->next_;
  child_->next_ = c;
  child_ = c;
}

Child::Child() : parent_(NULL), next_(NULL), element_("0") {}

void Child::Detach(Parent** smallest, HashMap* map) {
  if (next_ == this) {
    if (parent_ == *smallest) {
      *smallest = parent_->right_;
      parent_->right_->left_ = NULL;
      delete parent_;
      return;
    }
    parent_->right_->left_ = parent_->left_;
    parent_->left_->right_ = parent_->right_;
    delete parent_;
    return;
  }
  if (parent_->child_ == next_) {
    parent_->child_ = this;
  }
  string temp = element_;
  element_ = next_->element_;
  next_->element_ = temp;
  int index;
  if (element_ != "0") {
    map->Find(element_, &index);
    map->values_[index] = this;
  }
  if (next_->element_ != "0") {
    map->Find(next_->element_, &index);
    map->values_[index] = next_;
  }
  next_ = next_->next_;
}

SpaceSaving::SpaceSaving(const int size) {
  map_ = new HashMap(size);
  smallest_ = new Parent();
  largest_ = smallest_;
    for (int i = 0; i < size; ++i) {
        smallest_->Add(new Child());
  }
}


SpaceSaving::~SpaceSaving() {
  for (int i = 0; i < map_->size_; ++i) {
    if (map_->keys_[i] != "0") {
      delete map_->values_[i];
    }
  }
  while (smallest_ != NULL) {
    Parent* tmp = smallest_->right_;
    delete smallest_;
    smallest_ = tmp;
  }
  delete map_;
}

void SpaceSaving::Increment(Child* bucket) {
  unsigned long long count = bucket->parent_->value_ + 1;
  Parent* next = bucket->parent_->right_;
  if (next != NULL && next->value_ == count) {
    Child* temp = bucket->next_;
    bucket->Detach(&smallest_, map_);
    next->Add(temp);
  } else if (bucket->next_ == bucket) {
    bucket->parent_->value_ = count;
  } else {
    Child* temp = bucket->next_;
    bucket->Detach(&smallest_, map_);
    bucket = temp;
    Parent* p = new Parent();
    p->left_ = bucket->parent_;
    p->value_ = count;
    bucket->parent_->right_ = p;
    if (next != NULL) {
      p->right_ = next;
      next->left_ = p;
    } else {
      largest_ = p;
    }
    p->Add(bucket);
  }
}

void SpaceSaving::Process(const string element) {
  int index;
  if (!map_->Find(element, &index)) {
    Child* bucket = smallest_->child_;
    map_->Remove(bucket->element_);
    bucket->element_ = element;
    map_->Insert(element, bucket);
    Increment(bucket);
  } else {
    Increment(map_->values_[index]);
  }
}
