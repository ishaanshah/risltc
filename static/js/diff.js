const diffString = `diff --git a/old.glsl b/new.glsl
index 665618b..4ffa51a 100644
--- a/ris.glsl
+++ b/risltc.glsl
@@ -1,23 +1,24 @@
 struct reservoir_t {
     // The running sum of all the weights
     float w_sum;
-    // Keep track of chosen position
-    vec3 light_sample;
     // Keep track of current chosen light
     int light_index;
+    // Additionally, we keep track of the sample value
+    // to avoid recomputing LTCs for chosen sample
+    float sample_value;
 };

 void initialize_reservoir(inout reservoir_t reservoir) {
     reservoir.w_sum = 0;
     reservoir.light_index = -1;
-    reservoir.light_sample = vec3(0);
+    reservoir.sample_value = 0;
 }

-void insert_in_reservoir(inout reservoir_t reservoir, float w, int light_index, vec3 light_sample, float rand) {
+void insert_in_reservoir(inout reservoir_t reservoir, float w, int light_index, float sample_value, float rand) {
     reservoir.w_sum += w;
     if (w > 0 && rand < (w / reservoir.w_sum)) {
-        reservoir.light_sample = light_sample;
         reservoir.light_index = light_index;
+        reservoir.sample_value = sample_value;
     }
 }

@@ -26,21 +27,19 @@ void main() {
     initialize_reservoir(res);
     for (int i = 0; i < NUM_CANDIDATES; i += 1) {
         int light_idx;
-        vec3 light_pos;
         float pdf;
-        // Randomly sample a light and a point on it 
-        sampleLightPos(light_idx, light_pos, pdf, rng());
+        // Randomly sample a light only
+        sampleLight(light_idx, pdf, rng());
-        // Evaluate the integrand at sample without visibility
-        vec3 color = evaluate_shading_no_vis(shading_data, light_idx, light_pos);
+        // Evaluate LTC of the light at shading point
+        vec3 color = evaluate_ltc(shading_data, light_idx);
         float p_hat = length(color);
         float w = p_hat / pdf;
-        insert_in_reservoir(res, w, light_idx, light_sample, rng());
+        insert_in_reservoir(res, w, light_idx, p_hat, rng());
     }

     if (res.light_index >= 0) {
         bool visibility;
-        // Evaluate the integrand at sample with visibility
-        vec3 color = evaluate_shading_vis(shading_data, res.light_idx, res.light_pos, visibility);
-        float p_hat = len(color);
+        // Evaluate integrand using ProjLTC [Peters, 2021]
+        vec3 color = evaluate_projltc(shading_data, res.light_idx, visibility);
         if (visibility) {
-            float W = res.w_sum / (m * p_hat);
+            // Note that we have to divide with
+            // the LTC evaluation and not len(color)
+            float W = res.w_sum / (m * res.sample_value);
         } else {
             W = 0.0;
         }`;

document.addEventListener('DOMContentLoaded', function () {
var targetElement = document.getElementById('code-diff');
var configuration = {
    drawFileList: false,
    fileListToggle: false,
    fileListStartVisible: false,
    fileContentToggle: false,
    matching: 'words',
    synchronisedScroll: true,
    highlight: true,
    renderNothingWhenEmpty: false,
    outputFormat: 'side-by-side',
    wordWrap: true,
};
var diff2htmlUi = new Diff2HtmlUI(targetElement, diffString, configuration);
diff2htmlUi.draw();
diff2htmlUi.highlightCode();
});