// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef RULELOADERALGO_H
#define RULELOADERALGO_H

#include "ghashbase.h"
#include "ruletable_algo.h"
#include "ruletreealgo.h"

/**
 *   This algorithm loads rule data from external files.
 */
class ruleloaderalgo : public ghashbase {

public:

    ruleloaderalgo();
    virtual ~ruleloaderalgo();
    virtual state slowcalc(state nw, state n, state ne, state w, state c,
                           state e, state sw, state s, state se);
    virtual const char* setrule(const char* s);
    virtual const char* getrule();
    virtual const char* DefaultRule();
    virtual int NumCellStates();
    static void doInitializeAlgoInfo(staticAlgoInfo &);

protected:
    
    ruletable_algo* LocalRuleTable;      // local instance of RuleTable algo
    ruletreealgo* LocalRuleTree;         // local instance of RuleTree algo

    enum RuleTypes {TABLE, TREE} rule_type;
    
    void SetAlgoVariables(RuleTypes ruletype);
    const char* LoadTableOrTree(FILE* rulefile, const char* rule, size_t offset);
};

extern const char* noTABLEorTREE;

#endif
