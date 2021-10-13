//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef INCLUDED_DATA_H
#define INCLUDED_DATA_H

#include <iostream>

using namespace std;

/*****************************************************************************
  Data

  desc:
    Similar to the string class but for arbitrary binary data.  It will clean
    itself up during destruction and has overloaded the copy constructor and 
    assignment operator for deap copies.
******************************************************************************/
class Data{
private:
  void init();
  
  unsigned char *m_ptr;
  unsigned long m_size;
  unsigned long m_capacity;
  
  unsigned long m_reallocs;

  bool m_allow_shrink;

public:
  Data();
  Data(unsigned long size);
  Data(const void *data, unsigned long size);
  Data(const Data &data);
  ~Data();

  unsigned char *ptr(unsigned long offset = 0) const{ return m_ptr + offset; }
  bool empty() const{ return !m_size; }
  unsigned long size() const{ return m_size; }
  unsigned long capacity() const{ return m_capacity; }

  void clear();
  void truncate(unsigned long reduced_size);

  /* DANGER  DANGER  DANGER */
  unsigned char *user_fill(unsigned long size);  // if user runs off this, SHOOT THEM IN THE HEAD!

  void allow_shrink(bool flag){ m_allow_shrink = flag; }
  bool fix_size(unsigned long size);
  bool reserve(unsigned long size);

  bool set(unsigned long offset, const Data &data);
  bool set(const Data &data);

  bool set(unsigned long offset, const void *data, unsigned long size);
  bool set(const void *data, unsigned long size);

  bool append(const void *data, unsigned long size);
  bool append(const Data &data);
  
  Data operator=(const Data &data);
  Data operator+=(const Data &data);
//  char &operator[](const unsigned int i){ return m_ptr[i]; } // not sure if I want to expose this yet
};

/*****************************************************************************/
inline bool operator==(const Data &d1, const Data &d2){
  return d1.size() == d2.size() && memcmp(d1.ptr(), d2.ptr(), d1.size()) == 0;
}

ostream &operator<<(ostream &out_file, const Data &data);

#endif
