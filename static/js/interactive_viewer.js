function setup() {
    var local = true;

    content = document.getElementById("content-time");

    scenes = [
        ["Zero-Day (Glossy)", "scenes/zero-day-(etg)/"],
        ["Bistro Exterior (Glossy)", "scenes/bistro-exterior-(etg)/"],
        ["Bistro Interior (Glossy)", "scenes/bistro-interior-(etg)/"],
        ["Zero-Day (Diffuse)", "scenes/zero-day-(etd)/"],
        ["Bistro Exterior (Glossy)", "scenes/bistro-exterior-(etd)/"],
        ["Bistro Interior (Glossy)", "scenes/bistro-interior-(etd)/"],
    ];
    new ImageBox(content, data['imageBoxes'], scenes, "time")

    content = document.getElementById("content-sample");
    scenes = [
        ["Zero-Day (Glossy)", "scenes/zero-day/"],
        ["Bistro Exterior (Glossy)", "scenes/bistro-exterior/"],
        ["Bistro Interior (Glossy)", "scenes/bistro-interior/"],
        ["Zero-Day (Diffuse)", "scenes/zero-day-(diffuse)/"],
        ["Bistro Exterior (Glossy)", "scenes/bistro-exterior-(diffuse)/"],
        ["Bistro Interior (Glossy)", "scenes/bistro-interior-(diffuse)/"],
    ];
    new ImageBox(content, data['imageBoxes'], scenes, "sample")
}