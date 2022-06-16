mod templates;

enum PipelineStage {
    
}


fn main() {
    let map_func = format!(
        "
void {func_name}(const {in_type}* restrict in_ptr, {out_type}* restrict out_ptr) {{
    stage0_in_t in;
    stage0_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    {{
        // MAP PROGRAM
        {program}
    }}
    memcpy(out_ptr, &out, sizeof(out));
}}

",
        func_name = "first_map",
        in_type = "map_in_t",
        out_type = "map_out_t",
        program = ""
    );
    println!("{map_func}");
}
