#include "flux/compiler/liveness_analysis.h"
#include <algorithm>
#include <cassert>

namespace Flux {

void LivenessAnalyzer::analyze(const ExprAST* root)
{
    results.clear();
    walkAllBlocks(root);
}

static void walkExpr(const ExprAST* expr, void (*cb)(const BlockExprAST*, void*), void* ctx)
{
    if (!expr)
        return;

    if (auto* block = dynamic_cast<const BlockExprAST*>(expr)) {
        cb(block, ctx);
        for (const auto& stmt : block->getStatements())
            walkExpr(stmt.get(), cb, ctx);
        return;
    }

    auto walk = [&](const ExprAST* e) { walkExpr(e, cb, ctx); };

    if (auto* ife = dynamic_cast<const IfExprAST*>(expr)) {
        walk(ife->getCond());
        walk(ife->getThen());
        if (ife->getElse())
            walk(ife->getElse());
        return;
    }
    if (auto* fore = dynamic_cast<const ForExprAST*>(expr)) {
        walk(fore->getStart());
        walk(fore->getEnd());
        if (fore->getStep())
            walk(fore->getStep());
        walk(fore->getBody());
        return;
    }
    if (auto* whilee = dynamic_cast<const WhileExprAST*>(expr)) {
        walk(whilee->getCond());
        walk(whilee->getBody());
        return;
    }
    if (auto* ret = dynamic_cast<const ReturnExprAST*>(expr)) {
        if (ret->getVal())
            walk(ret->getVal());
        return;
    }
    if (auto* assign = dynamic_cast<const AssignExprAST*>(expr)) {
        walk(assign->getLHS());
        walk(assign->getValueExpr());
        return;
    }
    if (auto* let = dynamic_cast<const LetExprAST*>(expr)) {
        if (let->getInit())
            walk(let->getInit());
        if (let->getBody())
            walk(let->getBody());
        return;
    }
    if (auto* bin = dynamic_cast<const BinaryExprAST*>(expr)) {
        walk(bin->getLHS());
        walk(bin->getRHS());
        return;
    }
    if (auto* un = dynamic_cast<const UnaryExprAST*>(expr)) {
        walk(un->getOperand());
        return;
    }
    if (auto* call = dynamic_cast<const CallExprAST*>(expr)) {
        for (size_t i = 0; i < call->getArgs().size(); ++i)
            walk(call->getArgs()[i].get());
        return;
    }
    if (auto* member = dynamic_cast<const MemberExprAST*>(expr)) {
        if (member->getObject())
            walk(member->getObject());
        return;
    }
    if (auto* deref = dynamic_cast<const DerefExprAST*>(expr)) {
        if (deref->getOperand())
            walk(deref->getOperand());
        return;
    }
    if (auto* borrow = dynamic_cast<const BorrowExprAST*>(expr)) {
        if (borrow->getOperand())
            walk(borrow->getOperand());
        return;
    }
    if (auto* idx = dynamic_cast<const IndexExprAST*>(expr)) {
        walk(idx->getArray());
        walk(idx->getRowIndex());
        return;
    }
    if (auto* vec = dynamic_cast<const VectorExprAST*>(expr)) {
        for (size_t i = 0; i < vec->getElements().size(); ++i)
            walk(vec->getElements()[i].get());
        return;
    }
}

static void countBlocks(const BlockExprAST* block, void* ctx)
{
    auto* count = static_cast<int*>(ctx);
    (void)block;
    (*count)++;
}

void LivenessAnalyzer::walkAllBlocks(const ExprAST* root)
{
    // First pass: count blocks to reserve space
    int totalBlocks = 0;
    walkExpr(root, countBlocks, &totalBlocks);

    // Second pass: actually analyze each block
    // (But we need to do it inline since we need the CFG results)
    // Actually, analyze each BlockExprAST as we encounter it.
    // Use a separate walk that does the analysis.
    struct WalkCtx
    {
        LivenessAnalyzer* self;
    };
    WalkCtx wctx{this};

    walkExpr(
        root,
        [](const BlockExprAST* block, void* ctx) {
            auto* wctx = static_cast<WalkCtx*>(ctx);
            BlockCFG cfg;
            cfg.block = block;
            wctx->self->buildCFG(block, cfg);
            wctx->self->solveLiveness(cfg);
            StmtLiveness sl;
            wctx->self->computeStmtLiveness(cfg, sl);
            wctx->self->results[block] = std::move(sl);
        },
        &wctx);
}

// ---- CFG Builder ----

void LivenessAnalyzer::buildCFG(const BlockExprAST* block, BlockCFG& cfg)
{
    const auto& stmts = block->getStatements();
    int n = static_cast<int>(stmts.size());
    if (n == 0) {
        cfg.bbs.emplace_back();
        cfg.entryBB = 0;
        return;
    }

    cfg.entryBB = 0;
    cfg.bbs.emplace_back();

    for (int i = 0; i < n; ++i) {
        const ExprAST* stmt = stmts[i].get();

        // Check if this is a control-flow statement
        bool isCtrl =
            dynamic_cast<const IfExprAST*>(stmt) != nullptr || dynamic_cast<const ForExprAST*>(stmt) != nullptr ||
            dynamic_cast<const WhileExprAST*>(stmt) != nullptr || dynamic_cast<const ReturnExprAST*>(stmt) != nullptr;

        if (!isCtrl) {
            cfg.bbs.back().stmtIndices.push_back(i);
            continue;
        }

        // Start a new block before the control flow
        if (!cfg.bbs.back().stmtIndices.empty()) {
            cfg.bbs.emplace_back();
        }

        if (dynamic_cast<const ReturnExprAST*>(stmt)) {
            // Return terminates. Add it to the current block.
            cfg.bbs.back().stmtIndices.push_back(i);
            cfg.bbs.back().succs.clear(); // no successors
            cfg.bbs.emplace_back();       // unreachable block after return
            continue;
        }

        if (auto* ife = dynamic_cast<const IfExprAST*>(stmt)) {
            int thenBB = static_cast<int>(cfg.bbs.size());
            int elseBB = thenBB;
            int mergeBB = thenBB;

            // Then block (guaranteed to exist)
            cfg.bbs.emplace_back();
            if (auto* thenBlock = dynamic_cast<const BlockExprAST*>(ife->getThen())) {
                for (int j = 0; j < static_cast<int>(thenBlock->getStatements().size()); ++j)
                    cfg.bbs[thenBB].stmtIndices.push_back(i);
            }

            // Else block
            if (ife->getElse()) {
                elseBB = static_cast<int>(cfg.bbs.size());
                cfg.bbs.emplace_back();
                if (auto* elseBlock = dynamic_cast<const BlockExprAST*>(ife->getElse())) {
                    for (int j = 0; j < static_cast<int>(elseBlock->getStatements().size()); ++j)
                        cfg.bbs[elseBB].stmtIndices.push_back(i);
                }
            }

            // Merge block
            mergeBB = static_cast<int>(cfg.bbs.size());
            cfg.bbs.emplace_back();

            // Edges
            cfg.bbs.back().succs.clear();                      // previous block (the one before the if)
            int prevBB = static_cast<int>(cfg.bbs.size()) - 4; // block before thenBB
            cfg.bbs[prevBB].succs.push_back(thenBB);
            cfg.bbs[prevBB].succs.push_back(ife->getElse() ? elseBB : mergeBB);
            cfg.bbs[thenBB].succs.push_back(mergeBB);
            if (ife->getElse())
                cfg.bbs[elseBB].succs.push_back(mergeBB);

            // Continue from merge block
            // Remove the empty merge block if there's nothing after the if
            // by setting curBB = mergeBB
            continue;
        }

        if (auto* fore = dynamic_cast<const ForExprAST*>(stmt)) {
            int bodyBB = static_cast<int>(cfg.bbs.size());
            int afterBB = bodyBB + 1;

            cfg.bbs.emplace_back(); // body
            if (auto* bodyBlock = dynamic_cast<const BlockExprAST*>(fore->getBody())) {
                for (int j = 0; j < static_cast<int>(bodyBlock->getStatements().size()); ++j)
                    cfg.bbs[bodyBB].stmtIndices.push_back(i);
            }

            cfg.bbs.emplace_back(); // after

            // Edges: current -> body, body -> body (loop) or body -> after
            int prevBB = static_cast<int>(cfg.bbs.size()) - 3;
            cfg.bbs[prevBB].succs.push_back(bodyBB);
            cfg.bbs[bodyBB].succs.push_back(prevBB); // back edge
            cfg.bbs[bodyBB].succs.push_back(afterBB);

            // Continue from after block
            continue;
        }

        if (auto* whilee = dynamic_cast<const WhileExprAST*>(stmt)) {
            int headBB = static_cast<int>(cfg.bbs.size());
            int bodyBB = headBB + 1;
            int afterBB = headBB + 2;

            cfg.bbs.emplace_back(); // head (condition)
            cfg.bbs.emplace_back(); // body
            if (auto* bodyBlock = dynamic_cast<const BlockExprAST*>(whilee->getBody())) {
                for (int j = 0; j < static_cast<int>(bodyBlock->getStatements().size()); ++j)
                    cfg.bbs[bodyBB].stmtIndices.push_back(i);
            }

            cfg.bbs.emplace_back(); // after

            // Edges: current -> head, head -> body, head -> after, body -> head
            int prevBB = static_cast<int>(cfg.bbs.size()) - 4;
            cfg.bbs[prevBB].succs.push_back(headBB);
            cfg.bbs[headBB].succs.push_back(bodyBB);
            cfg.bbs[headBB].succs.push_back(afterBB);
            cfg.bbs[bodyBB].succs.push_back(headBB);

            continue;
        }
    }
}

// ---- Def/Use Collection ----

static void collectDefUseStmt(const ExprAST* stmt, std::set<std::string>& useOut, std::set<std::string>& defOut)
{
    if (!stmt)
        return;

    if (auto* let = dynamic_cast<const LetExprAST*>(stmt)) {
        std::string name = let->getVarName();
        name.erase(0, name.find_first_not_of(" "));
        name.erase(name.find_last_not_of(" ") + 1);
        defOut.insert(name);
        if (let->getInit())
            collectDefUseStmt(let->getInit(), useOut, defOut);
        if (let->getBody())
            collectDefUseStmt(let->getBody(), useOut, defOut);
        return;
    }

    if (auto* assign = dynamic_cast<const AssignExprAST*>(stmt)) {
        if (auto* var = dynamic_cast<const VariableExprAST*>(assign->getLHS())) {
            std::string name = var->getName();
            name.erase(0, name.find_first_not_of(" "));
            name.erase(name.find_last_not_of(" ") + 1);
            defOut.insert(name);
        }
        if (assign->getValueExpr())
            collectDefUseStmt(assign->getValueExpr(), useOut, defOut);
        return;
    }

    if (auto* var = dynamic_cast<const VariableExprAST*>(stmt)) {
        std::string name = var->getName();
        name.erase(0, name.find_first_not_of(" "));
        name.erase(name.find_last_not_of(" ") + 1);
        useOut.insert(name);
        return;
    }

    if (auto* bin = dynamic_cast<const BinaryExprAST*>(stmt)) {
        collectDefUseStmt(bin->getLHS(), useOut, defOut);
        collectDefUseStmt(bin->getRHS(), useOut, defOut);
        return;
    }
    if (auto* un = dynamic_cast<const UnaryExprAST*>(stmt)) {
        collectDefUseStmt(un->getOperand(), useOut, defOut);
        return;
    }
    if (auto* call = dynamic_cast<const CallExprAST*>(stmt)) {
        for (size_t i = 0; i < call->getArgs().size(); ++i)
            collectDefUseStmt(call->getArgs()[i].get(), useOut, defOut);
        return;
    }
    if (auto* member = dynamic_cast<const MemberExprAST*>(stmt)) {
        if (member->getObject())
            collectDefUseStmt(member->getObject(), useOut, defOut);
        return;
    }
    if (auto* deref = dynamic_cast<const DerefExprAST*>(stmt)) {
        if (deref->getOperand())
            collectDefUseStmt(deref->getOperand(), useOut, defOut);
        return;
    }
    if (auto* borrow = dynamic_cast<const BorrowExprAST*>(stmt)) {
        if (borrow->getOperand())
            collectDefUseStmt(borrow->getOperand(), useOut, defOut);
        return;
    }
    if (auto* idx = dynamic_cast<const IndexExprAST*>(stmt)) {
        collectDefUseStmt(idx->getArray(), useOut, defOut);
        collectDefUseStmt(idx->getRowIndex(), useOut, defOut);
        return;
    }
    if (auto* vec = dynamic_cast<const VectorExprAST*>(stmt)) {
        for (size_t i = 0; i < vec->getElements().size(); ++i)
            collectDefUseStmt(vec->getElements()[i].get(), useOut, defOut);
        return;
    }
}

// ---- Liveness Solver ----

void LivenessAnalyzer::solveLiveness(BlockCFG& cfg)
{
    int n = static_cast<int>(cfg.bbs.size());

    // Build predecessors
    for (int i = 0; i < n; ++i)
        cfg.bbs[i].preds.clear();
    for (int i = 0; i < n; ++i) {
        for (int s : cfg.bbs[i].succs) {
            if (s >= 0 && s < n)
                cfg.bbs[s].preds.push_back(i);
        }
    }

    // Compute def/use per block (from stmt def/use)
    for (int i = 0; i < n; ++i) {
        auto& bb = cfg.bbs[i];
        bb.use.clear();
        bb.def.clear();
        bb.liveIn.clear();
        bb.liveOut.clear();

        for (int stmtIdx : bb.stmtIndices) {
            const auto& stmts = cfg.block->getStatements();
            if (stmtIdx < 0 || stmtIdx >= static_cast<int>(stmts.size()))
                continue;
            std::set<std::string> stmtUse, stmtDef;
            collectDefUseStmt(stmts[stmtIdx].get(), stmtUse, stmtDef);
            for (const auto& v : stmtUse) {
                if (!bb.def.count(v))
                    bb.use.insert(v);
            }
            for (const auto& v : stmtDef)
                bb.def.insert(v);
        }
    }

    // Iterative solver for liveness data-flow equations
    // liveIn[b] = use[b] ∪ (liveOut[b] - def[b])
    // liveOut[b] = ∪ liveIn[succ] for all successors
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < n; ++i) {
            std::set<std::string> newLiveOut;
            for (int s : cfg.bbs[i].succs) {
                if (s >= 0 && s < n) {
                    newLiveOut.insert(cfg.bbs[s].liveIn.begin(), cfg.bbs[s].liveIn.end());
                }
            }
            if (newLiveOut != cfg.bbs[i].liveOut) {
                cfg.bbs[i].liveOut = newLiveOut;
                changed = true;
            }

            std::set<std::string> newLiveIn = cfg.bbs[i].use;
            for (const auto& v : cfg.bbs[i].liveOut) {
                if (!cfg.bbs[i].def.count(v))
                    newLiveIn.insert(v);
            }
            if (newLiveIn != cfg.bbs[i].liveIn) {
                cfg.bbs[i].liveIn = newLiveIn;
                changed = true;
            }
        }
    }
}

