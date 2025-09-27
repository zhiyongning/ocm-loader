#ifndef TIMEDOMAIN_PARSER_HPP
#define TIMEDOMAIN_PARSER_HPP

#include <string>

namespace TimeDomainParser {

/**
 * 将 TimeDomain 前缀表达式转换为可读字符串。
 * @param expr TimeDomain 前缀形式表达式 (e.g. "-(d1){w1}(d3){d1}")
 * @return 人类可读字符串 (e.g. "Mon excluding Wed")
 */
std::string TimeDomainToReadable(const std::string& expr);

} // namespace TimeDomainParser

#endif // TIMEDOMAIN_PARSER_HPP