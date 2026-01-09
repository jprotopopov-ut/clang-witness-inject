#include "witness-inject/inject.h"
#include "witness-inject/util.h"
#include "witness-inject/witness.h"

#include <clang/AST/ASTTypeTraits.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/AST.h>
#include <clang/Lex/Lexer.h>
#include <clang/AST/ParentMapContext.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

namespace witness_inject {

    WitnessInjectASTConsumer::WitnessInjectASTConsumer(const WitnessInjectionConfig &config, clang::Rewriter &rewriter)
        : config{config}, rewriter{rewriter} {}

    void WitnessInjectASTConsumer::HandleTranslationUnit(clang::ASTContext &ctx) {
        auto &sm = ctx.getSourceManager();
        auto fid = sm.getMainFileID();
        auto fe = sm.getFileEntryRefForID(fid);
        if (!fe) {
            return;
        }

        for (auto &witness : this->config.witness) {
            for (auto &invariant : witness.content) {
                if (invariant.invariant.location.filepath == fe->getName()) {
                    switch (invariant.invariant.type) {
                        case witness::InvariantType::Location:
                            this->InjectLocationInvariant(ctx, fid, invariant.invariant);
                            break;

                        case witness::InvariantType::Loop:
                            this->InjectLoopInvariant(ctx, fid, invariant.invariant);
                            break;
                    }
                }
            }
        }
    }

    void WitnessInjectASTConsumer::InjectLocationInvariant(clang::ASTContext &ctx, clang::FileID fid, const witness::Invariant &invariant) {
        auto &sm = ctx.getSourceManager();

        auto loc = sm.translateLineCol(fid, invariant.location.line, invariant.location.column);
        auto node = witness_inject::util::largestASTNodeStartingAt(ctx, loc);
        this->InjectInvariantAt(ctx, loc, node, invariant);
    }

    void WitnessInjectASTConsumer::InjectLoopInvariant(clang::ASTContext &ctx, clang::FileID fid, const witness::Invariant &invariant) {
        auto &sm = ctx.getSourceManager();

        auto loc = sm.translateLineCol(fid, invariant.location.line, invariant.location.column);
        auto node = witness_inject::util::largestASTNodeStartingAt(ctx, loc);
        if (auto forStmt = node.get<clang::ForStmt>()) {
            auto body = forStmt->getBody();
            this->InjectLoopInvariantAt(ctx, body, invariant);
        } else if (auto whileStmt = node.get<clang::WhileStmt>()) {
            auto body = whileStmt->getBody();
            this->InjectLoopInvariantAt(ctx, body, invariant);
        } else if (auto doStmt = node.get<clang::DoStmt>()) {
            auto body = doStmt->getBody();
            this->InjectLoopInvariantAt(ctx, body, invariant);
        }

        auto end = clang::Lexer::getLocForEndOfToken(node.getSourceRange().getEnd(), 0, sm, ctx.getLangOpts());
        this->InjectInvariantAt(ctx, end, node, invariant);
    }

    void WitnessInjectASTConsumer::InjectLoopInvariantAt(clang::ASTContext &ctx, const clang::Stmt *body, const witness::Invariant &invariant) {
        auto &sm = ctx.getSourceManager();

        if (auto seq = llvm::dyn_cast<clang::CompoundStmt>(body)) {
            this->rewriter.InsertTextAfterToken(seq->getLBracLoc(), this->config.assertFn);
            this->rewriter.InsertTextAfterToken(seq->getLBracLoc(), "(");
            this->rewriter.InsertTextAfterToken(seq->getLBracLoc(), invariant.value);
            this->rewriter.InsertTextAfterToken(seq->getLBracLoc(), "); ");
        } else {
            auto begin = clang::Lexer::GetBeginningOfToken(body->getBeginLoc(), sm, ctx.getLangOpts());
            this->InjectInvariantAt(ctx, begin, clang::DynTypedNode::create(*body), invariant);
        }
    }

    void WitnessInjectASTConsumer::InjectInvariantAt(clang::ASTContext &ctx, clang::SourceLocation loc, clang::DynTypedNode node, const witness::Invariant &invariant) {
        auto &sm = ctx.getSourceManager();
        if (auto stmt = node.get<clang::Stmt>()) {
            auto parents = ctx.getParents(node);
            bool insertSeqStmt = parents.empty() ||
                (!parents[0].get<clang::CompoundStmt>() &&
                !parents[0].get<clang::FunctionDecl>());

            if (insertSeqStmt) {
                auto begin = clang::Lexer::GetBeginningOfToken(stmt->getBeginLoc(), sm, ctx.getLangOpts());
                auto end = clang::Lexer::getLocForEndOfToken(stmt->getEndLoc(), 0, sm, ctx.getLangOpts());
                this->rewriter.InsertText(begin, "{ ");
                this->rewriter.InsertText(end, " }");
            }

            this->rewriter.InsertText(loc, this->config.assertFn);
            this->rewriter.InsertText(loc, "(");
            this->rewriter.InsertText(loc, invariant.value);
            this->rewriter.InsertText(loc, "); ");
        } else if (auto decl = node.get<clang::Decl>()) {
            auto begin = clang::Lexer::GetBeginningOfToken(decl->getBeginLoc(), sm, ctx.getLangOpts());
            this->rewriter.InsertText(loc, this->config.assertFn);
            this->rewriter.InsertText(begin, "(");
            this->rewriter.InsertText(begin, invariant.value);
            this->rewriter.InsertText(begin, "); ");
        }
    }
}
