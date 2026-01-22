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
#include <string>

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
        if (this->config.skipSwitchcases &&
            (node.get<clang::CaseStmt>() ||
            node.get<clang::DefaultStmt>())) {
            return;
        }
        auto begin = node.get<clang::CompoundStmt>()
            ? loc
            : clang::Lexer::GetBeginningOfToken(node.getSourceRange().getBegin(), sm, ctx.getLangOpts());
        this->InjectInvariantAt(ctx, begin, node, invariant);
    }

    void WitnessInjectASTConsumer::InjectLoopInvariant(clang::ASTContext &ctx, clang::FileID fid, const witness::Invariant &invariant) {
        auto &sm = ctx.getSourceManager();

        auto loc = sm.translateLineCol(fid, invariant.location.line, invariant.location.column);
        auto node = witness_inject::util::largestASTNodeStartingAt(ctx, loc);
        if (auto forStmt = node.get<clang::ForStmt>()) {
            auto loc = forStmt->getRParenLoc();
            if (forStmt->getInc()) {
                this->rewriter.InsertText(loc, ", ");
            }
            this->rewriter.InsertText(loc, this->config.assertFn);
            this->rewriter.InsertText(loc, "(");
            this->InjectLocation(ctx, loc, invariant.location);
            this->rewriter.InsertText(loc, invariant.value);
            this->rewriter.InsertText(loc, ")");
        } else if (auto whileStmt = node.get<clang::WhileStmt>()) {
            auto body = whileStmt->getBody();
            this->InjectLoopInvariantAt(ctx, body, invariant);

            auto begin = clang::Lexer::GetBeginningOfToken(node.getSourceRange().getBegin(), sm, ctx.getLangOpts());
            this->InjectInvariantAt(ctx, begin, node, invariant);
        } else if (auto doStmt = node.get<clang::DoStmt>()) {
            auto body = doStmt->getBody();
            this->InjectLoopInvariantAt(ctx, body, invariant);

            auto begin = clang::Lexer::GetBeginningOfToken(node.getSourceRange().getBegin(), sm, ctx.getLangOpts());
            this->InjectInvariantAt(ctx, begin, node, invariant);
        }
    }

    void WitnessInjectASTConsumer::InjectLoopInvariantAt(clang::ASTContext &ctx, const clang::Stmt *body, const witness::Invariant &invariant) {
        auto &sm = ctx.getSourceManager();

        if (auto seq = llvm::dyn_cast<clang::CompoundStmt>(body)) {
            this->rewriter.InsertText(seq->getRBracLoc(), this->config.assertFn);
            this->rewriter.InsertText(seq->getRBracLoc(), "(");
            this->InjectLocation(ctx, seq->getRBracLoc(), invariant.location);
            this->rewriter.InsertText(seq->getRBracLoc(), invariant.value);
            this->rewriter.InsertText(seq->getRBracLoc(), "); ");
        } else {
            auto end = clang::Lexer::getLocForEndOfToken(body->getEndLoc(), 0, sm, ctx.getLangOpts());
            this->InjectInvariantAt(ctx, end, clang::DynTypedNode::create(*body), invariant);
        }
    }

    void WitnessInjectASTConsumer::InjectInvariantAt(clang::ASTContext &ctx, clang::SourceLocation loc, clang::DynTypedNode node, const witness::Invariant &invariant) {
        auto &sm = ctx.getSourceManager();
        if (auto stmt = node.get<clang::Stmt>()) {
            auto parents = ctx.getParents(node);
            bool insertSeqStmt = parents.empty() ||
                (!parents[0].get<clang::CompoundStmt>() &&
                !parents[0].get<clang::FunctionDecl>() &&
                !parents[0].get<clang::LabelStmt>() &&
                !parents[0].get<clang::DefaultStmt>() &&
                !parents[0].get<clang::CaseStmt>());

            if (insertSeqStmt) {
                auto begin = clang::Lexer::GetBeginningOfToken(stmt->getBeginLoc(), sm, ctx.getLangOpts());
                auto end = clang::Lexer::getLocForEndOfToken(stmt->getEndLoc(), 0, sm, ctx.getLangOpts());
                this->rewriter.InsertText(begin, "{ ");
                this->rewriter.InsertText(end, "; }");
            }

            this->rewriter.InsertText(loc, this->config.assertFn);
            this->rewriter.InsertText(loc, "(");
            this->InjectLocation(ctx, loc, invariant.location);
            this->rewriter.InsertText(loc, invariant.value);
            this->rewriter.InsertText(loc, "); ");
        } else if (auto decl = node.get<clang::Decl>()) {
            auto begin = clang::Lexer::GetBeginningOfToken(decl->getBeginLoc(), sm, ctx.getLangOpts());
            this->rewriter.InsertText(loc, this->config.assertFn);
            this->rewriter.InsertText(begin, "(");
            this->InjectLocation(ctx, loc, invariant.location);
            this->rewriter.InsertText(begin, invariant.value);
            this->rewriter.InsertText(begin, "); ");
        }
    }

    void WitnessInjectASTConsumer::InjectLocation(clang::ASTContext &, clang::SourceLocation loc, const witness::Location &location) {
        this->rewriter.InsertText(loc, "/* line=");
        this->rewriter.InsertText(loc, std::to_string(location.line));
        this->rewriter.InsertText(loc, ", column=");
        this->rewriter.InsertText(loc, std::to_string(location.column));
        this->rewriter.InsertText(loc, " */ ");
    }
}
