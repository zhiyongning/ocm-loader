#include "TimeDomainParser.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <cctype>
namespace TimeDomainParser {

// ------------------- Tokenizer -------------------
enum class TokenType { Operator, Term, End };

struct Token {
    TokenType type;
    char op;
    std::string term;
};

class Tokenizer {
public:
    Tokenizer(const std::string& s) : str(s), pos(0), len((int)s.size()) {}

    Token next() {
        skipSpaces();
        if (pos >= len) return {TokenType::End, 0, ""};

        char c = str[pos];
        if (c == '+' || c == '*' || c == '-' || c == '!') { pos++; return {TokenType::Operator, c, ""}; }
        if (c == '(') return parseTerm();
        if (std::isalnum((unsigned char)c)) return parsePlainTerm();

        pos++; 
        return next();
    }

    Token peek() { int saved = pos; Token t = next(); pos = saved; return t; }
    bool hasMore() { skipSpaces(); return pos < len; }

private:
    Token parseTerm() {
        pos++; 
        std::string inner;
        while (pos < len && str[pos] != ')') inner.push_back(str[pos++]);
        if (pos < len && str[pos] == ')') pos++;
        skipSpaces();
        if (pos < len && str[pos] == '{') { while (pos < len && str[pos] != '}') pos++; if (pos < len) pos++; }
        trim(inner);
        std::string primary = extractPrimary(inner);
        return {TokenType::Term, 0, primary};
    }

    Token parsePlainTerm() {
        std::string t;
        while (pos < len && (std::isalnum((unsigned char)str[pos]) || str[pos]=='_')) t.push_back(str[pos++]);
        return {TokenType::Term, 0, t};
    }

    void skipSpaces() { while (pos < len && std::isspace((unsigned char)str[pos])) pos++; }
    static void trim(std::string &s) { size_t a = 0; while (a<s.size() && std::isspace((unsigned char)s[a])) ++a; size_t b = s.size(); while(b>a && std::isspace((unsigned char)s[b-1])) --b; s=s.substr(a,b-a);}
    static std::string extractPrimary(const std::string& s) { size_t i=0; while(i<s.size() && s[i]!=',' && !std::isspace((unsigned char)s[i])) ++i; return s.substr(0,i); }

    const std::string& str;
    int pos;
    int len;
};

// ------------------- Token 翻译 -------------------
static std::unordered_map<std::string, std::string> weekdayMap = {
    {"d1","Mon"},{"d2","Tue"},{"d3","Wed"},{"d4","Thu"},{"d5","Fri"},{"d6","Sat"},{"d7","Sun"}
};
static std::unordered_map<std::string, std::string> monthMap = {
    {"M1","Jan"},{"M2","Feb"},{"M3","Mar"},{"M4","Apr"},{"M5","May"},{"M6","Jun"},
    {"M7","Jul"},{"M8","Aug"},{"M9","Sep"},{"M10","Oct"},{"M11","Nov"},{"M12","Dec"}
};

static std::string translateToken(const std::string& tk) {
    if (tk.empty()) return "";
    auto itw = weekdayMap.find(tk); if(itw!=weekdayMap.end()) return itw->second;
    auto itm = monthMap.find(tk); if(itm!=monthMap.end()) return itm->second;
    if(tk[0]=='h') return tk.substr(1)+":00";
    if(tk[0]=='t') return "day "+tk.substr(1);
    if(tk[0]=='w') return "every "+tk.substr(1)+" weeks";
    if(tk[0]=='z') return tk=="z1"?"Sunrise to Sunset":"Sunset to Sunrise";
    if(tk[0]=='-') return "EXCEPT "+tk.substr(1);
    return tk;
}

// ------------------- 解析 -------------------
std::string parseExpr(Tokenizer &tz);

static std::string joinWith(const std::vector<std::string>& parts, const std::string& sep) {
    std::ostringstream oss; for(size_t i=0;i<parts.size();++i){ if(i)oss<<sep; oss<<parts[i]; } return oss.str();
}

std::string parseTerm(Tokenizer &tz, const Token &t) { return translateToken(t.term); }

std::string parseExpr(Tokenizer &tz) {
    Token t = tz.next(); if(t.type==TokenType::End) return "";
    if(t.type==TokenType::Term) return parseTerm(tz,t);
    if(t.type==TokenType::Operator){
        char op=t.op;
        if(op=='!') return "NOT("+parseExpr(tz)+")";
        else if(op=='-') return "From "+parseExpr(tz)+" To "+parseExpr(tz);
        else if(op=='*'){ std::vector<std::string> parts; parts.push_back(parseExpr(tz)); parts.push_back(parseExpr(tz));
            while(tz.hasMore()){ std::string p=parseExpr(tz); if(p.empty()) break; parts.push_back(p); } return "("+joinWith(parts," AND ")+")"; }
        else if(op=='+'){ std::vector<std::string> parts; parts.push_back(parseExpr(tz)); parts.push_back(parseExpr(tz));
            while(tz.hasMore()){ std::string p=parseExpr(tz); if(p.empty()) break; parts.push_back(p); } return "("+joinWith(parts," OR ")+")"; }
        else return std::string("<op ")+op+">";
    }
    return "<unknown>";
}

// ------------------- API -------------------
std::string TimeDomainToReadable(const std::string &expr) {
    Tokenizer tz(expr); std::vector<std::string> parts;
    while(tz.hasMore()){ std::string p=parseExpr(tz); if(!p.empty()) parts.push_back(p); else break; }
    if(parts.empty()) return "";
    if(parts.size()==1) return parts[0];
    return joinWith(parts,"; ");
}


} // namespace TimeDomainParser