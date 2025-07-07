// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "ruleloaderalgo.h"
#include "util.h"       // for lifegetuserrules, lifegetrulesdir, lifegettempdir, lifefatal
#include <string.h>     // for strcmp, strchr
#include <string>       // for std::string

const int MAX_LINE_LEN = 4096;
const char* noTABLEorTREE = "No @TABLE or @TREE section found in .rule file.";

int ruleloaderalgo::NumCellStates()
{
    if (rule_type == TABLE)
        return LocalRuleTable->NumCellStates();
    else // rule_type == TREE
        return LocalRuleTree->NumCellStates();
}

static FILE* OpenRuleFile(std::string& rulename, const char* dir)
{
    // try to open rulename.rule in given dir
    std::string path = dir;
    if (path.length() == 0) return NULL;
    int istart = (int)path.size();
    path += rulename + ".rule";
    // change "dangerous" characters in rulename to underscores
    for (unsigned int i=istart; i<path.size(); i++)
        if (path[i] == '/' || path[i] == '\\') path[i] = '_';
    return fopen(path.c_str(), "rt");
}

void ruleloaderalgo::SetAlgoVariables(RuleTypes ruletype)
{
    // yuk -- we wouldn't need to copy all these variables if we merged
    // the RuleTable and RuleTree code into one algo
    rule_type = ruletype;
    if (rule_type == TABLE) {
        maxCellStates = LocalRuleTable->NumCellStates();
        grid_type = LocalRuleTable->getgridtype();
        gridwd = LocalRuleTable->gridwd;
        gridht = LocalRuleTable->gridht;
        gridleft = LocalRuleTable->gridleft;
        gridright = LocalRuleTable->gridright;
        gridtop = LocalRuleTable->gridtop;
        gridbottom = LocalRuleTable->gridbottom;
        boundedplane = LocalRuleTable->boundedplane;
        sphere = LocalRuleTable->sphere;
        htwist = LocalRuleTable->htwist;
        vtwist = LocalRuleTable->vtwist;
        hshift = LocalRuleTable->hshift;
        vshift = LocalRuleTable->vshift;
    } else {
        // rule_type == TREE
        maxCellStates = LocalRuleTree->NumCellStates();
        grid_type = LocalRuleTree->getgridtype();
        gridwd = LocalRuleTree->gridwd;
        gridht = LocalRuleTree->gridht;
        gridleft = LocalRuleTree->gridleft;
        gridright = LocalRuleTree->gridright;
        gridtop = LocalRuleTree->gridtop;
        gridbottom = LocalRuleTree->gridbottom;
        boundedplane = LocalRuleTree->boundedplane;
        sphere = LocalRuleTree->sphere;
        htwist = LocalRuleTree->htwist;
        vtwist = LocalRuleTree->vtwist;
        hshift = LocalRuleTree->hshift;
        vshift = LocalRuleTree->vshift;
    }
    
    // need to clear cache
    ghashbase::setrule("not used");
}

const char* ruleloaderalgo::LoadTableOrTree(FILE* rulefile, const char* rule, size_t offset)
{
    const char *err;
    char line_buffer[MAX_LINE_LEN+1];
    int lineno = 0;

    linereader lr(rulefile);
    if (offset > 0L) fseek(rulefile, (long)offset, SEEK_SET);

    // find line starting with @TABLE or @TREE
    while (lr.fgets(line_buffer,MAX_LINE_LEN) != 0) {
        lineno++;
        if (strcmp(line_buffer, "@TABLE") == 0) {
            err = LocalRuleTable->LoadTable(rulefile, lineno, '@', rule);
            // err is the result of setrule(rule)
            if (err == NULL) {
                SetAlgoVariables(TABLE);
            }
            // LoadTable has closed rulefile so don't do lr.close()
            return err;
        }
        if (strcmp(line_buffer, "@TREE") == 0) {
            err = LocalRuleTree->LoadTree(rulefile, lineno, '@', rule);
            // err is the result of setrule(rule)
            if (err == NULL) {
                SetAlgoVariables(TREE);
            }
            // LoadTree has closed rulefile so don't do lr.close()
            return err;
        }
    }
    
    lr.close();
    return noTABLEorTREE;
}

static const char* CreateTemporaryRule(const char* tempdir, FILE* rulefile, std::string& rulename, size_t offset)
{
    char line_buffer[MAX_LINE_LEN+1];
    int count = 0;
    
    std::string temppath = tempdir;
    temppath += rulename + ".rule";
    FILE* tempfile = fopen(temppath.c_str(), "w");
    if (!tempfile) {
        fclose(rulefile);
        return "Failed to create temporary rule file!";
    }

    linereader lr(rulefile);
    fseek(rulefile, (long)offset, SEEK_SET); // skip to start of @RULE line

    // copy @RULE data to rulename.rule in tempdir
    while (lr.fgets(line_buffer,MAX_LINE_LEN) != 0) {
        count++;
        // offset can be off by 1 on Windows (presumably due to CRLF)
        if (count == 1 && line_buffer[0] == 'R') fputs("@",tempfile);
        fputs(line_buffer,tempfile);
        fputs("\n",tempfile);
    }

    lr.close(); // closes rulefile
    fclose(tempfile);
    return NULL;
}

