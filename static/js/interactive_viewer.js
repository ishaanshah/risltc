function setup() {
    var local = true;

    content = document.getElementById("content");
    var box = document.getElementById('image-box');

    if (local) {
        scenes = [
            ["Zero-Day (Glossy)", "scenes/zero-day-(etg)/"],
            ["Bistro Exterior (Glossy)", "scenes/bistro-exterior-(etg)/"],
            ["Bistro Interior (Glossy)", "scenes/bistro-interior-(etg)/"],
            ["Zero-Day (Diffuse)", "scenes/zero-day-(etd)/"],
            ["Bistro Exterior (Glossy)", "scenes/bistro-exterior-(etd)/"],
            ["Bistro Interior (Glossy)", "scenes/bistro-interior-(etd)/"],
        ];
        new ImageBox(content, data['imageBoxes'], scenes);
    } else {
        var jeri = document.createElement('div');
        jeri.className = "jeri";
        var viewer = Jeri.renderViewer(jeri, data["jeri"]);
        box.appendChild(jeri);

        viewer.setState({ activeRow: 1 });
        
        content.appendChild(box);
    }
}