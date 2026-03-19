//===--- Markup.cpp - Self-contained Markdown parser for doc comments ------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// A self-contained Markdown parser for Swift doc comments, replacing the
// upstream cmark dependency.  Supports the subset of CommonMark used in
// Swift documentation: paragraphs, headers, horizontal rules, code blocks
// (fenced and indented), block quotes, ordered/unordered lists, inline
// code, emphasis, strong emphasis, links, images, soft/hard line breaks,
// and inline/block HTML.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "swiftc/Markup/LineList.h"
#include "swiftc/Markup/Markup.h"

using namespace swift;
using namespace markup;

//===----------------------------------------------------------------------===//
// Inline parser
//===----------------------------------------------------------------------===//

namespace {

/// Split a raw text string into inline markup AST nodes.
/// Handles: backtick code spans, **strong**, *emphasis*, [links](url),
/// ![images](url "title"), inline HTML, hard line breaks (trailing \\),
/// and soft breaks (\n inside a paragraph).
void parseInlines(MarkupContext &MC, StringRef Text,
                  SmallVectorImpl<MarkupASTNode *> &Out) {
  size_t i = 0;
  size_t textStart = 0;
  auto flushText = [&](size_t end) {
    if (end > textStart) {
      StringRef Seg = Text.substr(textStart, end - textStart);
      Out.push_back(Text::create(MC, MC.allocateCopy(Seg)));
    }
  };

  while (i < Text.size()) {
    char C = Text[i];

    // ----- backtick code span -----
    if (C == '`') {
      flushText(i);
      size_t tickLen = 1;
      while (i + tickLen < Text.size() && Text[i + tickLen] == '`')
        ++tickLen;
      // Look for matching closing backtick run.
      size_t searchStart = i + tickLen;
      size_t closePos = StringRef::npos;
      for (size_t j = searchStart; j <= Text.size() - tickLen; ++j) {
        if (Text.substr(j, tickLen) == Text.substr(i, tickLen)) {
          // Verify not part of a longer run (only match exact length).
          bool longerBefore = (j > 0 && Text[j - 1] == '`');
          bool longerAfter = (j + tickLen < Text.size() &&
                              Text[j + tickLen] == '`');
          if (!longerBefore && !longerAfter) {
            closePos = j;
            break;
          }
        }
      }
      if (closePos != StringRef::npos) {
        StringRef Content = Text.substr(i + tickLen,
                                        closePos - i - tickLen);
        // Strip a single leading/trailing space if both are present.
        if (Content.size() >= 2 && Content.front() == ' ' &&
            Content.back() == ' ')
          Content = Content.substr(1, Content.size() - 2);
        Out.push_back(Code::create(MC, MC.allocateCopy(Content)));
        i = closePos + tickLen;
        textStart = i;
        continue;
      }
      // No closing backticks found – treat as literal.
      i += tickLen;
      continue;
    }

    // ----- hard line break (backslash before newline) -----
    if (C == '\\' && i + 1 < Text.size() && Text[i + 1] == '\n') {
      flushText(i);
      Out.push_back(LineBreak::create(MC));
      i += 2;
      textStart = i;
      continue;
    }

    // ----- hard line break (two+ trailing spaces before newline) -----
    if (C == ' ' && i + 1 < Text.size()) {
      size_t spStart = i;
      while (i < Text.size() && Text[i] == ' ')
        ++i;
      if (i < Text.size() && Text[i] == '\n' && (i - spStart) >= 2) {
        flushText(spStart);
        Out.push_back(LineBreak::create(MC));
        ++i; // skip newline
        textStart = i;
        continue;
      }
      // Not a hard break – continue; the text will be flushed later.
      continue;
    }

    // ----- soft break -----
    if (C == '\n') {
      flushText(i);
      Out.push_back(SoftBreak::create(MC));
      ++i;
      textStart = i;
      continue;
    }

    // ----- inline HTML -----
    if (C == '<' && i + 1 < Text.size() &&
        (isalpha(Text[i + 1]) || Text[i + 1] == '/' ||
         Text[i + 1] == '!')) {
      size_t closePos = Text.find('>', i + 1);
      if (closePos != StringRef::npos) {
        flushText(i);
        StringRef Tag = Text.substr(i, closePos - i + 1);
        Out.push_back(InlineHTML::create(MC, MC.allocateCopy(Tag)));
        i = closePos + 1;
        textStart = i;
        continue;
      }
    }

    // ----- images ![alt](url "title") -----
    if (C == '!' && i + 1 < Text.size() && Text[i + 1] == '[') {
      size_t altClose = Text.find(']', i + 2);
      if (altClose != StringRef::npos && altClose + 1 < Text.size() &&
          Text[altClose + 1] == '(') {
        size_t urlClose = Text.find(')', altClose + 2);
        if (urlClose != StringRef::npos) {
          flushText(i);
          StringRef AltText = Text.substr(i + 2, altClose - i - 2);
          StringRef UrlAndTitle = Text.substr(altClose + 2,
                                              urlClose - altClose - 2);
          StringRef Url = UrlAndTitle;
          Optional<StringRef> Title;
          // Check for quoted title.
          size_t QuotePos = UrlAndTitle.find('"');
          if (QuotePos != StringRef::npos) {
            Url = UrlAndTitle.substr(0, QuotePos).rtrim();
            size_t TitleEnd = UrlAndTitle.find('"', QuotePos + 1);
            if (TitleEnd != StringRef::npos)
              Title = MC.allocateCopy(
                  UrlAndTitle.substr(QuotePos + 1, TitleEnd - QuotePos - 1));
          }
          SmallVector<MarkupASTNode *, 2> AltChildren;
          AltChildren.push_back(
              Text::create(MC, MC.allocateCopy(AltText)));
          Out.push_back(Image::create(MC, MC.allocateCopy(Url), Title,
                                      AltChildren));
          i = urlClose + 1;
          textStart = i;
          continue;
        }
      }
    }

    // ----- links [text](url) -----
    if (C == '[') {
      size_t closePos = Text.find(']', i + 1);
      if (closePos != StringRef::npos && closePos + 1 < Text.size() &&
          Text[closePos + 1] == '(') {
        size_t urlClose = Text.find(')', closePos + 2);
        if (urlClose != StringRef::npos) {
          flushText(i);
          StringRef LinkText = Text.substr(i + 1, closePos - i - 1);
          StringRef Url = Text.substr(closePos + 2,
                                      urlClose - closePos - 2);
          SmallVector<MarkupASTNode *, 2> LinkChildren;
          parseInlines(MC, LinkText, LinkChildren);
          Out.push_back(Link::create(MC, MC.allocateCopy(Url),
                                     LinkChildren));
          i = urlClose + 1;
          textStart = i;
          continue;
        }
      }
    }

    // ----- strong **text** or __text__ -----
    if ((C == '*' || C == '_') && i + 1 < Text.size() &&
        Text[i + 1] == C) {
      // Look for matching closing pair.
      size_t closePos = Text.find(Text.substr(i, 2), i + 2);
      if (closePos != StringRef::npos) {
        flushText(i);
        StringRef Inner = Text.substr(i + 2, closePos - i - 2);
        SmallVector<MarkupASTNode *, 2> Children;
        parseInlines(MC, Inner, Children);
        Out.push_back(Strong::create(MC, Children));
        i = closePos + 2;
        textStart = i;
        continue;
      }
    }

    // ----- emphasis *text* or _text_ -----
    if ((C == '*' || C == '_') &&
        !(i + 1 < Text.size() && Text[i + 1] == C)) {
      size_t closePos = Text.find(C, i + 1);
      if (closePos != StringRef::npos) {
        // Make sure it's not a double marker.
        if (closePos + 1 >= Text.size() || Text[closePos + 1] != C) {
          flushText(i);
          StringRef Inner = Text.substr(i + 1, closePos - i - 1);
          SmallVector<MarkupASTNode *, 2> Children;
          parseInlines(MC, Inner, Children);
          Out.push_back(Emphasis::create(MC, Children));
          i = closePos + 1;
          textStart = i;
          continue;
        }
      }
    }

    ++i;
  }
  flushText(Text.size());
}

//===----------------------------------------------------------------------===//
// Block parser
//===----------------------------------------------------------------------===//

/// A simple line-based Markdown block parser.
/// Splits the input into lines and recognizes block-level structure:
/// ATX headers, setext headers, horizontal rules, fenced code blocks,
/// indented code blocks, block quotes, ordered/unordered lists, HTML blocks,
/// and paragraphs.
class MarkdownParser {
  MarkupContext &MC;
  SmallVector<StringRef, 32> Lines;
  size_t Pos = 0;

