// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "ruletable_algo.h"

// for case-insensitive string comparison
#include <string.h>
#ifndef WIN32
   #define stricmp strcasecmp
   #define strnicmp strncasecmp
#endif

#include <algorithm>
#include <map>
#include <sstream>
using namespace std;

const string ruletable_algo::neighborhood_value_keywords[N_SUPPORTED_NEIGHBORHOODS] = 
                    {"vonNeumann","Moore","hexagonal","oneDimensional"};
// (keep in sync with TNeighborhood)

bool ruletable_algo::IsDefaultRule(const char* rulename)
{
    return (strcmp(rulename, DefaultRule()) == 0);
}

static FILE* static_rulefile = NULL;
static int static_lineno = 0;
static char static_endchar = 0;
static string result;

const char* ruletable_algo::LoadTable(FILE* rulefile, int lineno, char endchar, const char* s)
{
    // set static vars so LoadRuleTable will load table data from .rule file
    static_rulefile = rulefile;
    static_lineno = lineno;
    static_endchar = endchar;
    const char* err = setrule(s);   // calls LoadRuleTable
    static_rulefile = NULL;
    static_lineno = 0;
    static_endchar = 0;
    return err;
}

int ruletable_algo::NumCellStates()
{
   return this->n_states;
}

bool starts_with(const string& line, const string& keyword)
{
   return strnicmp(line.c_str(), keyword.c_str(), keyword.length()) == 0;
}

const char* ruletable_algo::setrule(const char* s)
{
   const char *colonptr = strchr(s, ':');
   string rule_name(s);
   if (colonptr) 
      rule_name.assign(s,colonptr);

   result = LoadRuleTable(rule_name);
   if (!result.empty()) {
      return result.c_str();
   }
   
   // check for rule suffix like ":T200,100" to specify a bounded universe
   if (colonptr) {
      const char* err = setgridsize(colonptr);
      if (err) return err;
   } else {
      // universe is unbounded
      gridwd = 0;
      gridht = 0;
   }

   // set canonical rule string returned by getrule()
   this->current_rule = rule_name.c_str();
   if (gridwd > 0 || gridht > 0) {
      // setgridsize() was successfully called above, so append suffix
      string bounds = canonicalsuffix();
      this->current_rule += bounds;
   }
   
   maxCellStates = this->n_states;
   ghashbase::setrule(rule_name.c_str());
   return NULL;
}

vector<string> tokenize(const string& str,const string& delimiters)
{
   vector<string> tokens;

   // skip delimiters at beginning.
   string::size_type lastPos = str.find_first_not_of(delimiters, 0);

   // find first "non-delimiter".
   string::size_type pos = str.find_first_of(delimiters, lastPos);

   while (string::npos != pos || string::npos != lastPos)
   {
      // found a token, add it to the vector.
      tokens.push_back(str.substr(lastPos, pos - lastPos));

      // skip delimiters.  Note the "not_of"
      lastPos = str.find_first_not_of(delimiters, pos);

      // find next "non-delimiter"
      pos = str.find_first_of(delimiters, lastPos);
   }

   return tokens;
}

string trim_right(const string & s, const string & t = " \t\r\n")
{ 
   string d (s); 
   string::size_type i (d.find_last_not_of (t));
   if (i == string::npos)
      return "";
   else
      return d.erase (d.find_last_not_of (t) + 1); 
}

string trim_left(const string & s, const string & t = " \t\r\n") 
{ 
   string d (s); 
   return d.erase (0, s.find_first_not_of (t)); 
}

string trim(const string & s, const string & t = " \t\r\n")
{ 
   string d (s); 
   return trim_left (trim_right (d, t), t); 
}

