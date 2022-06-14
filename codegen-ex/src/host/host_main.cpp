#include <cstdint>
#include <dpu>
#include <iostream>
#include <vector>
#include <array>

using namespace dpu;

int main() {
    constexpr size_t elem_count = 1000;

    std::vector<uint32_t> input;
    // number of elements
    input.push_back(elem_count);
    // comparison value
    input.push_back(0);
    // data
    for (size_t i = 0; i < elem_count * 2; ++i) {
        input.push_back(1 + i % 2);
    }

    try {
        auto dpu = DpuSet::allocate(1, "backend=simulator");
        dpu.load("device");

        dpu.copy("input_data_buffer", input);

        dpu.exec();
        dpu.log(std::cout);

        std::vector<std::vector<uint32_t>> incoming(1);
        incoming[0].resize(1);
        dpu.copy(incoming, "reduction_output");
        std::cout << incoming[0][0] << std::endl;
    } catch (const DpuError& e) {
        std::cerr << "an exception occurred: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
