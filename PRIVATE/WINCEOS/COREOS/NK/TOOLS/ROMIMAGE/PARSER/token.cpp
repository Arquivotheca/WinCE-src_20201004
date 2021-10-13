//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "token.h"

/*****************************************************************************
  DoLongFileNameFudge()

  in:
    char ch - char to check
    
  out:
    char - converted char for replacement

  desc:
    used to process the tokens one character at a time.  stores the state of 
    whether or not inside of quotes and if so, changes spaces into '/' so that 
    the tokenizer wont see it as a delimiter
******************************************************************************/
wchar_t Token::DoLongFileNameFudge(wchar_t ch){
  static bool in_quotes = false;

  if(ch == '\"'){
    in_quotes = !in_quotes;
    return ' ';
  }

  if(in_quotes && ch == ' ')
    return '/';

  return ch;
}

/*****************************************************************************
  DoLongFileNameFudge()

  in:
    char ch - char to check
    
  out:
    char - converted char for replacement

  desc:
    fixes what DoLongFileNameFudge() did to the string
******************************************************************************/
wchar_t Token::UnDoLongFileNameFudge(wchar_t ch){
  if(ch == '/')
    return ' ';

  return ch;
}

int Token::CheckQuotes(wstring line, bool check){
  static int last = 0;
  static int lines = 0;

  int count = 0;

  lines++;

  if(check){
    count = last;

    if(count){
      fprintf(stderr, "Error: Missmatched quotes found, possibly on line %d\n", last);
    }

    last = 0;
    lines = 0;

    return count;
  }    
  
  for(unsigned int i = 0; i < line.length(); i++){
    if(line[i] == '\"'){
      count++;
    }
  }

  if(count & 1){
    last = lines;
  }

  return last;
}

/*****************************************************************************
  tokenize()

  in:
    string line - line to be tokenized
    
  out:
    vector<string> - vector of tokens from the line

  desc:
    tokenizes a line watching out for quoted strings

    NOTE: 
    any operators encountered are used as delimiters AND are returned in 
    their respective spots in the token list unlike white space delimiters
******************************************************************************/
vector<wstring> Token::tokenize(wstring line){
  vector<wstring> token_list;

  transform(line.begin(), line.end(), line.begin(), Token::DoLongFileNameFudge);

  while(!line.empty()){
//    string chars = ".\\/:_-~{}[]";
    wstring chars = L" \a\n\r\t=\"'";
    wstring token;

    if(line.substr(0, 2) ==  L"==" || 
       line.substr(0, 2) ==  L"!=" ||
       line.substr(0, 2) ==  L"&&" ||
       line.substr(0, 2) ==  L"||"){
      token = line.substr(0, 2);
      line.erase(0, 2);
    }
    else if(line[0] == '=' ||
            line[0] == '!' ||
            line[0] == '(' || 
            line[0] == ')'){
      token += line[0];
      line.erase(0, 1);
    }
    else if(chars.find(line[0]) == wstring::npos){  //isalnum(line[0]) || chars.find(line[0]) != string::npos){
      while(!line.empty() && chars.find(line[0]) == wstring::npos){  //isalnum(line[0]) || chars.find(line[0]) != string::npos){
        token += line[0];
        line.erase(0, 1);
      }
    }
    else{
      if(line[0] != ' ' && line[0] != '\t')
      {
        printf("ignoring '%C' (0x%08x)\n", line[0], line[0]);
      }
      
      line.erase(0, 1);
    }

    if(!token.empty()){
      transform(token.begin(), token.end(), token.begin(), Token::UnDoLongFileNameFudge);
      token_list.push_back(token);        // add the token to the list
    }
  }

  return token_list;
}

/*****************************************************************************
  get_token()

  in:
    string line         - line to be tokenized
    unsigned long begin - begining index for tokenizing
    string delim        - delimiters
    
  out:
    string - vector of tokens from the line

  desc:
    gets a specific token from a line watching out for quoted strings
******************************************************************************/
string Token::get_token(string line, unsigned long begin, string delim){
  line = line.substr(begin, string::npos);

  int i = lowercase(line).find_first_of(lowercase(delim));
  
  line = line.substr(0, i);

  return line;
}

/*****************************************************************************
  get_bool_value()

  in:
    string token  - should be a ascii representation of a bool value
    string &value - default value for default return
    bool ret      - default return for default value
    
  out:
    boolean value of token

  desc:
    lookes for an expression "=value"
    returns  ret if found
    returns !ret if not found
******************************************************************************/
bool Token::get_bool_value(const string &token, const string &value /* = "on" */, bool ret /* = true */){
  return token == value ? ret : !ret;
}

/*****************************************************************************
  get_hex_value()

  in:
    string token  - should be a ascii hex representation of a unsigned long value
    
  out:
    unsigned long value of hex string
    
  desc:
    looks for a hex string "=xxxxxxx" and returns the unsigned long value
******************************************************************************/
unsigned long Token::get_hex_value(const string &token){
  unsigned long ret = 0;
  char *ptr;

  ret = strtoul(token.c_str(), &ptr, 16);

  if(*ptr != NULL)
    cerr << "Warning: possible strtoul error in get_hex_value(" << token << ")\n";
  
  return ret;
}

/*****************************************************************************
  get_dec_value()

  in:
    string token  - should be a ascii dec representation of a unsigned long value
    
  out:
    unsigned long value of dec string
    
  desc:
    looks for a dec string "=dddddddd" and returns the unsigned long value
******************************************************************************/
unsigned long Token::get_dec_value(const string &token){
  unsigned long ret = 0;
  char *ptr;

  ret = strtoul(token.c_str(), &ptr, 10);

  if(*ptr != NULL)
    cerr << "Warning: possible strtoul error in get_dec_value(" << token << ")\n";
  
  return ret;
}