const char *defaultRuleData[] = {
   "n_states:8", "neighborhood:vonNeumann", "symmetries:rotate4",
   "000000", "000012", "000020", "000030", "000050", "000063", "000071",
   "000112", "000122", "000132", "000212", "000220", "000230", "000262",
   "000272", "000320", "000525", "000622", "000722", "001022", "001120",
   "002020", "002030", "002050", "002125", "002220", "002322", "005222",
   "012321", "012421", "012525", "012621", "012721", "012751", "014221",
   "014321", "014421", "014721", "016251", "017221", "017255", "017521",
   "017621", "017721", "025271", "100011", "100061", "100077", "100111",
   "100121", "100211", "100244", "100277", "100511", "101011", "101111",
   "101244", "101277", "102026", "102121", "102211", "102244", "102263",
   "102277", "102327", "102424", "102626", "102644", "102677", "102710",
   "102727", "105427", "111121", "111221", "111244", "111251", "111261",
   "111277", "111522", "112121", "112221", "112244", "112251", "112277",
   "112321", "112424", "112621", "112727", "113221", "122244", "122277",
   "122434", "122547", "123244", "123277", "124255", "124267", "125275",
   "200012", "200022", "200042", "200071", "200122", "200152", "200212",
   "200222", "200232", "200242", "200250", "200262", "200272", "200326",
   "200423", "200517", "200522", "200575", "200722", "201022", "201122",
   "201222", "201422", "201722", "202022", "202032", "202052", "202073",
   "202122", "202152", "202212", "202222", "202272", "202321", "202422",
   "202452", "202520", "202552", "202622", "202722", "203122", "203216",
   "203226", "203422", "204222", "205122", "205212", "205222", "205521",
   "205725", "206222", "206722", "207122", "207222", "207422", "207722",
   "211222", "211261", "212222", "212242", "212262", "212272", "214222",
   "215222", "216222", "217222", "222272", "222442", "222462", "222762",
   "222772", "300013", "300022", "300041", "300076", "300123", "300421",
   "300622", "301021", "301220", "302511", "401120", "401220", "401250",
   "402120", "402221", "402326", "402520", "403221", "500022", "500215",
   "500225", "500232", "500272", "500520", "502022", "502122", "502152",
   "502220", "502244", "502722", "512122", "512220", "512422", "512722",
   "600011", "600021", "602120", "612125", "612131", "612225", "700077",
   "701120", "701220", "701250", "702120", "702221", "702251", "702321",
   "702525", "702720", 0 };

