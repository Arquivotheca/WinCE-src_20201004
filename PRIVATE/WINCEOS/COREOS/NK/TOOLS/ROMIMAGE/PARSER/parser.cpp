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
#include "parser.h"
#include "token.h"

/*****************************************************************************
  wgetline()

  in:
    istream &file - input source to read lines from
    string &line  - location for output line (only valid if return is true)
    wchar_t delim - line delimiter
    
  out:
    boolean value indicating success

  desc:
    gets the next line as a string from the file, handling all comment removal
    so that the calling function gets nothing but valid return data
******************************************************************************/
bool mygetline(FILE *file, string &line, char delim /* = '\n' */)
{
  line = "";
  
  char *ptr;
  char s[1024] = {0};
  ptr = fgets(s, sizeof(s)/sizeof(s[0]), file);

  if(feof(file) || !ptr){
    return true;
  }

  line = s;

  if(line.length() && line[line.length() - 1] == '\n') line.erase(line.length() - 1, 1);
  if(line.length() && line[line.length() - 1] == '\r') line.erase(line.length() - 1, 1);
  if(line.length() && line[line.length() - 1] == '\n') line.erase(line.length() - 1, 1);

  return true;
}

bool mygetline(FILE *file, wstring &line, wchar_t delim /* = '\n' */)
{
  static bool first = true;
  line = L"";
  
  wchar_t *ptr;
  wchar_t s[1024] = {0};
  ptr = fgetws(s, sizeof(s)/sizeof(s[0]), file);

  if(feof(file) || !ptr){
    return true;
  }

  line = s;

  if(first){
    if(line[0] == 0xfffe){
      line.erase(0, 1);
    }

    first = false;
  }

  if(line.length() && line[line.length() - 1] == '\n') line.erase(line.length() - 1, 1);
  if(line.length() && line[line.length() - 1] == '\r') line.erase(line.length() - 1, 1);
  if(line.length() && line[line.length() - 1] == '\n') line.erase(line.length() - 1, 1);

  return true;
}

/*****************************************************************************
  GetNextLine()

  in:
    istream &file - input source to read lines from
    string &line  - location for output line (only valid if return is true)
    
  out:
    boolean value indicating success

  desc:
    gets the next line as a string from the file, handling all comment removal
    so that the calling function gets nothing but valid return data
******************************************************************************/
bool GetNextLine(FILE *file, wstring &line){
  static int lcount = 0;
  static int comment = false;

  line = L"";
  int indx;
  
  while(line.empty() && !feof(file)){
    mygetline(file, line, '\n');

    lcount++;

    // check for single line comments
    line = line.substr(0, line.find(L";"));  // kill ";"  style comments
    line = line.substr(0, line.find(L"//")); // kill "//" style comments
  
    // check for end of multiline comment
    if(comment){
      indx = line.find(L"*/");

      // not found, ignore this line and get a new one
      if(indx == wstring::npos){
        line.erase();
        continue;
      }

      // kill the comment part and continue processing
      line.erase(0, indx + 2);
      comment = false;
    }

    // check for start of multiline comment
    indx = line.find(L"/*");
    if(indx != wstring::npos){
      int indx2 = 0;

      // check to see if it ends on the same line
      indx2 = line.find(L"*/", indx);
      if(indx2 != wstring::npos) // yup, just erase the substr
        line.erase(indx, (indx2 + 2) - indx);
      else{ // nope, kill the end of this line and look for it on future lines
        line = line.substr(0, indx); // kill "/*" style comments
        comment = true;
      }
    }

    replace(line.begin(), line.end(), wchar_t('\t'), wchar_t(' ')); // kill tabs
    line.erase(0, line.find_first_not_of(L" "));   // kill leading white space (also takes care of blank lines)

    Token::CheckQuotes(line);
  }

  return !line.empty();
}

