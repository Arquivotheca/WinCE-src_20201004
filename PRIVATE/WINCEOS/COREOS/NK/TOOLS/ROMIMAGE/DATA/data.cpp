//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "data.h"

/*****************************************************************************
  Data()

  in:
    varies
    
  out:
    none

  desc:
    various ways of constructing the object for user convinience
******************************************************************************/
void Data::init(){
  m_ptr = NULL;
  m_size = m_capacity = 0;
  m_reallocs = 0;
  m_allow_shrink = true;
}

Data::Data(){
  init();
}

Data::Data(unsigned long size){
  init();

  reserve(size);
}

Data::Data(const void *data, unsigned long size){ 
  init();

  set(data, size); 
}

Data::Data(const Data &data){ 
  init();

  set(data.m_ptr, data.m_size);
}

/*****************************************************************************
  ~Data()

  in:
    none
    
  out:
    none

  desc:
    now we remove the data for real
******************************************************************************/
Data::~Data(){
  if(m_ptr) delete[] m_ptr; 
}

/*****************************************************************************
  clear()

  in:
    none
    
  out:
    none - can't fail

  desc:
    user thinks they are clearing the buffer but we really just set the size
    to zero.  This speeds things up by not having to delete the data this time
    and not having to reallocate data next time.
******************************************************************************/
void Data::clear(){
  m_size = 0;
}

/*****************************************************************************
DANGER  DANGER  DANGER   DANGER  DANGER  DANGER   DANGER  DANGER  DANGER   

user_fill()

  in:
    unsigned long size - allocates (size) bytes of data and returns the user a 
                         writeable pointer to the data
    
  out:
    char * - pointer to data space allocated

  desc:
    this is so the the user can copy data into the class on their own.  This 
    is VERY dangerous because if the user overwrites the buffer we are all 
    screwed.  If they do overwrite the buffer, I reserve the right to take 
    apropriate recourse of my choosing, which may include (but not is not 
    limited to): hanging, drawn and quartering, and burning at the stake (or
    any combinate of them I want).

DANGER  DANGER  DANGER   DANGER  DANGER  DANGER   DANGER  DANGER  DANGER   
******************************************************************************/
unsigned char *Data::user_fill(unsigned long size){
  if(!reserve(size))
    return NULL;

  m_size = size; // assume that they are going to use the whole thing!

  return m_ptr;
}    
  
/*****************************************************************************
  reserve()

  in:
    unsigned long size - number of bytes of memory to reseve
    
  out:
    boolean value indicating success

  desc:
    allows the user to prereserve (size) amount of memory in buffer.  This is 
    usefull for both speed (to reduce the number of reallocs), and for the 
    user_fill() function to give the user the requested amount of space, along
    with keeping all memory allocs in one function for the rest of the class.
******************************************************************************/
#define max(a, b) ((a) > (b) ? (a) : (b)) 
#define min(a, b) ((a) < (b) ? (a) : (b)) 

bool Data::reserve(unsigned long size){
  // if the requested size is less than or equal to the current capacity and 
  // greater than half the current capacity, then
  // just use the memory we already have (used as a performance mod)
  if(size <= m_capacity)
    if(!m_allow_shrink || (size >= m_capacity / 2 && m_capacity - size < 1024 * 500)) 
      return true;

  // another performance mod to reduce frequent copies when we are appending 
  // data repeatedly to the instance
  if(size > m_capacity && size - m_capacity < 1024 * 16) size += m_reallocs++ * (size - m_capacity);

  unsigned char *temp = new unsigned char[size];
  if(!temp){
    cerr << "Error: Failed to allocate memory in Data object\n";
    return false;
  }

  memset(temp, 0, size);

  m_capacity = size;

  // if they have data and are just changing the future capacity we
  // need to dupe the current data into the newly allocated buffer and
  // delete the old buffer
  // * only use the lesser of the two sizes incase we shrunk the capacity
  if(m_ptr){
    memcpy(temp, m_ptr, min(size, m_size));
    delete[] m_ptr;
  }

  m_ptr = temp;
  
  return true;
}

/*****************************************************************************
  set()

  in:
    unsgined long offset - offset in the buffer to write data to
    char *data           - pointer to binary data
    unsigned long size   - number of bytes in data
    
  out:
    boolean value indicating success

  desc:
    allocates memory and copies the data internally
******************************************************************************/
bool Data::set(unsigned long offset, const void *data, unsigned long size){
  // want to make sure if someone writes a small bit of data to the beginning 
  // of a buffer that it won't shrink the buffer in reserve().
  if(!reserve(max(size + offset, m_size)))
    return false;

  memcpy(m_ptr + offset, data, size);

  m_size = max(size + offset, m_size);
  
  return true;
}

bool Data::set(unsigned long offset, const Data &data){
  return set(offset, data.m_ptr, data.m_size);
}

// this could call set(offset, data, size) with an offset of zero
// but this is a little more memory efficient because it allows 
// reserve() to resize the memory usage smaller if appropriate
bool Data::set(const void *data, unsigned long size){
  if(!reserve(size))
    return false;

  memcpy(m_ptr, data, size);

  m_size = size;
  
  return true;
}

bool Data::set(const Data &data){
  return set(data.m_ptr, data.m_size);
}

/*****************************************************************************
  append()

  in:
    char *data         - pointer to binary data
    unsigned long size - number of bytes in data
    
  out:
    boolean value indicating success

  desc:
    allocates memory and appends the data to the existing data
******************************************************************************/
bool Data::append(const void *data, unsigned long size){
  return set(m_size, data, size);
}

bool Data::append(const Data &data){
  return set(m_size, data);
}

/*****************************************************************************
  operator=()

  in:
    Data &data - Data instance to copy
    
  out:
    Data - standard form for operator=()

  desc:
    copies the opject passed in to the current object
******************************************************************************/
Data Data::operator=(const Data &data){
  if(this != &data) 
    set(data);

  return *this;
}

/*****************************************************************************
  operator+=()

  in:
    Data &data - Data instance to copy
    
  out:
    Data - standard form for operator=()

  desc:
    copies the opject passed in to the current object
******************************************************************************/
Data Data::operator+=(const Data &data){
  append(data);

  return *this;
}

/*****************************************************************************
  output streams for class Data
******************************************************************************/
ostream &operator<<(ostream &out_file, const Data &data){
  int i = data.size();
  unsigned char *ptr = data.ptr();

  while(i--)
    out_file << *ptr++;

  return out_file;
}