string ruletable_algo::LoadRuleTable(string rule)
{
   const string comment_keyword = "#";
   const string symmetries_keyword = "symmetries:";
   const string neighborhood_keyword = "neighborhood:";
   const string n_states_keyword = "n_states:";
   const string variable_keyword = "var ";
   
   map< string, vector<string> > available_symmetries;
   {
       const string vonNeumann_available_symmetries[5] = {"none","rotate4","rotate4reflect","reflect_horizontal","permute"};
       available_symmetries["vonNeumann"].assign(vonNeumann_available_symmetries,vonNeumann_available_symmetries+5);
       const string Moore_available_symmetries[7] = {"none","rotate4","rotate8","rotate4reflect","rotate8reflect","reflect_horizontal","permute"};
       available_symmetries["Moore"].assign(Moore_available_symmetries,Moore_available_symmetries+7);
       const string hexagonal_available_symmetries[6] = {"none","rotate2","rotate3","rotate6","rotate6reflect","permute"};
       available_symmetries["hexagonal"].assign(hexagonal_available_symmetries,hexagonal_available_symmetries+6);
       const string oneDimensional_available_symmetries[3] = {"none","reflect","permute"};
       available_symmetries["oneDimensional"].assign(oneDimensional_available_symmetries,oneDimensional_available_symmetries+3);
   }
   
   string line;
   const int MAX_LINE_LEN=4000;
   char line_buffer[MAX_LINE_LEN];
   linereader line_reader(0);
   int lineno = 0;
   string full_filename;
   
   bool isDefaultRule = IsDefaultRule(rule.c_str());
   if (isDefaultRule) {
      // no need to read table data from a file
   } else if (static_rulefile) {
      // read table data from currently open .rule file
      line_reader.setfile(static_rulefile);
      line_reader.setcloseonfree();
      lineno = static_lineno;
      full_filename = rule + ".rule";
   } else {
      return string("Rule file not found");
   }

   string symmetries = "rotate4"; // default
   TNeighborhood neighborhood = vonNeumann;  // default
   unsigned int n_states = 8;  // default
   map< string, vector<state> > variables;
   vector< pair< vector< vector<state> >, state > > transition_table;
   unsigned int n_inputs=0;

   // these line must have been read before the rest of the file
   bool n_states_parsed=false,neighborhood_parsed=false,symmetries_parsed=false;

   for (;;) 
   {
      if (isDefaultRule) {
         if (defaultRuleData[lineno] == 0)
            break;
         line = defaultRuleData[lineno];
      } else {
         if (!line_reader.fgets(line_buffer,MAX_LINE_LEN))
            break;
         if (line_reader.truncated) {
            ostringstream oss;
            oss << "Error reading " << full_filename << " on line " << lineno+1 
                << ": line too long (maximum length = " << MAX_LINE_LEN << ")";
            return oss.str();
         }
         if (line_buffer[0] == static_endchar)
            break;
         line = line_buffer;
      }
      lineno++;
      // snip off any trailing comment
      if(line.find('#')!=string::npos)
         line.assign(line.begin(),line.begin()+line.find('#'));
      // trim any leading/trailing whitespace
      line = trim(line); 
      // try each of the allowed forms for this line:
      if(line.empty())
         continue; // line was blank or just had a comment
      else if(starts_with(line,n_states_keyword))
      {
         // parse the rest of the line
         if(sscanf(line.c_str()+n_states_keyword.length(),"%d",&n_states)!=1)
         {
            ostringstream oss;
            oss << "Error reading " << full_filename << " on line " << lineno << ": " << line;
            return oss.str();
         }
         if(n_states<2 || n_states>256)
         {
            ostringstream oss;
            oss << "Error reading " << full_filename << " on line " << lineno << ": n_states out of range (min 2, max 256)";
            return oss.str();
         }
         n_states_parsed = true;
      }
      else if(starts_with(line,neighborhood_keyword))
      {
         // parse the rest of the line
         string remaining(line.begin()+neighborhood_keyword.length(),line.end());
         remaining = trim(remaining); // (allow for space between : and value)
         const string* found = find(this->neighborhood_value_keywords,
            this->neighborhood_value_keywords+N_SUPPORTED_NEIGHBORHOODS,remaining);
         if(found == this->neighborhood_value_keywords+N_SUPPORTED_NEIGHBORHOODS)
         {
            ostringstream oss;
            oss << "Error reading " << full_filename << " on line " << lineno << ": unsupported neighborhood";
            return oss.str();
         }
         neighborhood = (TNeighborhood)(found - this->neighborhood_value_keywords);
         switch(neighborhood) {
            default:
            case vonNeumann: n_inputs=5; grid_type=VN_GRID; break;
            case Moore: n_inputs=9; grid_type=SQUARE_GRID; break;
            case hexagonal: n_inputs=7; grid_type=HEX_GRID; break;
            case oneDimensional: n_inputs=3; grid_type=SQUARE_GRID; break;
         }
         neighborhood_parsed = true;
      }
      else if(starts_with(line,symmetries_keyword))
      {
         if(!neighborhood_parsed)
         {
            ostringstream oss;
            oss << "Error reading " << full_filename << ": neighborhood must be declared before symmetries";
            return oss.str();
         }
         string remaining(line.begin()+symmetries_keyword.length(),line.end());
         remaining = trim(remaining); // (allow for space between : and value)
         string neighborhood_as_string = this->neighborhood_value_keywords[neighborhood];
         vector<string>::const_iterator found = find(
            available_symmetries[neighborhood_as_string].begin(),
            available_symmetries[neighborhood_as_string].end(), remaining );
         if(found == available_symmetries[neighborhood_as_string].end())
         {
            ostringstream oss;
            oss << "Error reading " << full_filename << " on line " << lineno << ": unsupported symmetries";
            return oss.str();
         }
         symmetries = remaining;
         symmetries_parsed = true;
      }
      else if(starts_with(line,variable_keyword))
      {
         if(!n_states_parsed || !neighborhood_parsed || !symmetries_parsed)
         {
            ostringstream oss;
            oss << "Error reading " << full_filename << ": one or more of n_states, neighborhood or symmetries missing\nbefore first variable";
            return oss.str();
         }
         // parse the line for the variable name
         vector<string> tokens = tokenize(line,"= ");
         string variable_name = tokens[1];
         // parse the line again for the states
         tokens = tokenize(line,"= {,}");
         vector<state> states;
         if(tokens[1] != variable_name)
         {
            // this can happen if variable_name contains a delimiter like a comma
            ostringstream oss;
            oss << "Error reading " << full_filename << " on line " << lineno << ": " << line << " - bad variable name";
            return oss.str();
         }  
         if(tokens.size()<3)
         {
            ostringstream oss;
            oss << "Error reading " << full_filename << " on line " << lineno << ": " << line;
            return oss.str();
         }  
         for(unsigned int i=2;i<tokens.size();i++)
         {
            if(variables.find(tokens[i])!=variables.end())
            {
               // variables permitted inside later variables
               states.insert(states.end(),variables[tokens[i]].begin(),variables[tokens[i]].end());
            }
            else
            {
               unsigned int s=0;
               if(sscanf(tokens[i].c_str(),"%u",&s)!=1)
               {
                  ostringstream oss;
                  oss << "Error reading " << full_filename << " on line " << lineno << ": " << line;
                  return oss.str();
               }
               if( s>=n_states)
               {
                  ostringstream oss;
                  oss << "Error reading " << full_filename << " on line " << lineno << ": " << line << " - state value out of range";
                  return oss.str();
               }
               states.push_back((state)s);
            }
         }
         variables[variable_name] = states;
      }
      else
      {
         // must be a transitions line
         if(!n_states_parsed || !neighborhood_parsed || !symmetries_parsed)
         {
            ostringstream oss;
            oss << "Error reading " << full_filename << ": one or more of n_states, neighborhood or symmetries missing\nbefore first transition";
            return oss.str();
         }
         if(n_states<=10 && variables.empty() && line.find(',')==string::npos)
         {
            // if there are only single-digit states and no variables then can use comma-free form:
            // e.g. 012345 for 0,1,2,3,4 -> 5
            vector< vector<state> > inputs;
            state output;
            if(line.length() < n_inputs+1)
            {
               ostringstream oss;
               oss << "Error reading " << full_filename << " on line " << lineno << ": " << line << " - too few entries";
               return oss.str();
            }
            for(unsigned int i=0;i<n_inputs;i++)
            {
               char c = line[i];
               if(c<'0' || c>'9')
               {
                  ostringstream oss;
                  oss << "Error reading " << full_filename << " on line " << lineno << ": " << line;
                  return oss.str();
               }

               inputs.push_back(vector<state>(1,c-'0'));
            }
            unsigned char c = line[n_inputs];
            if(c<'0' || c>'9')
            {
               ostringstream oss;
               oss << "Error reading " << full_filename << " on line " << lineno << ": " << line;
               return oss.str();
            }
            output = c-'0';
            if (output >= n_states)     // AKT: avoid later crash in PackTransition
            {
                ostringstream oss;
                oss << "Error reading " << full_filename << " on line " << lineno << ": " << line << " - state out of range";
                return oss.str();
            }
            transition_table.push_back(make_pair(inputs,output));
         }
         else // transition line with commas
         {  
            vector<string> tokens = tokenize(line,", #\t");
            if(tokens.size() < n_inputs+1)
            {
               ostringstream oss;
               oss << "Error reading " << full_filename << " on line " << lineno << ": " << line << " - too few entries";
               return oss.str();
            }
            // first pass: which variables appear more than once? these are "bound" (must take the same value each time they appear in this transition)
            vector<string> bound_variables;
            for(map< string, vector<state> >::const_iterator var_it=variables.begin();var_it!=variables.end();var_it++)
               if(count(tokens.begin(),tokens.begin()+n_inputs+1,var_it->first)>1)
                  bound_variables.push_back(var_it->first);
            unsigned int n_bound_variables = (unsigned int)bound_variables.size();
            // second pass: iterate through the possible states for the bound variables, adding a transition for each combination
            vector< vector<state> > inputs(n_inputs);
            state output;
            map<string,unsigned int> bound_variable_indices; // each is an index into vector<state> of 'variables' map
            for(unsigned int i=0;i<n_bound_variables;i++)
               bound_variable_indices[bound_variables[i]]=0;
            for(;;)
            {
               // output the transition for the current set of bound variables
               for(unsigned int i=0;i<n_inputs;i++) // collect the inputs
               {
                  if(!bound_variables.empty() && find(bound_variables.begin(),bound_variables.end(),tokens[i])!=bound_variables.end())
                    inputs[i] = vector<state>(1,variables[tokens[i]][bound_variable_indices[tokens[i]]]); // this input is a bound variable
                  else if(variables.find(tokens[i])!=variables.end())
                     inputs[i] = variables[tokens[i]]; // this input is an unbound variable
                  else 
                  {
                     unsigned int s=0;
                     if(sscanf(tokens[i].c_str(),"%u",&s)!=1) // this input is a state
                     {
                        ostringstream oss;
                        oss << "Error reading " << full_filename << " on line " << lineno << ": " << line;
                        return oss.str();
                     }
                     if(s>=n_states)
                     {
                        ostringstream oss;
                        oss << "Error reading " << full_filename << " on line " << lineno << ": " << line << " - state out of range";
                        return oss.str();
                     }
                     inputs[i] = vector<state>(1,(state)s);
                  }
               }
               // collect the output
               if(!bound_variables.empty() && 
                  find(bound_variables.begin(),bound_variables.end(),tokens[n_inputs])!=bound_variables.end())
                     output = variables[tokens[n_inputs]][bound_variable_indices[tokens[n_inputs]]];
               else 
               {
                  unsigned int s;
                  if(variables.find(tokens[n_inputs])!=variables.end() &&
                    variables[tokens[n_inputs]].size()==1)
                  {
                     // single-state variables are permitted as the output
                     s = variables[tokens[n_inputs]][0];
                  }
                  else if(sscanf(tokens[n_inputs].c_str(),"%u",&s)!=1) // if not a bound variable, output must be a state
                  {
                     ostringstream oss;
                     oss << "Error reading " << full_filename << " on line " << lineno << ": " << line
                        << " - output must be state, single-state variable or bound variable";
                     return oss.str();
                  }
                  if(s>=n_states)
                  {
                     ostringstream oss;
                     oss << "Error reading " << full_filename << " on line " << lineno << ": " << line << " - state out of range";
                     return oss.str();
                  }
                  output = (state)s;
               }
               transition_table.push_back(make_pair(inputs,output));
               // move on to the next value of bound variables
               {
                  unsigned int iChanging=0;
                  for(;iChanging<n_bound_variables;iChanging++)
                  {
                     if(bound_variable_indices[bound_variables[iChanging]] < variables[bound_variables[iChanging]].size()-1)
                     {
                        bound_variable_indices[bound_variables[iChanging]]++;
                        break;
                     }
                     else
                     {
                        bound_variable_indices[bound_variables[iChanging]]=0;
                     }
                  }
                  if(iChanging>=n_bound_variables)
                     break;
               }
            }
         }
      }
   } // (finished reading lines from the file)

   if(!n_states_parsed || !neighborhood_parsed || !symmetries_parsed)
   {
      ostringstream oss;
      oss << "Error reading " << full_filename << ": one or more of n_states, neighborhood or symmetries missing";
      return oss.str();
   }

   this->neighborhood = neighborhood;
   this->n_states = n_states;
   PackTransitions(symmetries,n_inputs,transition_table);

   return string(""); // success
}

