#pragma once

#include "TokenAnalyzer.h"

namespace clang {
namespace format {

class BraceBreaker : public TokenAnalyzer {
public:
  BraceBreaker(const Environment &Env, const FormatStyle &Style);

  std::pair<tooling::Replacements, unsigned>
  analyze(TokenAnnotator &Annotator,
          SmallVectorImpl<AnnotatedLine *> &AnnotatedLines,
          FormatTokenLexer &Tokens) override;
};

} // end namespace format
} // end namespace clang