/*****************************************************************************
  ExpandEnvironmentVariables()

  in:
    string line - line to check for variables
    
  out:
    string - converted line (may be unchanged if no env vars were found)

  desc:
    searches for the pattern $(variable name) in the environment and replaces
    the reference in the string with the value.  If not found the reference
    in the string is still removed.
******************************************************************************/
wstring ExpandEnvironmentVariables(wstring line){
  int open, close;
  wchar_t *envvar; // has to be a char pointer for good reason
  
  while((open = line.find(L"$(")) != wstring::npos){
    if((close = line.find(L")", open)) == wstring::npos){
      wcerr << L"Error: Missig ')' on line '" << line << L"'\n";
      break;    
    }

    // check to see if this is an assignment
    if(0 && line.substr(close + 1, 2) == L"=\""){
      wstring env_var = line.substr(open + 2, close - (open + 2));

      open = close + 3;
      if((close = line.find(L"\"", open)) == wstring::npos){
        wcerr << L"Error: Missing close \" on line '" << line << L"'\n";
        break;    
      }

      env_var += L"=" + line.substr(open, close - open);

      _wputenv(env_var.c_str());

      line = L"";
    }
    else{
      // get the env setting if any
      envvar = _wgetenv(line.substr(open + 2, close - (open + 2)).c_str());
      
      // remove the embeded variable
      line.erase(open, close - open + 1);
  
      // see if we should insert a value
      if(!envvar) envvar = L"false";
        
      line.insert(open, envvar);
    }
  }

  return line;
}

/*****************************************************************************
  FudgeIfs()

  in:
    string line - line to check for if statement and fudge

  out:
    string - fudged line

  desc:  
    standardize all if statemets to the following form...

    basically just putting the env vars in $()'s for concistancy    
    if var=val -> if $(var)==val
    if var     -> if $(var)
    if var !   -> if ! $(var)
******************************************************************************/
wstring FudgeIfs(wstring line){
  // see if they $'d the vars already
  if(line.find(L"$") != wstring::npos)
    return line;
  
  wStringList temp = Token::tokenize(line);

  if(!temp.empty() && temp[0] == L"if"){
    if(temp.size() < 2){
      cerr << "\nError: invalid IF statement found:";
      wcerr << endl << line << endl;
      exit(1);
    }

    // form new line
    line.erase();
    temp[1] = L" $(" + temp[1] + L") ";

    for(wStringList::iterator itr = temp.begin(); itr != temp.end(); itr++){
      if(itr + 1 != temp.end() && *(itr + 1) == L"!")
        itr->swap(*(itr +1));
      else if(*itr == L"=")
        *itr = L"==";
      
      line += *itr;
      line += L" ";
    }
  }

  return line;
}

/*****************************************************************************
  SkipLine()

  in:
    wStringList token_list - list of tokens comprising a line
    
  out:
    boolean value indicating weather this line should be skipped, ie. true 
    return means the line evaluated to be a false conditional and can be 
    skipped.

  desc:
    Evaluating conditionals
  
    'if' statments
    --------------
      If we are already ignoring, there is no decision, just increment 
      ignore_level again.  Otherwise we check the conditional as follows
     
      (remembering that undefined variables were removed in 
       ExpandEnvironmentVariables())

      The defined syntax allows six possible combinations:
      "if $(var) = val" - 4 token  - env var defined     = false if(var != val)
      "if        = val" - 3 token  - env var not defined = false
      "if $(var)      " - 2 tokens - env var defined     = true
      "if             " - 1 tokens - env var not defined = false
      "if $(var)     !" - 3 tokens - env var defined     = !(true)  -> false
      "if            !" - 2 tokens - env var not defined = !(false) -> true

      "if (exp) && (expr)
  
      therefore if the syntax is correct one can use the # of tokens 
      (1 or 3 or (4 with a compare)) to determin the 'falseness' of the statement 
      -> 1 token or 3 tokens or (4 tokens and var != val) then increment ignore level
  
    'else' statements
    -----------------
       if we are NOT ignoring anything, 
         then ignore the 'else' block by assigning ignore_level = 1
         
       if we are ignoring only ONE level deep, 
         then do the else by assigning ignore_level = 0
         
       otherwise we are ignoring more than one level and the else makes no 
         difference what so ever!
       
    'endif' statements
    ------------------
       if we are ignoring anything, 
         then decriment ignore_level on an endif
******************************************************************************/