  bool atEnd() const { return Pos >= Lines.size(); }
  StringRef currentLine() const { return Lines[Pos]; }
  void advance() { ++Pos; }

  /// Is this line blank (empty or only whitespace)?
  static bool isBlank(StringRef L) {
    return L.ltrim().empty();
  }

  /// Is this an ATX heading? (# ... ######)
  static unsigned atxHeadingLevel(StringRef L) {
    StringRef Trimmed = L.ltrim();
    unsigned Level = 0;
    while (Level < Trimmed.size() && Trimmed[Level] == '#' && Level < 6)
      ++Level;
    if (Level == 0)
      return 0;
    // Must be followed by space or end of line.
    if (Level < Trimmed.size() && Trimmed[Level] != ' ')
      return 0;
    return Level;
  }

  /// Is this a horizontal rule? (3+ of *, -, or _ with optional spaces)
  static bool isHRule(StringRef L) {
    StringRef Trimmed = L.ltrim();
    if (Trimmed.size() < 3)
      return false;
    char C = Trimmed[0];
    if (C != '*' && C != '-' && C != '_')
      return false;
    unsigned Count = 0;
    for (char Ch : Trimmed) {
      if (Ch == C)
        ++Count;
      else if (Ch != ' ' && Ch != '\t')
        return false;
    }
    return Count >= 3;
  }

