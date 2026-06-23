#ifndef FLUX_COMPILER_LIVENESS_ANALYSIS_H
#define FLUX_COMPILER_LIVENESS_ANALYSIS_H

#include "flux/compiler/ast.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Flux {

// Per-statement liveness information for one BlockExprAST.
// after[-1] = vars live on entry to the block.
// after[i]  = vars live after executing statement i.
struct StmtLiveness
{
    std::map<int, std::set<std::string>> after;

    bool isLiveAfter(int stmtIdx, const std::string& varName) const
    {
        auto it = after.find(stmtIdx);
        if (it != after.end())
            return it->second.count(varName) > 0;
        return true; // conservative: assume live
    }
};

// Builds a control-flow graph from the AST and runs
// iterative data-flow liveness analysis on ref variables.
class LivenessAnalyzer
{
public:
    void analyze(const ExprAST* root);

    // Returns nullptr if no liveness info for this block.
    const StmtLiveness* getLiveness(const BlockExprAST* block) const;

private:
    std::map<const BlockExprAST*, StmtLiveness> results;

    // Internal CFG node for a single BlockExprAST
    struct BB
    {
        std::vector<int> stmtIndices; // indices into parent block's statements
        std::vector<int> succs;
        std::vector<int> preds;
        std::set<std::string> use;
        std::set<std::string> def;
        std::set<std::string> liveIn;
        std::set<std::string> liveOut;
    };

    struct BlockCFG
    {
        const BlockExprAST* block = nullptr;
        std::vector<BB> bbs;
        int entryBB = 0;
    };

    void buildCFG(const BlockExprAST* block, BlockCFG& cfg);
    void collectDefUse(const ExprAST* stmt, std::set<std::string>& useOut, std::set<std::string>& defOut);
    void solveLiveness(BlockCFG& cfg);
    void computeStmtLiveness(const BlockCFG& cfg, StmtLiveness& out);

    void walkAllBlocks(const ExprAST* root);
};

} // namespace Flux

#endif
