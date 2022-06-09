#include <cstdint>
#include <dpu>
#include <iostream>
#include <vector>
#include <array>

using namespace dpu;

int main() {
    constexpr size_t elem_count = 1000;

        std::vector<std::array<uint32_t, 2>>
            input_data(elem_count, {1, 2});
    std::vector<uint32_t> comparison(1, 0);
    std::vector<uint32_t> zero_vec(1, 0);
	std::vector<uint32_t> elem_count_vec(1, elem_count);

    try {
        auto dpu = DpuSet::allocate(1, "backend=simulator");
        dpu.load("device");

        dpu.copy("input_data_buffer", input_data);
		dpu.copy("total_input_elems", elem_count_vec);
        dpu.copy("comparison", comparison);
        dpu.copy("zero_vec", zero_vec);

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