  /// Is this a fenced code block opener? (``` or ~~~)
  static bool isFenceOpen(StringRef L, StringRef &Fence, StringRef &Info) {
    StringRef Trimmed = L.ltrim();
    char C = 0;
    if (Trimmed.starts_with("```"))
      C = '`';
    else if (Trimmed.starts_with("~~~"))
      C = '~';
    else
      return false;
    size_t FenceLen = 0;
    while (FenceLen < Trimmed.size() && Trimmed[FenceLen] == C)
      ++FenceLen;
    Fence = Trimmed.substr(0, FenceLen);
    Info = Trimmed.substr(FenceLen).ltrim();
    return true;
  }

  /// Does this line start a block quote? (> ...)
  static bool isBlockQuote(StringRef L) {
    return L.ltrim().starts_with(">");
  }

  /// Unordered list item marker: -, *, or +
  static bool isUnorderedListItem(StringRef L, StringRef &Rest) {
    StringRef Trimmed = L.ltrim();
    if (Trimmed.size() >= 2 &&
        (Trimmed[0] == '-' || Trimmed[0] == '*' || Trimmed[0] == '+') &&
        Trimmed[1] == ' ') {
      Rest = Trimmed.substr(2);
      return true;
    }
    return false;
  }

  /// Ordered list item: digit(s). or digit(s))
  static bool isOrderedListItem(StringRef L, StringRef &Rest) {
    StringRef Trimmed = L.ltrim();
    size_t i = 0;
    while (i < Trimmed.size() && isdigit(Trimmed[i]))
      ++i;
    if (i == 0 || i >= Trimmed.size())
      return false;
    if ((Trimmed[i] == '.' || Trimmed[i] == ')') &&
        i + 1 < Trimmed.size() && Trimmed[i + 1] == ' ') {
      Rest = Trimmed.substr(i + 2);
      return true;
    }
    return false;
  }

  /// Is this an indented code block line? (4+ spaces or 1+ tab)
  static bool isIndentedCode(StringRef L) {
    return L.starts_with("    ") || L.starts_with("\t");
  }

  /// Is this a block HTML line?
  static bool isBlockHTML(StringRef L) {
    StringRef Trimmed = L.ltrim();
    return Trimmed.starts_with("<") && Trimmed.size() > 1 &&
           (isalpha(Trimmed[1]) || Trimmed[1] == '/' || Trimmed[1] == '!');
  }

  /// Is this a setext underline? (=== or ---)
  static int setextLevel(StringRef L) {
    StringRef Trimmed = L.ltrim().rtrim();
    if (Trimmed.size() < 1)
      return 0;
    if (Trimmed.find_first_not_of('=') == StringRef::npos && Trimmed.size() >= 1)
      return 1;
    if (Trimmed.find_first_not_of('-') == StringRef::npos && Trimmed.size() >= 1)
      return 2;
    return 0;
  }

  //=== Block element parsers ===//