// convert transition table to bitmask lookup
void ruletable_algo::PackTransitions(const string& symmetries, int n_inputs,
                            const vector< pair< vector< vector<state> >, state > >& transition_table)
{
    // cumbersome initialization of a remap array for the different symmetries
    map< string, vector< vector<int> > > symmetry_remap[N_SUPPORTED_NEIGHBORHOODS];
    {
       int vn_rotate4[4][6] = {{0,1,2,3,4,5},{0,2,3,4,1,5},{0,3,4,1,2,5},{0,4,1,2,3,5}};
       for(int i=0;i<4;i++)
          symmetry_remap[vonNeumann]["rotate4"].push_back(vector<int>(vn_rotate4[i],vn_rotate4[i]+6));
       int vn_rotate4reflect[8][6] = {{0,1,2,3,4,5},{0,2,3,4,1,5},{0,3,4,1,2,5},{0,4,1,2,3,5},
          {0,4,3,2,1,5},{0,3,2,1,4,5},{0,2,1,4,3,5},{0,1,4,3,2,5}};
       for(int i=0;i<8;i++)
          symmetry_remap[vonNeumann]["rotate4reflect"].push_back(vector<int>(vn_rotate4reflect[i],vn_rotate4reflect[i]+6));
       int vn_reflect_horizontal[2][6] = {{0,1,2,3,4,5},{0,1,4,3,2,5}};
       for(int i=0;i<2;i++)
          symmetry_remap[vonNeumann]["reflect_horizontal"].push_back(vector<int>(vn_reflect_horizontal[i],vn_reflect_horizontal[i]+6));
       int moore_rotate4[4][10] = {{0,1,2,3,4,5,6,7,8,9},{0,3,4,5,6,7,8,1,2,9},{0,5,6,7,8,1,2,3,4,9},{0,7,8,1,2,3,4,5,6,9}};
       for(int i=0;i<4;i++)
          symmetry_remap[Moore]["rotate4"].push_back(vector<int>(moore_rotate4[i],moore_rotate4[i]+10));
       int moore_rotate8[8][10] = {{0,1,2,3,4,5,6,7,8,9},{0,2,3,4,5,6,7,8,1,9},{0,3,4,5,6,7,8,1,2,9},{0,4,5,6,7,8,1,2,3,9},
          {0,5,6,7,8,1,2,3,4,9},{0,6,7,8,1,2,3,4,5,9},{0,7,8,1,2,3,4,5,6,9},{0,8,1,2,3,4,5,6,7,9}};
       for(int i=0;i<8;i++)
          symmetry_remap[Moore]["rotate8"].push_back(vector<int>(moore_rotate8[i],moore_rotate8[i]+10));
       int moore_rotate4reflect[8][10] = {{0,1,2,3,4,5,6,7,8,9},{0,3,4,5,6,7,8,1,2,9},{0,5,6,7,8,1,2,3,4,9},{0,7,8,1,2,3,4,5,6,9},
          {0,1,8,7,6,5,4,3,2,9},{0,7,6,5,4,3,2,1,8,9},{0,5,4,3,2,1,8,7,6,9},{0,3,2,1,8,7,6,5,4,9}};
       for(int i=0;i<8;i++)
          symmetry_remap[Moore]["rotate4reflect"].push_back(vector<int>(moore_rotate4reflect[i],moore_rotate4reflect[i]+10));
       int moore_rotate8reflect[16][10] = {{0,1,2,3,4,5,6,7,8,9},{0,2,3,4,5,6,7,8,1,9},{0,3,4,5,6,7,8,1,2,9},{0,4,5,6,7,8,1,2,3,9},
          {0,5,6,7,8,1,2,3,4,9},{0,6,7,8,1,2,3,4,5,9},{0,7,8,1,2,3,4,5,6,9},{0,8,1,2,3,4,5,6,7,9},
          {0,8,7,6,5,4,3,2,1,9},{0,7,6,5,4,3,2,1,8,9},{0,6,5,4,3,2,1,8,7,9},{0,5,4,3,2,1,8,7,6,9},
          {0,4,3,2,1,8,7,6,5,9},{0,3,2,1,8,7,6,5,4,9},{0,2,1,8,7,6,5,4,3,9},{0,1,8,7,6,5,4,3,2,9}};
       for(int i=0;i<16;i++)
          symmetry_remap[Moore]["rotate8reflect"].push_back(vector<int>(moore_rotate8reflect[i],moore_rotate8reflect[i]+10));
       int moore_reflect_horizontal[2][10] = {{0,1,2,3,4,5,6,7,8,9},{0,1,8,7,6,5,4,3,2,9}};
       for(int i=0;i<2;i++)
          symmetry_remap[Moore]["reflect_horizontal"].push_back(vector<int>(moore_reflect_horizontal[i],moore_reflect_horizontal[i]+10));
       int oneDimensional_reflect[2][4] = {{0,1,2,3},{0,2,1,3}};
       for(int i=0;i<2;i++)
          symmetry_remap[oneDimensional]["reflect"].push_back(vector<int>(oneDimensional_reflect[i],oneDimensional_reflect[i]+4));
       int hex_rotate2[2][8] = {{0,1,2,3,4,5,6,7},{0,4,5,6,1,2,3,7}};
       for(int i=0;i<2;i++)
          symmetry_remap[hexagonal]["rotate2"].push_back(vector<int>(hex_rotate2[i],hex_rotate2[i]+8));
       int hex_rotate3[3][8] = {{0,1,2,3,4,5,6,7},{0,3,4,5,6,1,2,7},{0,5,6,1,2,3,4,7}};
       for(int i=0;i<3;i++)
          symmetry_remap[hexagonal]["rotate3"].push_back(vector<int>(hex_rotate3[i],hex_rotate3[i]+8));
       int hex_rotate6[6][8] = {{0,1,2,3,4,5,6,7},{0,2,3,4,5,6,1,7},{0,3,4,5,6,1,2,7},
          {0,4,5,6,1,2,3,7},{0,5,6,1,2,3,4,7},{0,6,1,2,3,4,5,7}};
       for(int i=0;i<6;i++)
          symmetry_remap[hexagonal]["rotate6"].push_back(vector<int>(hex_rotate6[i],hex_rotate6[i]+8));
       int hex_rotate6reflect[12][8] = {{0,1,2,3,4,5,6,7},{0,2,3,4,5,6,1,7},{0,3,4,5,6,1,2,7},
                           {0,4,5,6,1,2,3,7},{0,5,6,1,2,3,4,7},{0,6,1,2,3,4,5,7},
                           {0,6,5,4,3,2,1,7},{0,5,4,3,2,1,6,7},{0,4,3,2,1,6,5,7},
                           {0,3,2,1,6,5,4,7},{0,2,1,6,5,4,3,7},{0,1,6,5,4,3,2,7}};
       for(int i=0;i<12;i++)
          symmetry_remap[hexagonal]["rotate6reflect"].push_back(vector<int>(hex_rotate6reflect[i],hex_rotate6reflect[i]+8));
   }

   // initialize the packed transition table
   this->lut.assign(n_inputs,vector< vector<TBits> >(this->n_states));
   this->output.clear();
   this->n_compressed_rules = 0;
   // each transition rule looks like: e.g. 1,[2,3,5],4,[0,1],3 -> 0
   vector< vector<state> > permuted_inputs(n_inputs);
   for(vector< pair< vector< vector<state> >, state> >::const_iterator rule_it = transition_table.begin();
       rule_it!=transition_table.end();
       rule_it++)
   {
      const vector< vector<state> > & inputs = rule_it->first;
      state output = rule_it->second;
      if(symmetries=="none")
      {
         PackTransition(inputs,output);
      }
      else if(symmetries=="permute")
      {
         // work through the permutations of all but the centre cell
         permuted_inputs = inputs;
         sort(permuted_inputs.begin()+1,permuted_inputs.end()); // (must sort before permuting)
         do {
            PackTransition(permuted_inputs,output);
         } while(next_permutation(permuted_inputs.begin()+1,permuted_inputs.end())); // (skips duplicates)
      }
      else
      {
         const vector< vector<int> > & remap = symmetry_remap[this->neighborhood][symmetries];
         for(int iSymm=0;iSymm<(int)remap.size();iSymm++)
         {
            for(int i=0;i<n_inputs;i++)
                permuted_inputs[i] = inputs[remap[iSymm][i]];
            PackTransition(permuted_inputs,output);
         }
      }
   }
}

