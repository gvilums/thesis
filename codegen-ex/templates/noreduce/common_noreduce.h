<%inherit file="common.h"/>

<%block name="specific_defs">
${ parent.create_typedef("output_t", f"stage_{ stages[-1]['id'] }_out_t") }
</%block>