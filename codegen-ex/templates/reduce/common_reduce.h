<%inherit file="common.h"/>

<%block name="specific_defs">
#define REDUCTION_VAR_COUNT ${ pipeline["reduction_vars"] }
${ parent.create_typedef("reduction_in_t", f"stage_{reduction['input_idx']}_out_t") }
${ parent.create_typedef("reduction_out_t", reduction["output"]) }
typedef reduction_out_t output_t;
</%block>