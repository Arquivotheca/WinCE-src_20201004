//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "parse.h"
#include "token.h"

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
bool GetNextLine(istream &file, string &line){
  static int lcount = 0;
  static int comment = false;

  line = "";
  int indx;
  
  while(line.empty() && !file.eof()){
    getline(file, line, '\n');
    lcount++;

    // check for single line comments
    line = line.substr(0, line.find(";"));  // kill ";"  style comments
    line = line.substr(0, line.find("//")); // kill "//" style comments
  
    // check for end of multiline comment
    if(comment){
      indx = line.find("*/");

      // not found, ignore this line and get a new one
      if(indx == string::npos){
        line.erase();
        continue;
      }

      // kill the comment part and continue processing
      line.erase(0, indx + 2);
      comment = false;
    }

    // check for start of multiline comment
    indx = line.find("/*");
    if(indx != string::npos){
      int indx2 = 0;

      // check to see if it ends on the same line
      indx2 = line.find("*/", indx);
      if(indx2 != string::npos) // yup, just erase the substr
        line.erase(indx, (indx2 + 2) - indx);
      else{ // nope, kill the end of this line and look for it on future lines
        line = line.substr(0, indx); // kill "/*" style comments
        comment = true;
      }
    }

    replace(line.begin(), line.end(), '\t', ' '); // kill tabs
    line.erase(0, line.find_first_not_of(" "));   // kill leading white space (also takes care of blank lines)
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
string ExpandEnvironmentVariables(string line){
  int open, close;
  char *envvar; // has to be a char pointer for good reason
  
  while((open = line.find("$(")) != string::npos){
    if((close = line.find(")", open)) == string::npos){
      cerr << "Error: Missig ')' on line '" << line << "'\n";
      break;    
    }

    // check to see if this is an assignment
    if(0 && line.substr(close + 1, 2) == "=\""){
      string env_var = line.substr(open + 2, close - (open + 2));

      open = close + 3;
      if((close = line.find("\"", open)) == string::npos){
        cerr << "Error: Missing close \" on line '" << line << "'\n";
        break;    
      }

      env_var += "=" + line.substr(open, close - open);

      _putenv(env_var.c_str());

      line = "";
    }
    else{
      // get the env setting if any
      envvar = getenv(line.substr(open + 2, close - (open + 2)).c_str());
      
      // remove the embeded variable
      line.erase(open, close - open + 1);
  
      // see if we should insert a value
      if(!envvar) envvar = "false";
        
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
string FudgeIfs(string line){
  // see if they $'d the vars already
  if(line.find('$') != string::npos)
    return line;
  
  StringList temp = Token::tokenize(line);

  if(!temp.empty() && temp[0] == "if"){
    if(temp.size() < 2){
      cerr << "\nError: invalid IF statement found:" << endl << line << endl;
      exit(1);
    }

    // form new line
    line.erase();
    temp[1] = " $(" + temp[1] + ") ";

    for(StringList::iterator itr = temp.begin(); itr != temp.end(); itr++){
      if(itr + 1 != temp.end() && *(itr + 1) == "!")
        itr->swap(*(itr +1));
      else if(*itr == "=")
        *itr = "==";
      
      line += *itr + " ";
    }
  }

  return line;
}

/*****************************************************************************
  SkipLine()

  in:
    StringList token_list - list of tokens comprising a line
    
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
      cout << *start << " "; \
    cout << endl;
#endif

bool evaluate_expr(StringList token_list){
  StringList::iterator start;
  StringList::iterator end;

  if(count(token_list.begin(), token_list.end(), "(") != count(token_list.begin(), token_list.end(), ")")){
    for(start = token_list.begin(); start != token_list.end(); start++)
      cerr << *start << " "; 
    cerr << "\nError: parse error in IF condition - missmatched parenthesis\n";
    exit(1);
  }

  if(!token_list.empty() && token_list.front() == "if")
    token_list.erase(token_list.begin());

  DEBUG_EXPR();
  
  // handle all ()'s and subexpressions
  while((start = find(token_list.begin(), token_list.end(), "(")) != token_list.end()){
    StringList::iterator temp;

    temp = end = start;
    
    do{
      if((end = find(end + 1, token_list.end(), ")")) == token_list.end()){
        for(start = token_list.begin(); start != token_list.end(); start++)
          cerr << *start << " "; 
        cerr << "\nError: parse error in IF condition\n";
        exit(1);
      }
    }while((temp = find(temp + 1, end, "(")) != end);

    bool subexpr = evaluate_expr(StringList(start + 1, end));

    start = token_list.erase(start, end + 1);
    token_list.insert(start, subexpr ? "true" : "false");
  }

  DEBUG_EXPR();

  // handle all !'s
  while((start = find(token_list.begin(), token_list.end(), "!")) != token_list.end()){
    *start = *(start + 1) == "false" ? "true" : "false";
    token_list.erase(start + 1);
  }

  DEBUG_EXPR();

  // handle all =='s and !='s
  while((start = find(token_list.begin(), token_list.end(), "==")) != token_list.end()){
    *(start - 1) = *(start - 1) == *(start + 1) ? "true" : "false";
    token_list.erase(start, start + 2);
  }
  
  DEBUG_EXPR();

  while((start = find(token_list.begin(), token_list.end(), "!=")) != token_list.end()){
    *(start - 1) = *(start - 1) != *(start + 1) ? "true" : "false";
    token_list.erase(start, start + 2);
  }

  DEBUG_EXPR();

  while((start = find(token_list.begin(), token_list.end(), "&&")) != token_list.end()){
    *(start - 1) = *(start - 1) != "false" && *(start + 1) != "false" ? "true" : "false";
    token_list.erase(start, start + 2);
  }

  DEBUG_EXPR();

  while((start = find(token_list.begin(), token_list.end(), "||")) != token_list.end()){
    *(start - 1) = *(start - 1) != "false" || *(start + 1) != "false" ? "true" : "false";
    token_list.erase(start, start + 2);
  }

  DEBUG_EXPR();

  return token_list[0] != "false";
}

bool SkipLine(const StringList &token_list){
  static int ignore_level = 0;
  int last_level = ignore_level;

  

  if(token_list.empty())
    return ignore_level;

  if(token_list[0] == "if"){
    if(ignore_level || !evaluate_expr(token_list))
      ignore_level++;
    return true;
  }
    
  if(token_list[0] == "else"){
    if(ignore_level == 0)
      ignore_level = 1;
    else if(ignore_level == 1)
      ignore_level = 0;
    return true;
  }

  if(token_list[0] == "endif"){
    if(ignore_level)
      ignore_level--;
    return true;
  }

  return ignore_level;
}

/*****************************************************************************
  IsDuplicate()

  in:
    class container - any container class that supports begin(), and end() 
                      members
    class object    - object type stored in the container
    
  out:
    boolean indicating duplicate object
    
  desc:
    determins if the object already exists in the list.  Used before insertion
    to prevent duping data objects.
******************************************************************************/
template<class container, class object>
bool IsDuplicate(const container &cont, const object &item){
  if(find(cont.begin(), cont.end(), item) == cont.end())
    return false;

  cerr << "Warning: Found duplicate entry '" << item << "' ... skipping\n";
  return true;
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
bool IsSection(const string &token){
  if(token == MODULES ||
     token == MEMORY  ||
     token == FILES   ||
     token == CONFIG  )
    return true;

  return false;
}

// do this because passing five params to a stl for_each callback is a PITA!
static MemoryList *pmemory_list;
static MemoryList *preserve_list;
static ModuleList *pmodule_list;
static FileList   *pfile_list;
static Config     *pconfig;

/*****************************************************************************
  ProcessBIBLine()

  in:
    StringList token_list - tokenized line
    
  out:
    none

  desc:
    checks the line for section heading, and then parses all line into the 
    apropriate sections.
******************************************************************************/
void ProcessError(const StringList &slist){
  for(int i = 0; i < slist.size(); i++)
    cerr << slist[i] << " ";
  cerr << endl;

  cerr << "\nError: failed setting line\n";
  
  exit(1);
}

void ProcessBIBLine(const StringList &token_list){
  static string section = "";

  if(token_list.empty())
    return;

  if(IsSection(token_list[0]))
    section = token_list[0];
  else if(section.empty())
    cerr << "Warning: found '" << token_list[0] << "' before a section header\n";

  else if(section == CONFIG){
    if(!pconfig->set(token_list, *pmemory_list)) 
      ProcessError(token_list);
  }
  else if(section == FILES){
    File file;
    Module module;

    if(!file.set(token_list, *pmemory_list, pconfig->compressor_list)) 
      ProcessError(token_list);

    if(file.is_file()){
      if(!IsDuplicate(*pmodule_list, file) && !IsDuplicate(*pfile_list, file))
        pfile_list->push_back(file);
    }
    else{
      if(!module.set(token_list, *pmemory_list, pconfig->compressor_list)) 
        ProcessError(token_list);

      if(!IsDuplicate(*pmodule_list, module) && !IsDuplicate(*pfile_list, module))
        pmodule_list->push_back(module);
    }
  }
  else if(section == MODULES){
    File file;
    Module module;

    if(!module.set(token_list, *pmemory_list, pconfig->compressor_list))
      ProcessError(token_list);

    if(module.is_module()){
      if(!IsDuplicate(*pmodule_list, module) && !IsDuplicate(*pfile_list, module))
        pmodule_list->push_back(module);
    }
    else{
      if(!file.set(token_list, *pmemory_list, pconfig->compressor_list)) 
        ProcessError(token_list);
      
      if(!IsDuplicate(*pmodule_list, file) && !IsDuplicate(*pfile_list, file))
        pfile_list->push_back(file);
    }
  }
  else if(section == MEMORY){
    Memory memory;

    if(!memory.set(token_list))
      ProcessError(token_list);

    if(memory.type() == RESERVED_TYPE)
      preserve_list->push_back(memory);
    else if(!IsDuplicate(*pmemory_list, memory))
      pmemory_list->push_back(memory);
  }
  else
    cerr << "Internal Error: " << section << " not processed yet!\n";
}

/*****************************************************************************
  ParseBIBFile()

  in:
    string file_name - name of .bib file to parse
    ...
    
  out:
    boolean indicating success

  desc:
    retrieves and entire file as a vector of lines and passes it off to other
    routines for processing
******************************************************************************/
bool ParseBIBFile(const string &file_name, 
                MemoryList &memory_list, 
                MemoryList &reserve_list, 
                ModuleList &module_list, 
                FileList   &file_list,
                Config     &config){

  // do this because passing five params to a stl for_each callback is a PITA!
  pmemory_list = &memory_list;
  preserve_list = &reserve_list;
  pmodule_list = &module_list;
  pfile_list = &file_list;
  pconfig = &config;
  
  StringList input_listing;
  vector<StringList> token_listing;
  string line;

  if(GetFileAttributes(file_name.c_str()) == -1){
    cerr << "Error: Could not find file '" << file_name << "'\n";
    return false;
  }

  ifstream file(file_name.c_str(), ios::in);

  if(file.bad()){
    cerr << "Error: Could not open '" << file_name << "' for reading\n";
    return false;
  }
  
  while(GetNextLine(file, line))
    input_listing.push_back(line);
  
  file.close();

  transform(input_listing.begin(), input_listing.end(), input_listing.begin(), FudgeIfs);
  transform(input_listing.begin(), input_listing.end(), input_listing.begin(), ExpandEnvironmentVariables);
  transform(input_listing.begin(), input_listing.end(), back_inserter(token_listing), Token::tokenize);

  // Note: remove_if() is querky, look it up and make SURE you know how it works before touching this!
  token_listing.erase(remove_if(token_listing.begin(), token_listing.end(), SkipLine), token_listing.end());
  
  // if we are still ignoring anything then there was a missmatch
  if(SkipLine(StringList())){
    cerr << "Error: Unmatched IF/ELSE/ENDIF block... somewhere :(\n";  
    return false;
  }
  
  for_each(token_listing.begin(), token_listing.end(), ProcessBIBLine);

  return config.sanity_check(memory_list);
}