void ruletable_algo::PackTransition(const vector< vector<state> > & inputs,
                                    state output)
{
    int n_inputs = (int)inputs.size();
    const unsigned int n_bits = (unsigned int)(sizeof(TBits)*8);
    
    this->output.push_back(output);
    int iRule = (int)(this->output.size()-1);
    int iBit = iRule % n_bits;
    unsigned int iRuleC = (iRule-iBit)/n_bits; // the compressed index of the rule
    
    // add a new compressed rule if required
    if(iRuleC >= this->n_compressed_rules)
    {
        for(int iInput=0;iInput<n_inputs;iInput++)
            for(int iState=0;iState<(int)n_states;iState++)
                this->lut[iInput][iState].push_back(0);
        this->n_compressed_rules++;
    }
    
    TBits mask = (TBits)1 << iBit; // (cast needed to ensure this is a 64-bit shift, not a 32-bit shift)
    for(int iNbor=0;iNbor<n_inputs;iNbor++)
    {
        const vector<state> & possibles = inputs[iNbor];
        for(vector<state>::const_iterator poss_it=possibles.begin();poss_it!=possibles.end();poss_it++)
        {
           // add the bits
           this->lut[iNbor][*poss_it][iRuleC] |= mask;
        }
    }
}

const char* ruletable_algo::getrule() {
   return this->current_rule.c_str();
}