// ---- Per-statement Liveness (Backward Dataflow) ----

void LivenessAnalyzer::computeStmtLiveness(const BlockCFG& cfg, StmtLiveness& out)
{
    const auto& stmts = cfg.block->getStatements();
    int n = static_cast<int>(stmts.size());
    if (n == 0)
        return;

    // Map each statement index to the basic block that contains it
    std::vector<int> stmtToBB(n, -1);
    for (int i = 0; i < static_cast<int>(cfg.bbs.size()); ++i) {
        for (int s : cfg.bbs[i].stmtIndices) {
            if (s >= 0 && s < n)
                stmtToBB[s] = i;
        }
    }

    // Entry liveness = liveIn of entry block
    if (cfg.entryBB >= 0 && cfg.entryBB < static_cast<int>(cfg.bbs.size()))
        out.after[-1] = cfg.bbs[cfg.entryBB].liveIn;

    // Process each basic block independently, walking backwards.
    for (int bi = 0; bi < static_cast<int>(cfg.bbs.size()); ++bi) {
        const auto& bb = cfg.bbs[bi];
        const auto& idxs = bb.stmtIndices;
        if (idxs.empty())
            continue;

        // Start with liveOut of this block
        std::set<std::string> liveAfter = bb.liveOut;

        // Walk statements in reverse order
        for (auto it = idxs.rbegin(); it != idxs.rend(); ++it) {
            int i = *it;
            if (i < 0 || i >= n)
                continue;

            // after[i] = liveAfter (what's live after executing this stmt)
            out.after[i] = liveAfter;

            std::set<std::string> stmtUse, stmtDef;
            collectDefUseStmt(stmts[i].get(), stmtUse, stmtDef);

            // before[i] = (after[i] - def[i]) ∪ use[i]
            // This becomes liveAfter for the previous statement
            std::set<std::string> liveBefore = liveAfter;
            for (const auto& v : stmtDef)
                liveBefore.erase(v);
            for (const auto& v : stmtUse)
                liveBefore.insert(v);

            liveAfter = std::move(liveBefore);
        }
    }
}

// ---- Public API ----

const StmtLiveness* LivenessAnalyzer::getLiveness(const BlockExprAST* block) const
{
    auto it = results.find(block);
    return it != results.end() ? &it->second : nullptr;
}

} // namespace Flux
