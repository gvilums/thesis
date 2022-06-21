<%inherit file="common.h"/>

<%block name="specific_defs">
${ parent.create_typedef("output_t", f"stage_{ len(stages) - 1 }_out_t") }
</%block>