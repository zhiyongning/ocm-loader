#include <iostream>
#include "TimeDomainParser.hpp" 

int main() {
    std::vector<std::string> examples = {
        "[(d1){w1}]+[(d2){w1}]+[(d3){w1}]+[(d4){w1}]+[(d5){w1}]",   // 1 week days
        "[(d6){w1}]+[(d7){w1}]",                                   // 2 weekend
        "[(t7){d1}]-[(t9){d1}]",                                   // 3 daily 7-9
        "[(t17){d1}]-[(t19){d1}]",                                 // 4 daily 17-19
        "[(d1){w1}*[(t0){d1}]-[(t6){d1}]]",                        // 5 Monday 0-6
        "[(M5D1){y1}]",                                            // 6 May 1 yearly
        "[(M6){M3}]",                                              // 7 Jun-Aug
        "[(d2){w1}*[(t22){d1}]-[(t24){d1}]]+[(d4){w1}*[(t22){d1}]-[(t24){d1}]]+[(d3){w1}*[(t0){d1}]-[(t5){d1}]]", // 8 complex nights
        "[(t0){d1}]-[(t24){d1}]",                                  // 9 24 hours
        "[(d6){w2}]",                                              // 10 every-2-weeks saturday
        "[[(d1){w1}]*[(d3){-w1}]]",
        "*(d1){w1}(d3){-w1}"

    };

    for (size_t i = 0; i < examples.size(); ++i) {
        std::cout << "Input " << (i+1) << ": " << examples[i] << "\n";
        std::cout << "Output: " << TimeDomainParser::TimeDomainToReadable(examples[i]) << "\n\n";
    }
    return 0;
}