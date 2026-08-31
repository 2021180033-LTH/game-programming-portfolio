shader_type spatial;
render_mode cull_disabled, depth_draw_opaque;

// Reusable, texture-free path surface for the first EverPlanet visual pass.
// The region's CSGSphere remains the only walking collision. This shader only
// changes the appearance of the visual PlaneMesh placed a few centimetres above it.

uniform vec4 base_color : source_color = vec4(0.52, 0.38, 0.13, 1.0);
uniform vec4 variation_color : source_color = vec4(0.66, 0.49, 0.19, 1.0);
uniform vec4 edge_color : source_color = vec4(0.34, 0.25, 0.09, 1.0);
uniform float color_variation : hint_range(0.0, 0.45, 0.01) = 0.16;
uniform float edge_wobble : hint_range(0.0, 0.20, 0.005) = 0.075;
uniform float edge_feather : hint_range(0.005, 0.12, 0.005) = 0.025;
uniform float edge_tint_strength : hint_range(0.0, 1.0, 0.01) = 0.32;
uniform float curvature_strength : hint_range(0.0, 0.02, 0.0001) = 0.00625;
uniform float roughness_value : hint_range(0.0, 1.0, 0.01) = 0.96;

varying vec3 path_world_position;

float hash21(vec2 p) {
	p = fract(p * vec2(123.34, 345.45));
	p += dot(p, p + 34.345);
	return fract(p.x * p.y);
}

float value_noise(vec2 p) {
	vec2 cell = floor(p);
	vec2 local = fract(p);
	local = local * local * (3.0 - 2.0 * local);
	float a = hash21(cell);
	float b = hash21(cell + vec2(1.0, 0.0));
	float c = hash21(cell + vec2(0.0, 1.0));
	float d = hash21(cell + vec2(1.0, 1.0));
	return mix(mix(a, b, local.x), mix(c, d, local.x), local.y);
}

void vertex() {
	// A radius-80 m sphere has a local sag of approximately r^2/(2R).
	// Subdivided planes follow that profile closely enough to avoid buried corners.
	VERTEX.y -= dot(VERTEX.xz, VERTEX.xz) * curvature_strength;
	path_world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 centered_uv = UV * 2.0 - 1.0;
	// World-space noise keeps colour continuous where reusable path pieces overlap.
	float broad_noise = value_noise(path_world_position.xz * 0.28);
	float fine_noise = value_noise(path_world_position.xz * 0.92 + vec2(17.3, -8.7));

	// Pieces are authored as longitudinal strips. Only the two side banks are cut;
	// their ends stay open and overlap their neighbours without dark transverse seams.
	float end_taper = smoothstep(0.68, 1.0, abs(centered_uv.y)) * 0.065;
	float width_noise = (broad_noise - 0.5) + (fine_noise - 0.5) * 0.35;
	float half_width = 0.91 - end_taper + width_noise * edge_wobble;
	float outline_distance = abs(centered_uv.x) - half_width;
	float shape_mask = 1.0 - smoothstep(-edge_feather, edge_feather, outline_distance);

	// Alpha scissor keeps the patch in the opaque path while giving its silhouette
	// a rounded, slightly irregular edge instead of a visible rectangular tile.
	ALPHA = shape_mask;
	ALPHA_SCISSOR_THRESHOLD = 0.5;

	float variation = clamp((broad_noise * 0.67 + fine_noise * 0.33 - 0.5) * 2.0, -1.0, 1.0);
	vec3 surface_color = mix(base_color.rgb, variation_color.rgb, 0.5 + variation * color_variation);
	float edge_band = smoothstep(-0.2, -0.015, outline_distance);
	surface_color = mix(surface_color, edge_color.rgb, edge_band * edge_tint_strength);

	ALBEDO = surface_color;
	ROUGHNESS = roughness_value;
	SPECULAR = 0.08;
}
