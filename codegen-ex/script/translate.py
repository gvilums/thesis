map_template = """
void {func_name}(const {in_type}* restrict in_ptr, {out_type}* restrict out_ptr) {
    stage0_in_t in;
    stage0_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    {
        // MAP PROGRAM
        {program}
    }
    memcpy(out_ptr, &out, sizeof(out));
}
"""

filter_template = """
int {func_name}(const {in_type}* restrict in_ptr) {
    stage1_in_t in;
    memcpy(&in, in_ptr, sizeof(in));
    {
        // FILTER PROGRAM
        {program}
    }
}
"""