const char* ruletable_algo::DefaultRule() {
   return "Langtons-Loops";
}

ruletable_algo::ruletable_algo()
   : n_states(8), neighborhood(vonNeumann), n_compressed_rules(0)
{
   maxCellStates = n_states;
}

ruletable_algo::~ruletable_algo()
{
}

// --- the update function ---
state ruletable_algo::slowcalc(state nw, state n, state ne, state w, state c, state e,
                        state sw, state s, state se) 
{
   TBits is_match = 0;  // AKT: explicitly initialized to avoid gcc warning

   switch (this->neighborhood) {
      case vonNeumann: // c,n,e,s,w
         for(unsigned int iRuleC=0;iRuleC<this->n_compressed_rules;iRuleC++)
         {
            // quit early if possible
            is_match = this->lut[0][c][iRuleC] & this->lut[1][n][iRuleC];
            if (is_match) {
               is_match &= this->lut[2][e][iRuleC] & this->lut[3][s][iRuleC] & this->lut[4][w][iRuleC];

               // if any of them matched, return the output of the first
               if(is_match)
               {
                  // find the least significant bit of is_match
                  unsigned int iBit=0;
                  TBits mask=1;
                  while(!(is_match&mask))
                  {
                     ++iBit;
                     mask <<= 1;
                  }
                  return this->output[ iRuleC*sizeof(TBits)*8 + iBit ]; // find the uncompressed rule index
               }
            }
         }
         break;

      case Moore: // c,n,ne,e,se,s,sw,w,nw
         for(unsigned int iRuleC=0;iRuleC<this->n_compressed_rules;iRuleC++)
         {
            // quit early if possible
            is_match = this->lut[0][c][iRuleC] & this->lut[1][n][iRuleC];
            if (is_match) {
               is_match &= this->lut[2][ne][iRuleC] & this->lut[3][e][iRuleC];
               if (is_match) {
                  is_match &= this->lut[4][se][iRuleC] & this->lut[5][s][iRuleC];
                  if (is_match) {
                     is_match &= this->lut[6][sw][iRuleC] & this->lut[7][w][iRuleC] & this->lut[8][nw][iRuleC];

                     // if any of them matched, return the output of the first
                     if(is_match)
                     {
                        // find the least significant bit of is_match
                        unsigned int iBit=0;
                        TBits mask=1;
                        while(!(is_match&mask))
                        {
                           ++iBit;
                           mask <<= 1;
                        }
                        return this->output[ iRuleC*sizeof(TBits)*8 + iBit ]; // find the uncompressed rule index
                     }
                  }
               }
            }
         }
         break;

      case hexagonal: // c,n,e,se,s,w,nw
         for(unsigned int iRuleC=0;iRuleC<this->n_compressed_rules;iRuleC++)
         {
            // quit early if possible
            is_match = this->lut[0][c][iRuleC] & this->lut[1][n][iRuleC];
            if (is_match) {
               is_match &= this->lut[2][e][iRuleC] & this->lut[3][se][iRuleC];
               if (is_match) {
                  is_match &= this->lut[4][s][iRuleC] & this->lut[5][w][iRuleC] & this->lut[6][nw][iRuleC];

                  // if any of them matched, return the output of the first
                  if(is_match)
                  {
                     // find the least significant bit of is_match
                     unsigned int iBit=0;
                     TBits mask=1;
                     while(!(is_match&mask))
                     {
                        ++iBit;
                        mask <<= 1;
                     }
                     return this->output[ iRuleC*sizeof(TBits)*8 + iBit ]; // find the uncompressed rule index
                  }
               }
            }
         }
         break;

      case oneDimensional: // c,w,e
         for(unsigned int iRuleC=0;iRuleC<this->n_compressed_rules;iRuleC++)
         {
            is_match = this->lut[0][c][iRuleC] & this->lut[1][w][iRuleC] & this->lut[2][e][iRuleC];

            // if any of them matched, return the output of the first
            if(is_match)
            {
               // find the least significant bit of is_match
               unsigned int iBit=0;
               TBits mask=1;
               while(!(is_match&mask))
               {
                  ++iBit;
                  mask <<= 1;
               }
               return this->output[ iRuleC*sizeof(TBits)*8 + iBit ]; // find the uncompressed rule index
            }
         }
         break;
   }
   return c; // default: no change
}

static lifealgo *creator() { return new ruletable_algo(); }

void ruletable_algo::doInitializeAlgoInfo(staticAlgoInfo &ai) 
{
   ghashbase::doInitializeAlgoInfo(ai);
   ai.setAlgorithmName("RuleTable");
   ai.setAlgorithmCreator(&creator);
   ai.minstates = 2;
   ai.maxstates = 256;
   // init default color scheme
   ai.defgradient = true;              // use gradient
   ai.defr1 = 255;                     // start color = red
   ai.defg1 = 0;
   ai.defb1 = 0;
   ai.defr2 = 255;                     // end color = yellow
   ai.defg2 = 255;
   ai.defb2 = 0;
   // if not using gradient then set all states to white
   for (int i=0; i<256; i++) {
      ai.defr[i] = ai.defg[i] = ai.defb[i] = 255;
   }
}
