#include "witness-inject/util.h"
#include "clang/Basic/SourceLocation.h"

#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/AST.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/ParentMapContext.h>
#include <cstdint>
#include <limits>
#include <tuple>

namespace witness_inject::util {

    static clang::SourceLocation toFileLoc(clang::SourceManager &SM, clang::SourceLocation L) {
        if (L.isInvalid()) {
            return L;
        }
        return L.isMacroID()
            ? SM.getExpansionLoc(L)
            : L;
    }

    static clang::SourceLocation tokenBegin(clang::SourceManager &sm, const clang::LangOptions &lopts, clang::SourceLocation loc) {
        loc = toFileLoc(sm, loc);
        if (loc.isInvalid()) {
            return loc;
        }

        auto B = clang::Lexer::GetBeginningOfToken(loc, sm, lopts);
        if (B.isValid() && B == loc) {
            return B;
        }

        auto Tok = clang::Lexer::findNextToken(loc, sm, lopts);
        if (!Tok) {
            return clang::SourceLocation();
        }
        return toFileLoc(sm, Tok->getLocation());
    }

    static clang::SourceLocation tokenEnd(clang::SourceManager &sm, const clang::LangOptions &lopts, clang::SourceLocation loc) {
        loc = toFileLoc(sm, loc);
        if (loc.isInvalid()) {
            return loc;
        }
        return clang::Lexer::getLocForEndOfToken(loc, 0, sm, lopts);
    }

    static clang::SourceLocation nodeBegin(clang::SourceManager &sm, const clang::DynTypedNode &node) {
        return toFileLoc(sm, node.getSourceRange().getBegin());
    }

    static unsigned int fileOffset(clang::SourceManager &SM, clang::SourceLocation L) {
        if (L.isInvalid()) {
            return std::numeric_limits<unsigned int>::max();
        }

        unsigned int offset;
        std::tie(std::ignore, offset) = SM.getDecomposedLoc(L);
        return offset;
    }

    class SmallestContainerAtPos : public clang::RecursiveASTVisitor<SmallestContainerAtPos> {
     private:
        clang::SourceManager &sm;
        const clang::LangOptions &lopts;
        clang::SourceLocation loc;
        clang::DynTypedNode bestNode{};
        unsigned bestLength{0};

        template <typename T>
        bool consider(const T &node) {
            auto range = node.getSourceRange();
            if (range.getBegin().isInvalid() ||
                range.getEnd().isInvalid()) {
                return true;
            }
            auto begin_loc = toFileLoc(this->sm, range.getBegin());
            auto end_loc = tokenEnd(this->sm, this->lopts, range.getEnd());

            if (begin_loc.isInvalid() ||
                end_loc.isInvalid() ||
                sm.isBeforeInTranslationUnit(this->loc, begin_loc) ||
                sm.isBeforeInTranslationUnit(end_loc, this->loc)) {
                return true;
            }

            unsigned length = fileOffset(this->sm, end_loc) - fileOffset(this->sm, begin_loc);
            if (this->bestLength == 0 || length < this->bestLength) {
                this->bestNode = clang::DynTypedNode::create(node);
                this->bestLength = length;
            }
            return true;
        }

     public:
        SmallestContainerAtPos(clang::ASTContext &ctx, clang::SourceLocation loc)
            : sm(ctx.getSourceManager()), lopts(ctx.getLangOpts()), loc(toFileLoc(sm, loc)) {}

        bool VisitDecl(clang::Decl *D) {
            return consider(*D);
        }

        bool VisitStmt(clang::Stmt *S) {
            return consider(*S);
        }

        clang::DynTypedNode Best() const {
            return bestNode;
        }
    };

    clang::DynTypedNode largestASTNodeStartingAt(clang::ASTContext &ctx, clang::SourceLocation loc) {
        auto &sm = ctx.getSourceManager();

        auto beginLoc = tokenBegin(sm, ctx.getLangOpts(), loc);
        if (beginLoc.isInvalid()) {
            return {};
        }

        SmallestContainerAtPos visitor(ctx, beginLoc);
        visitor.TraverseDecl(ctx.getTranslationUnitDecl());
        auto node = visitor.Best();
        if (node.getNodeKind().isNone()) {
            return {};
        }

        auto nodeParents = ctx.getParents(node);
        while (!nodeParents.empty() && nodeBegin(sm, node) != beginLoc) {
            const auto &parent = nodeParents[0];
            if (nodeBegin(sm, parent) == beginLoc) {
                node = parent;
                break;
            }
            nodeParents = ctx.getParents(parent);
        }

        for (bool climb = true; climb;) {
            auto parents = ctx.getParents(node);
            climb = false;
            for (auto &P : parents) {
                if (nodeBegin(sm, P) == beginLoc || P.get<clang::Expr>()) {
                    node = P;
                    climb = true;
                    break;
                }
            }
        }
        return node;
    }
}