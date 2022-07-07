<%inherit file="host.h"/>

<%block name="process_decl">
int process(output_t* output\
% for i in range(0, len(in_stage["inputs"])):
, const input_${ i }_t* input_${ i }\
% endfor
, size_t elem_count\
% for global_value in pipeline["globals"]:
, const global_${ loop.index }_t* global_${ loop.index }\
% endfor
);
</%block>