const char* ruleloaderalgo::setrule(const char* s)
{
    FILE* rulefile;
    const char *err;
    const char *colonptr = strchr(s,':');
    std::string rulename(s);
    if (colonptr) rulename.assign(s,colonptr);
    
    // first check if rulename is the default rule for RuleTable or RuleTree
    // in which case there is no need to look for a .rule file
    if (LocalRuleTable->IsDefaultRule(rulename.c_str())) {
        err = LocalRuleTable->setrule(s);
        if (err) return err;
        SetAlgoVariables(TABLE);
        return NULL;
    }
    if (LocalRuleTree->IsDefaultRule(rulename.c_str())) {
        err = LocalRuleTree->setrule(s);
        if (err) return err;
        SetAlgoVariables(TREE);
        return NULL;
    }
    
    // check if a loaded RLE file contained local @RULE data
    if (local_file.length() > 0) {
        if (local_rule != rulename) {
            return "Local @RULE name does not match rule in RLE header!";
        }
        
        rulefile = fopen(local_file.c_str(), "rt");
        if (!rulefile) {
            return "Failed to open file with local @RULE data!";
        }
        
        const char* tempdir = lifegettempdir();
        if (strlen(tempdir) == 0) {
            // in bgolly we only need to do this
            return LoadTableOrTree(rulefile, s, RULE_offset);
        } else {
            // for Golly we need to create rulename.rule in tempdir
            // so we can undo gen changes, create a new pattern, etc
            err = CreateTemporaryRule(tempdir, rulefile, rulename, RULE_offset);
            if (err) return err;
            local_file.clear(); // no need to do this again
        }
    }
    
    // look for .rule file in tempdir, then in user's rules dir, then in Golly's rules dir
    bool inuser = false;
    rulefile = OpenRuleFile(rulename, lifegettempdir());
    if (!rulefile) {
        rulefile = OpenRuleFile(rulename, lifegetuserrules());
        if (rulefile) {
            inuser = true;
        } else {
            rulefile = OpenRuleFile(rulename, lifegetrulesdir());
        }
    }
    if (rulefile) {
        err = LoadTableOrTree(rulefile, s, 0L);
        if (inuser && err && (strcmp(err, noTABLEorTREE) == 0)) {
            // if .rule file was found in user's rules dir but had no
            // @TABLE or @TREE section then we look in Golly's rules dir
            // (this lets user override the colors/icons/names in a supplied .rule
            // file without having to copy the entire file)
            rulefile = OpenRuleFile(rulename, lifegetrulesdir());
            if (rulefile) err = LoadTableOrTree(rulefile, s, 0L);
        }
        return err;
    }
    
    // make sure we show given rule string in final error msg
    static std::string badrule;
    badrule = std::string("Unknown rule: ") + s;
    return badrule.c_str();
}

const char* ruleloaderalgo::getrule() {
    if (rule_type == TABLE)
        return LocalRuleTable->getrule();
    else // rule_type == TREE
        return LocalRuleTree->getrule();
}

const char* ruleloaderalgo::DefaultRule() {
    // use RuleTree's default rule (B3/S23)
    return LocalRuleTree->DefaultRule();
}

ruleloaderalgo::ruleloaderalgo()
{
    LocalRuleTable = new ruletable_algo();
    LocalRuleTree = new ruletreealgo();

    // initialize rule_type
    LocalRuleTree->setrule( LocalRuleTree->DefaultRule() );
    SetAlgoVariables(TREE);

    RULE_offset = 0L;
    local_file.clear();
    local_rule.clear();
}

ruleloaderalgo::~ruleloaderalgo()
{
    delete LocalRuleTable;
    delete LocalRuleTree;
}

state ruleloaderalgo::slowcalc(state nw, state n, state ne, state w, state c,
                               state e, state sw, state s, state se) 
{
    if (rule_type == TABLE)
        return LocalRuleTable->slowcalc(nw, n, ne, w, c, e, sw, s, se);
    else // rule_type == TREE
        return LocalRuleTree->slowcalc(nw, n, ne, w, c, e, sw, s, se);
}

static lifealgo* creator()
{
    return new ruleloaderalgo();
}

void ruleloaderalgo::doInitializeAlgoInfo(staticAlgoInfo &ai) 
{
    ghashbase::doInitializeAlgoInfo(ai);
    ai.setAlgorithmName("RuleLoader");
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
