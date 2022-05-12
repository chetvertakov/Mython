#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

namespace parse {

namespace token_type {
struct Number {  // Лексема «число»
    int value;   // число
};

struct Id {             // Лексема «идентификатор»
    std::string value;  // Имя идентификатора
};

struct Char {    // Лексема «символ»
    char value;  // код символа
};

struct String {  // Лексема «строковая константа»
    std::string value;
};

struct Class {};    // Лексема «class»
struct Return {};   // Лексема «return»
struct If {};       // Лексема «if»
struct Else {};     // Лексема «else»
struct Def {};      // Лексема «def»
struct Newline {};  // Лексема «конец строки»
struct Print {};    // Лексема «print»
struct Indent {};  // Лексема «увеличение отступа», соответствует двум пробелам
struct Dedent {};  // Лексема «уменьшение отступа»
struct Eof {};     // Лексема «конец файла»
struct And {};     // Лексема «and»
struct Or {};      // Лексема «or»
struct Not {};     // Лексема «not»
struct Eq {};      // Лексема «==»
struct NotEq {};   // Лексема «!=»
struct LessOrEq {};     // Лексема «<=»
struct GreaterOrEq {};  // Лексема «>=»
struct None {};         // Лексема «None»
struct True {};         // Лексема «True»
struct False {};        // Лексема «False»
}  // namespace token_type



using TokenBase
    = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
                   token_type::Class, token_type::Return, token_type::If, token_type::Else,
                   token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
                   token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
                   token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
                   token_type::None, token_type::True, token_type::False, token_type::Eof>;

struct Token : TokenBase {
    using TokenBase::TokenBase;

    template <typename T>
    [[nodiscard]] bool Is() const {
        return std::holds_alternative<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T& As() const {
        return std::get<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T* TryAs() const {
        return std::get_if<T>(this);
    }
};

bool operator==(const Token& lhs, const Token& rhs);
bool operator!=(const Token& lhs, const Token& rhs);

std::ostream& operator<<(std::ostream& os, const Token& rhs);

class LexerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Lexer {
public:
    explicit Lexer(std::istream& input);

    // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
    [[nodiscard]] const Token& CurrentToken() const;

    // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
    Token NextToken();

    // Если текущий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& Expect() const {
        using namespace std::literals;
        if (current_token_.Is<T>()) {
            return current_token_.As<T>();
        }
        throw LexerError("Not implemented"s);
    }

    // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void Expect(const U &value) const {
        using namespace std::literals;
        if (current_token_.Is<T>() &&
            current_token_.As<T>().value == value) {
            return;
        }
        throw LexerError("Not implemented"s);
    }

    // Если следующий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& ExpectNext() {
        NextToken();
        return Expect<T>();
    }

    // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void ExpectNext(const U &value) {
        NextToken();
        Expect<T>(value);
    }

private:
    std::istream &input_;
    bool start_of_line_ = true; // Является ли текущая лексема первой на строке
    uint32_t current_indent_ = 0; // текущий отступ
    uint32_t line_indent_ = 0; // кол-во отступов в начале текущей строки
    Token current_token_; // значение текущего токена

    // считывает следующий токен из потока и сохраняет его его в current_token_
    void ReadNextToken();
    // переход к следующей строке
    void NextLine();
    // обрабатывает комментарий
    void ParseComment();
    // обрабатывает конец файла
    void ParseEOF();
    // обрабатывает конец строки
    void ParseLineEnd();
    // обрабатывает пробелы
    void ParseSpaces();
    // обрабатывает отступ в начале строки
    void ParseIndent();
    // обрабатывает значимый токен
    void ParseToken();
    // обрабатывает имя (ключевое слово либо идентификатор)
    void ParseName();
    // обрабатывает символ
    void ParseChar();
};

}  // namespace parse

namespace util {

static std::unordered_map<std::string, parse::Token> KeyWords =
{
 {"class",   parse::token_type::Class{}},
 {"return",  parse::token_type::Return{}},
 {"if",      parse::token_type::If{}},
 {"else",    parse::token_type::Else{}},
 {"def",     parse::token_type::Def{}},
 {"print",   parse::token_type::Print{}},
 {"and",     parse::token_type::And{}},
 {"or",      parse::token_type::Or{}},
 {"not",     parse::token_type::Not{}},
 {"None",    parse::token_type::None{}},
 {"True",    parse::token_type::True{}},
 {"False",   parse::token_type::False{}}
};

static std::unordered_map<std::string, parse::Token> DualSymbols =
{
 {"==",  parse::token_type::Eq{}},
 {"!=",  parse::token_type::NotEq{}},
 {">=",  parse::token_type::GreaterOrEq{}},
 {"<=",  parse::token_type::LessOrEq{}}
};

// проверяет является ли символ цифрой
bool IsNum(char ch);
// проверяет является ли символ буквой
bool IsAlpha(char ch);
// проверяет является ли символ цифрой или буквой
bool IsAlNum(char ch);
// проверяет является ли символ цифрой буквой или знаком подчёркивания
bool IsAlNumLL(char ch);
// считывает строку от открывающейся кавычки до закрывающейся кавычки с учётом экранированных символов
std::string ReadString(std::istream &input);
// считывает идентификатор, состоящий из букв, цифр и символов подчеркивания
std::string ReadName(std::istream &input);
// считывает целое число
int ReadNumber(std::istream &input);
// считает все подряд идущих пробелы в строке и возвращает их кол-во
size_t CountSpaces(std::istream &input);
// считывает строку целиком
std::string ReadLine(std::istream &input);

} // namespace util
