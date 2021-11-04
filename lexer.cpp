#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

namespace parse
  {
    const std::unordered_map<std::string, Token> KEY_WORDS_TOKEN{
        {std::string("class"), token_type::Class{}}, {std::string("return"), token_type::Return{}}, {
            std::string("if"), token_type::If{}}, {std::string("else"), token_type::Else{}}, {
            std::string("def"), token_type::Def{}}, {
            std::string("print"), token_type::Print{}}, {std::string("and"), token_type::And{}}, {
            std::string("or"), token_type::Or{}}, {std::string("not"), token_type::Not{}}, {
            std::string("None"), token_type::None{}}, {
            std::string("True"), token_type::True{}}, {std::string("False"), token_type::False{}}
    };
    const std::unordered_map<std::string, Token> SPECIAL_OPERATORS_TOKEN{
        {std::string("=="), token_type::Eq{}}, {std::string("!="), token_type::NotEq{}}, {
            std::string("<="), token_type::LessOrEq{}}, {std::string(">="), token_type::GreaterOrEq{}}
    };
    const std::unordered_set<char> SPECIAL_OPERATORS{'=', '!', '<', '>'};

    template<typename Fn>
    std::string ReadString(std::istream &input, Fn pred) {
      std::string result;
      char c = -1;
      while (!input.eof()) {
        c = input.get();
        if (!pred(c)) {
          break;
        } else if (c == '\\') {
          char ch = input.get();
          if (SPECIAL_SYMBOLS.count(ch) != 0) {
            if (ch == 'n') {
              ch = '\n';
            } else if (ch == 't') {
              ch = '\t';
            }
            result.push_back(ch);
          }
        } else {
          result.push_back(c);
        }
      }
      if (c != -1 && !pred(c) && c != '\'' && c != '\"') {
        input.putback(c);
      }
      return result;
    }

    bool operator==(const Token &lhs, const Token &rhs) {
      using namespace token_type;

      if (lhs.index() != rhs.index()) {
        return false;
      }
      if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
      }
      if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
      }
      if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
      }
      if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
      }
      return true;
    }

    bool operator!=(const Token &lhs, const Token &rhs) {
      return !(lhs == rhs);
    }

    std::ostream &operator<<(std::ostream &os, const Token &rhs) {
      using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

      VALUED_OUTPUT(Number);
      VALUED_OUTPUT(Id);
      VALUED_OUTPUT(String);
      VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

      UNVALUED_OUTPUT(Class);
      UNVALUED_OUTPUT(Return);
      UNVALUED_OUTPUT(If);
      UNVALUED_OUTPUT(Else);
      UNVALUED_OUTPUT(Def);
      UNVALUED_OUTPUT(Newline);
      UNVALUED_OUTPUT(Print);
      UNVALUED_OUTPUT(Indent);
      UNVALUED_OUTPUT(Dedent);
      UNVALUED_OUTPUT(And);
      UNVALUED_OUTPUT(Or);
      UNVALUED_OUTPUT(Not);
      UNVALUED_OUTPUT(Eq);
      UNVALUED_OUTPUT(NotEq);
      UNVALUED_OUTPUT(LessOrEq);
      UNVALUED_OUTPUT(GreaterOrEq);
      UNVALUED_OUTPUT(None);
      UNVALUED_OUTPUT(True);
      UNVALUED_OUTPUT(False);
      UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

      return os << std::string_view("Unknown token :(");
    }

    bool StringIsComment(const std::string_view str) {
      for (const char c: str) {
        if (c != ' ') {
          if (c == '#') {
            break;
          }
          return false;
        }
      }
      return true;
    }

    bool IsSpecialChar(const char c) {
      return c == '.' || c == ',' || c == '(' || c == '+' || c == ')' || c == '-' || c == '*' || c == '/' || c == ':'
          || c == '@' || c == '%' || c == '$' || c == '^' || c == '&' || c == ';' || c == '?' || c == '=' || c == '<'
          || c == '>' || c == '!' || c == '{' || c == '}' || c == '[' || c == ']';
    }

    Lexer::Lexer(std::istream &input) {
      ParseTextOnTokens(input);
    }

    const Token &Lexer::CurrentToken() const {
      return current_token_;
    }

    Token Lexer::NextToken() {
      if (current_token_index_ < tokens_.size()) {
        current_token_ = tokens_[current_token_index_++];
      }
      return current_token_;
    }

    void Lexer::ParseTextOnTokens(std::istream &input) {
      std::string line;
      while (getline(input, line)) {
        if (!StringIsComment(line)) {
          std::stringstream stream(line);
          ParseString(stream);
          LoadEndl();
        }
      }
      LoadDedent();
      LoadEof();
      NextToken();
    }

    void Lexer::ParseString(std::istream &input) {
      LoadTab(input);
      LoadTokens(input);
    }

    void Lexer::LoadTab(std::istream &input) {
      if (input.peek() == ' ') {
        size_t i = 0;
        do {
          ++i;
          input.ignore();
        } while (input.peek() == ' ');
        if (i > indent_size_) {
          LoadIndent(i);
        } else {
          LoadDedent(i);
        }
      } else {
        LoadDedent();
      }
    }

    void Lexer::LoadTokens(std::istream &input) {
      while (!input.eof()) {
        if (input.peek() == ' ') {
          input.ignore();
        }
        if (input.peek() == '#') {
          break;
        }
        if (isdigit(input.peek())) {
          LoadNumber(input);
        }
        if (input.peek() == '\"' || input.peek() == '\'') {
          LoadString(input);
        }
        if (input.peek() == '_' || isalpha(input.peek())) {
          LoadWord(input);
        }
        if (IsSpecialChar(input.peek())) {
          if (SPECIAL_OPERATORS.count(input.peek()) != 0) {
            LoadSign(input);
          } else {
            LoadChar(input.get());
          }
        }
      }
    }

    void Lexer::LoadNumber(std::istream &input) {
      const std::string number = ReadString(input, [](const char c) {
        return isdigit(c);
      });
      tokens_.push_back(token_type::Number{stoi(number)});
    }

    void Lexer::LoadString(std::istream &input) {
      const std::string dest = ReadString(input, [end = input.get()](const char c) {
        return c != end;
      });
      tokens_.push_back(token_type::String{dest});
    }

    void Lexer::LoadWord(std::istream &input) {
      const std::string word = ReadString(input, [](const char c) {
        return c == '_' || isalnum(c);
      });
      const auto word_it = KEY_WORDS_TOKEN.find(word);
      if (word_it != KEY_WORDS_TOKEN.end()) {
        tokens_.push_back(word_it->second);
      } else {
        tokens_.push_back(token_type::Id{word});
      }
    }

    void Lexer::LoadSign(std::istream &input) {
      const char first = input.get();
      if (input.peek() == '=') {
        const char second = input.get();
        const std::string sign = {first, second};
        tokens_.push_back(SPECIAL_OPERATORS_TOKEN.at(sign));
      } else {
        LoadChar(first);
      }
    }

    void Lexer::LoadEndl() {
      if (!tokens_.empty() && !tokens_.back().Is<token_type::Newline>()) {
        tokens_.push_back(token_type::Newline{});
      }
    }

    void Lexer::LoadDedent(const size_t current_indent) {
      while (indent_size_ > current_indent) {
        indent_size_ -= 2;
        tokens_.push_back(token_type::Dedent{});
      }
    }

    void Lexer::LoadIndent(const size_t current_indent) {
      while (indent_size_ < current_indent) {
        indent_size_ += 2;
        tokens_.push_back(token_type::Indent{});
      }
    }

    void Lexer::LoadChar(const char c) {
      tokens_.push_back(token_type::Char{c});
    }

    void Lexer::LoadEof() {
      tokens_.push_back(token_type::Eof{});
    }

  }  // namespace parse