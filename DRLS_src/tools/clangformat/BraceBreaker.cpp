#include "BraceBreaker.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Regex.h"

#include <iostream>

namespace clang {
namespace format {

BraceBreaker::BraceBreaker(const Environment& Env, const FormatStyle& Style)
    : TokenAnalyzer(Env, Style) {
}

void insertNewline(tooling::Replacements& Result,
                   const Environment& Env,
                   FormatToken* token,
                   unsigned int lineStartCol) {
    std::string text{"\n"};

    for (unsigned int i = 0; i < lineStartCol; i++)
        text += " ";

    text += "{";

    auto Err = Result.add(
            tooling::Replacement(Env.getSourceManager(),
                                 CharSourceRange::getCharRange(token->Tok.getLocation(),
                                                               token->Tok.getEndLoc()),
                                 text));

    if (Err) {
    }
}

enum class State {
    Start,
    AfterControlStatement,
    InControlParen,
    NextLBraceMayBreak,
    AfterIdentifier,
    InDeclarationParen,
    AfterDeclarationParen,
    AfterInitializerColonOrComma,
    AfterInitializerIdentifier,
    InInitializerParen,
    InInitializerBrace,
    AfterTemplate,
    InTemplateBrace
};

std::pair<tooling::Replacements, unsigned>
BraceBreaker::analyze(TokenAnnotator& /*Annotator*/,
					  SmallVectorImpl<AnnotatedLine*>& AnnotatedLines,
					  FormatTokenLexer& /*Tokens*/) {
    AffectedRangeMgr.computeAffectedLines(AnnotatedLines);
    tooling::Replacements Fixes;
    std::string AllNamespaceNames = "";

    unsigned int lineStartCol = 0;
    bool hadNewline           = false;
    int parenDepth            = 0;

    for (size_t I = 0, E = AnnotatedLines.size(); I != E; ++I) {
        const AnnotatedLine* EndLine = AnnotatedLines[I];

        FormatToken* first = EndLine->First;
        lineStartCol       = EndLine->First->OriginalColumn;
        hadNewline         = false;

        auto token = EndLine->First;
        auto state = State::Start;

        do {
            switch (state) {
            case State::Start:
                if (token->is(tok::kw_template))
                    state = State::AfterTemplate;

                if (token->is(tok::kw_class) || token->is(tok::kw_struct))
                    state = State::NextLBraceMayBreak;

                if (token->is(tok::kw_if) || token->is(tok::kw_for) ||
                    token->is(tok::kw_while)) {
                    state = State::AfterControlStatement;
                }

                if (token->is(tok::identifier)) {
                    state = State::AfterIdentifier;
                }

                break;

            case State::AfterTemplate:
                if (token->is(tok::less)) {
                    parenDepth++;
                    state = State::InTemplateBrace;
                } else {
                    state = State::Start;
                }

                break;

            case State::InTemplateBrace:
                if (token->is(tok::less))
                    parenDepth++;

                if (token->is(tok::greater))
                    parenDepth--;

                if (parenDepth == 0) {
                    state = State::Start;
                    first = nullptr;
                }

                break;

            case State::AfterControlStatement:
                if (token->is(tok::l_paren)) {
                    parenDepth++;
                    state = State::InControlParen;
                }

                break;

            case State::AfterIdentifier:
                if (token->is(tok::l_paren)) {
                    parenDepth++;
                    state = State::InDeclarationParen;
                }

                break;

            case State::InDeclarationParen:
                if (token->is(tok::l_paren))
                    parenDepth++;

                if (token->is(tok::r_paren))
                    parenDepth--;

                if (parenDepth == 0)
                    state = State::AfterDeclarationParen;

                break;

            case State::AfterDeclarationParen:
                if (token->is(tok::l_brace) && hadNewline) {
                    insertNewline(Fixes, Env, token, lineStartCol);
                    state = State::Start;
                } else if (token->is(tok::colon) || token->is(tok::comma)) {
                    state = State::AfterInitializerColonOrComma;
                } else if (!token->is(tok::kw_const) && !token->is(tok::kw_throw) &&
                           !token->is(tok::kw_noexcept) &&
                           !token->TokenText.startswith("override")) {
                    state = State::Start;
                }

                break;

            case State::AfterInitializerColonOrComma:
                if (token->is(tok::identifier)) {
                    state = State::AfterInitializerIdentifier;
                }

                break;

            case State::AfterInitializerIdentifier:
                if (token->is(tok::l_paren)) {
                    parenDepth++;
                    state = State::InInitializerParen;
                }

                if (token->is(tok::l_brace)) {
                    parenDepth++;
                    state = State::InInitializerBrace;
                }

                break;

            case State::InInitializerParen:
                if (token->is(tok::l_paren))
                    parenDepth++;

                if (token->is(tok::r_paren))
                    parenDepth--;

                if (parenDepth == 0)
                    state = State::AfterDeclarationParen;

                break;

            case State::InInitializerBrace:
                if (token->is(tok::l_brace))
                    parenDepth++;

                if (token->is(tok::r_brace))
                    parenDepth--;

                if (parenDepth == 0)
                    state = State::AfterDeclarationParen;

                break;

            case State::InControlParen:
                if (token->is(tok::l_paren))
                    parenDepth++;

                if (token->is(tok::r_paren))
                    parenDepth--;

                if (parenDepth == 0)
                    state = State::NextLBraceMayBreak;

                break;

            case State::NextLBraceMayBreak:
                if (token->is(tok::l_brace) && hadNewline) {
                    insertNewline(Fixes, Env, token, lineStartCol);
                    state = State::Start;
                }

                break;
            }

            if (token != first && token->HasUnescapedNewline)
                hadNewline = true;

            token = token->Next;

            if (first == nullptr) {
                first      = token;
                hadNewline = false;
            }
        } while (token);
    }
    return {Fixes, 0};
}

}  // namespace format
}  // namespace clang