#define DEBUG_EXPR() 0

#if DEBUG_EXPR() == 1
#define DEBUG_EXPR() \
    for(start = token_list.begin(); start != token_list.end(); start++) \
      wcout << wstring(*start) << L" "; \
    cout << endl;
#endif

bool evaluate_expr(wStringList token_list){
  wStringList::iterator start;
  wStringList::iterator end;

  if(count(token_list.begin(), token_list.end(), L"(") != count(token_list.begin(), token_list.end(), L")")){
    for(start = token_list.begin(); start != token_list.end(); start++)
      wcerr << *start << L" "; 
    cerr << "\nError: parse error in IF condition - missmatched parenthesis\n";
    exit(1);
  }

  if(!token_list.empty() && token_list.front() == L"if")
    token_list.erase(token_list.begin());

  DEBUG_EXPR();
  
  // handle all ()'s and subexpressions
  while((start = find(token_list.begin(), token_list.end(), L"(")) != token_list.end()){
    wStringList::iterator temp;

    temp = end = start;
    
    do{
      if((end = find(end + 1, token_list.end(), L")")) == token_list.end()){
        for(start = token_list.begin(); start != token_list.end(); start++)
          wcerr << *start << L" "; 
        cerr << "\nError: parse error in IF condition\n";
        exit(1);
      }
    }while((temp = find(temp + 1, end, L"(")) != end);

    bool subexpr = evaluate_expr(wStringList(start + 1, end));

    start = token_list.erase(start, end + 1);
    token_list.insert(start, subexpr ? L"true" : L"false");
  }

  DEBUG_EXPR();

  // handle all !'s
  while((start = find(token_list.begin(), token_list.end(), L"!")) != token_list.end()){
    *start = *(start + 1) == L"false" ? L"true" : L"false";
    token_list.erase(start + 1);
  }

  DEBUG_EXPR();

  // handle all =='s and !='s
  while((start = find(token_list.begin(), token_list.end(), L"==")) != token_list.end()){
    *(start - 1) = *(start - 1) == *(start + 1) ? L"true" : L"false";
    token_list.erase(start, start + 2);
  }
  
  DEBUG_EXPR();

  while((start = find(token_list.begin(), token_list.end(), L"!=")) != token_list.end()){
    *(start - 1) = *(start - 1) != *(start + 1) ? L"true" : L"false";
    token_list.erase(start, start + 2);
  }

  DEBUG_EXPR();

  while((start = find(token_list.begin(), token_list.end(), L"&&")) != token_list.end()){
    *(start - 1) = *(start - 1) != L"false" && *(start + 1) != L"false" ? L"true" : L"false";
    token_list.erase(start, start + 2);
  }

  DEBUG_EXPR();

  while((start = find(token_list.begin(), token_list.end(), L"||")) != token_list.end()){
    *(start - 1) = *(start - 1) != L"false" || *(start + 1) != L"false" ? L"true" : L"false";
    token_list.erase(start, start + 2);
  }

  DEBUG_EXPR();

  return token_list[0] != L"false";
}

bool SkipLine(const wStringList &token_list){
  static int ignore_level = 0;
  int last_level = ignore_level;

  // bugbug: maybe check the syntax a little bit better :)

  if(token_list.empty())
    return ignore_level;

  if(token_list[0] == L"if"){
    if(ignore_level || !evaluate_expr(token_list))
      ignore_level++;
    return true;
  }
    
  if(token_list[0] == L"else"){
    if(ignore_level == 0)
      ignore_level = 1;
    else if(ignore_level == 1)
      ignore_level = 0;
    return true;
  }

  if(token_list[0] == L"endif"){
    if(ignore_level)
      ignore_level--;
    return true;
  }

  return ignore_level;
}

/*****************************************************************************
  IsSection()

  in:
    string token - token to check
    
  out:
    boolean value indicating match

  desc:
    checks token to see if it matches one of the predefined section names
******************************************************************************/
bool IsSection(const wstring &token){
  if(token == WMODULES ||
     token == WMEMORY  ||
     token == WFILES   ||
     token == WCONFIG  )
    return true;

  return false;
}

