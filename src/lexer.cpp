#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std::literals;

namespace util {
    extern std::unordered_map<std::string, parse::Token> KeyWords;
    extern std::unordered_map<std::string, parse::Token> DualSymbols;
}

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
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

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
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

    return os << "Unknown token :("sv;
}

Lexer::Lexer(std::istream& input) : input_{input} {
    ReadNextToken();
}

const Token& Lexer::CurrentToken() const {
    return current_token_;
}

Token Lexer::NextToken() {
    ReadNextToken();
    return current_token_;
}

void Lexer::ReadNextToken() {
    char ch = input_.peek();
    if (ch == std::ios::traits_type::eof()) { // дошли до конца файла
        ParseEOF();
    }
    else if (ch == '\n') { // дошли до конца строки
        ParseLineEnd();
    }
    else if (ch == '#') { // дошли до комментария
        ParseComment();
    }
    else if (ch == ' ') { // дошли до пробела
        ParseSpaces();
    }
    // если в начале строки есть отступы - выдаём отступ, пока текущий отступ не сравняется с отступом в строке
    else if (start_of_line_ && (current_indent_ != line_indent_)) {
        ParseIndent();
    }
    else { // дальше должен быть значимый токен
        ParseToken();
        start_of_line_ = false; // после первого значимого токена считаем, что мы уже не в начале строки
    }
}

void Lexer::NextLine() {
    util::ReadLine(input_);
    start_of_line_ = true;
    line_indent_ = 0;
}

void Lexer::ParseComment() {
    char c;
    while (input_.get(c)) {
        if (c == '\n') {
            input_.putback(c);
            break;
        }
    }
    ReadNextToken();
}

void Lexer::ParseEOF() {
    if (!start_of_line_) { // если конец файла в конце непустой строки - возвращаем Newline и переходим на следующую строку
        NextLine();
        current_token_ = token_type::Newline{};
    } else { // иначе возвращаем Eof предварительно закрыв незакрытые отступы
        if (current_indent_ > 0) {
            --current_indent_;
            current_token_ = token_type::Dedent{};
        } else {
            current_token_ = token_type::Eof{};
        }
    }
}

void Lexer::ParseLineEnd() {
    if (start_of_line_) { // если конец строки на пустой строке - пропускаем строку и считываем по-новой
        NextLine();
        ReadNextToken();
    } else { // иначе возвращаем Newline
        NextLine();
        current_token_ = token_type::Newline{};
    }
}

void Lexer::ParseSpaces() {
    auto spaces_count = util::CountSpaces(input_);
    if (start_of_line_) { // если это начало строки - записываем отступ
        line_indent_ = spaces_count / 2;
    }
    ReadNextToken(); // пропускаем пробелы и считываем по-новой
}

void Lexer::ParseIndent() {
    if (current_indent_ < line_indent_) {
        ++current_indent_;
        current_token_ = token_type::Indent{};
    } else if (current_indent_ > line_indent_) {
        --current_indent_;
        current_token_ = token_type::Dedent{};
    }
}

void Lexer::ParseToken() {
    char ch = input_.peek();
    if (util::IsNum(ch)) {   // если следующий токен - число
        current_token_ = token_type::Number{util::ReadNumber(input_)};
    } else if (util::IsAlNumLL(ch)) {    // если следующий токен - имя
        ParseName();
    } else if (ch == '\"' || ch == '\'') { // если следующий токен строка
        current_token_ = token_type::String{util::ReadString(input_)};
    } else { // во всех остальных случаях считаем что следующий токен - символ
        ParseChar();
    }
}

void Lexer::ParseName() {
    auto name = util::ReadName(input_);
    if (util::KeyWords.count(name) != 0) { // если это ключевое слово - присваиваем соответсвующий токен
        current_token_ = util::KeyWords.at(name);
    } else { // иначе считаем что это Id
        current_token_ = token_type::Id{name};
    }
}

void Lexer::ParseChar() {
    std::string sym_pair;
    sym_pair += input_.get();
    sym_pair += input_.peek();
    if (util::DualSymbols.count(sym_pair)) { // если пара символов подряд - это лексема, то присваиваем
        current_token_ = util::DualSymbols.at(sym_pair);
        input_.get();
    } else { // иначе берём только один символ
      current_token_ = token_type::Char{sym_pair[0]};
    }
}

}  // namespace parse

namespace util {

bool IsNum(char ch) {
    return std::isdigit(static_cast<unsigned char>(ch));
}

bool IsAlpha(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch));
}

bool IsAlNum(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch));
}

bool IsAlNumLL(char ch) {
    return IsAlNum(ch) || ch == '_';
}

std::string ReadString(std::istream& input) {
    std::string line;
    // считываем поток посимвольно, до конца строки либо до неэкранированной закрывающей кавычки
    char first = input.get(); // первая кавычка : ' или "
    char c;
    while (input.get(c)) {
        if (c == '\\') {
            char next;
            input.get(next);
            if (next == '\"') {
                line += '\"';
            } else if (next == '\'') {
                line += '\'';
            } else if (next == 'n') {
                line += '\n';
            } else if (next == 't') {
                line += '\t';
            }
        } else {
            if (c == first) {
                break;
            }
            line += c;
        }
    }
    // если последний знак был не закрывающей кавчикой, значит строка составлена некорректно
    if (c != first) {
        throw std::runtime_error("Failed to parse string : "s + line);
    }
    return line;
}

std::string ReadName(std::istream &input) {
    std::string line;
    char c;
    while (input.get(c)) {
        if (IsAlNumLL(c)) {
            line += c;
        } else {
            input.putback(c);
            break;
        }
    }
    return line;
}

size_t CountSpaces(std::istream &input) {
    size_t result = 0;
    char c;
    while (input.get(c)) {
        if (c == ' ') {
            ++result;
        } else {
            input.putback(c);
            break;
        }
    }
    return result;
}

std::string ReadLine(std::istream &input) {
    std::string s;
    getline(input, s);
    return s;
}

int ReadNumber(std::istream &input) {
    std::string num;
    char c;
    while(input.get(c)) {
        if (IsNum(c)) {
            num += c;
        } else {
            input.putback(c);
            break;
        }
    }
    if (num.empty()) {
        return 0;
    }
    return std::stoi(num);
}

} // namespace util