  MarkupASTNode *parseATXHeader() {
    StringRef L = currentLine().ltrim();
    unsigned Level = atxHeadingLevel(L);
    StringRef Content = L.drop_front(Level).ltrim();
    // Trim trailing #s.
    while (Content.ends_with("#"))
      Content = Content.drop_back(1);
    Content = Content.rtrim();
    advance();
    SmallVector<MarkupASTNode *, 4> Children;
    parseInlines(MC, Content, Children);
    return Header::create(MC, Level, Children);
  }

  MarkupASTNode *parseFencedCodeBlock() {
    StringRef Fence, Info;
    isFenceOpen(currentLine(), Fence, Info);
    StringRef Language = Info.empty() ? StringRef("swift") :
                         MC.allocateCopy(Info.split(' ').first);
    advance();
    SmallString<256> Content;
    while (!atEnd()) {
      StringRef L = currentLine();
      StringRef Trimmed = L.ltrim();
      if (Trimmed.starts_with(Fence.substr(0, 3)) &&
          Trimmed.ltrim(Fence[0]).ltrim().empty()) {
        advance();
        break;
      }
      Content += L;
      Content += '\n';
      advance();
    }
    return CodeBlock::create(MC, MC.allocateCopy(StringRef(Content)),
                             Language);
  }

  MarkupASTNode *parseIndentedCodeBlock() {
    SmallString<256> Content;
    while (!atEnd() && (isIndentedCode(currentLine()) ||
                        isBlank(currentLine()))) {
      StringRef L = currentLine();
      if (isBlank(L)) {
        Content += '\n';
      } else {
        // Remove 4 spaces or 1 tab of indentation.
        if (L.starts_with("\t"))
          L = L.drop_front(1);
        else
          L = L.drop_front(4);
        Content += L;
        Content += '\n';
      }
      advance();
    }
    // Trim trailing blank lines.
    while (Content.ends_with("\n\n"))
      Content.pop_back();
    return CodeBlock::create(MC, MC.allocateCopy(StringRef(Content)),
                             "swift");
  }

  MarkupASTNode *parseBlockQuote() {
    SmallVector<StringRef, 8> QuotedLines;
    while (!atEnd()) {
      StringRef L = currentLine();
      StringRef Trimmed = L.ltrim();
      if (Trimmed.starts_with("> "))
        QuotedLines.push_back(Trimmed.drop_front(2));
      else if (Trimmed.starts_with(">"))
        QuotedLines.push_back(Trimmed.drop_front(1));
      else if (!isBlank(L) && !QuotedLines.empty())
        // Lazy continuation.
        QuotedLines.push_back(L);
      else
        break;
      advance();
    }
    // Parse the inner content recursively.
    MarkdownParser Inner(MC, QuotedLines);
    SmallVector<MarkupASTNode *, 4> Children;
    Inner.parseBlocks(Children);
    return BlockQuote::create(MC, Children);
  }

  MarkupASTNode *parseList(bool Ordered) {
    SmallVector<MarkupASTNode *, 8> Items;
    while (!atEnd()) {
      StringRef L = currentLine();
      StringRef Rest;
      bool isItem = Ordered ? isOrderedListItem(L, Rest)
                            : isUnorderedListItem(L, Rest);
      if (!isItem)
        break;
      advance();

      // Collect continuation lines for this item.
      SmallVector<StringRef, 4> ItemLines;
      ItemLines.push_back(Rest);
      while (!atEnd()) {
        StringRef ContLine = currentLine();
        if (isBlank(ContLine)) {
          // Blank line might separate paragraphs in a list item,
          // but only if followed by an indented line.
          if (Pos + 1 < Lines.size() &&
              (Lines[Pos + 1].starts_with("  ") ||
               Lines[Pos + 1].starts_with("\t"))) {
            ItemLines.push_back("");
            advance();
            continue;
          }
          break;
        }
        // Continuation: indented or non-marker line.
        StringRef Dummy;
        if (isUnorderedListItem(ContLine, Dummy) ||
            isOrderedListItem(ContLine, Dummy))
          break;
        if (ContLine.starts_with("  ") || ContLine.starts_with("\t")) {
          // Strip 2 spaces or 1 tab of continuation indent.
          if (ContLine.starts_with("\t"))
            ContLine = ContLine.drop_front(1);
          else
            ContLine = ContLine.drop_front(2);
        }
        ItemLines.push_back(ContLine);
        advance();
      }

      // Parse item content.
      MarkdownParser ItemParser(MC, ItemLines);
      SmallVector<MarkupASTNode *, 4> ItemChildren;
      ItemParser.parseBlocks(ItemChildren);
      Items.push_back(Item::create(MC, ItemChildren));
    }
    return List::create(MC, Items, Ordered);
  }

