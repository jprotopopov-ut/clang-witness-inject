#include "witness-inject/inject.h"
#include "witness-inject/util.h"
#include "witness-inject/witness.h"
#include "clang/AST/Stmt.h"

#include <clang/Basic/SourceManager.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/AST.h>
#include <clang/Lex/Lexer.h>
#include <clang/AST/ParentMapContext.h>
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
                    this->InjectInvariant(ctx, fid, invariant.invariant);
                }
            }
        }
    }

    void WitnessInjectASTConsumer::InjectInvariant(clang::ASTContext &ctx, clang::FileID fid, const witness::Invariant &invariant) {
        auto &sm = ctx.getSourceManager();

        if (invariant.type == witness::InvariantType::Location) {
            auto loc = sm.translateLineCol(fid, invariant.location.line, invariant.location.column);
            auto node = witness_inject::util::largestASTNodeStartingAt(ctx, loc);
            if (auto stmt = node.get<clang::Stmt>()) {
                auto parents = ctx.getParents(node);
                bool insertSeqStmt = parents.empty() ||
                    (!parents[0].get<clang::CompoundStmt>() &&
                     !parents[0].get<clang::FunctionDecl>());

                if (insertSeqStmt) {
                    auto begin = clang::Lexer::GetBeginningOfToken(stmt->getBeginLoc(), sm, ctx.getLangOpts());
                    auto end = clang::Lexer::getLocForEndOfToken(stmt->getEndLoc(), 0, sm, ctx.getLangOpts());
                    this->rewriter.InsertText(begin, "do { ");
                    this->rewriter.InsertText(end, "; } while (0)");
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
}