  MarkupASTNode *parseHTMLBlock() {
    SmallString<256> Content;
    Content += currentLine();
    Content += '\n';
    advance();
    while (!atEnd() && !isBlank(currentLine())) {
      Content += currentLine();
      Content += '\n';
      advance();
    }
    return HTML::create(MC, MC.allocateCopy(StringRef(Content)));
  }

  MarkupASTNode *parseParagraph() {
    SmallString<256> Content;
    while (!atEnd()) {
      StringRef L = currentLine();
      if (isBlank(L))
        break;

      // Check if the *next* line is a setext underline for this paragraph.
      // (Only check when we have accumulated at least one line.)
      if (!Content.empty() && setextLevel(L) > 0 &&
          // Disambiguate: a line of only dashes could be a list item or hrule.
          !isHRule(L)) {
        // The content we have so far is a setext heading.
        unsigned Level = setextLevel(L);
        advance(); // consume underline
        SmallVector<MarkupASTNode *, 4> Children;
        parseInlines(MC, StringRef(Content).rtrim(), Children);
        return Header::create(MC, Level, Children);
      }

      // Stop if we hit another block-level construct.
      StringRef Fence, Info, Rest;
      if (atxHeadingLevel(L) || isHRule(L) || isFenceOpen(L, Fence, Info) ||
          isBlockQuote(L))
        break;
      if (isUnorderedListItem(L, Rest) || isOrderedListItem(L, Rest))
        break;

      if (!Content.empty())
        Content += '\n';
      Content += L;
      advance();
    }
    if (Content.empty())
      return nullptr;
    SmallVector<MarkupASTNode *, 4> Children;
    parseInlines(MC, StringRef(Content), Children);
    return Paragraph::create(MC, Children);
  }

public:
  MarkdownParser(MarkupContext &MC, StringRef Input) : MC(MC) {
    // Split into lines.
    while (!Input.empty()) {
      auto Pair = Input.split('\n');
      Lines.push_back(Pair.first);
      Input = Pair.second;
      if (Input.empty() && !Pair.first.empty())
        break;
    }
  }

  MarkdownParser(MarkupContext &MC, ArrayRef<StringRef> InputLines) : MC(MC) {
    for (auto L : InputLines)
      Lines.push_back(L);
  }

  void parseBlocks(SmallVectorImpl<MarkupASTNode *> &Out) {
    while (!atEnd()) {
      StringRef L = currentLine();

      // Skip blank lines.
      if (isBlank(L)) {
        advance();
        continue;
      }

      // ATX heading.
      if (atxHeadingLevel(L)) {
        Out.push_back(parseATXHeader());
        continue;
      }

      // Horizontal rule.
      if (isHRule(L)) {
        Out.push_back(HRule::create(MC));
        advance();
        continue;
      }

      // Fenced code block.
      StringRef Fence, Info;
      if (isFenceOpen(L, Fence, Info)) {
        Out.push_back(parseFencedCodeBlock());
        continue;
      }

      // Block quote.
      if (isBlockQuote(L)) {
        Out.push_back(parseBlockQuote());
        continue;
      }

      // Unordered list.
      StringRef Rest;
      if (isUnorderedListItem(L, Rest)) {
        Out.push_back(parseList(/*Ordered=*/false));
        continue;
      }

      // Ordered list.
      if (isOrderedListItem(L, Rest)) {
        Out.push_back(parseList(/*Ordered=*/true));
        continue;
      }

      // Indented code block.
      if (isIndentedCode(L)) {
        Out.push_back(parseIndentedCodeBlock());
        continue;
      }

      // Block HTML.
      if (isBlockHTML(L)) {
        Out.push_back(parseHTMLBlock());
        continue;
      }

      // Paragraph (default).
      if (auto *P = parseParagraph())
        Out.push_back(P);
    }
  }
};

} // anonymous namespace

//===----------------------------------------------------------------------===//
// Public API
//===----------------------------------------------------------------------===//

Document *swift::markup::parseDocument(MarkupContext &MC, LineList &LL) {
  auto Input = LL.str();
  if (Input.empty())
    return Document::create(MC, {});

  MarkdownParser Parser(MC, StringRef(Input));
  SmallVector<MarkupASTNode *, 8> Children;
  Parser.parseBlocks(Children);
  return Document::create(MC, Children);
}
