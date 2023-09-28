function setup() {
    var local = true;

    content = document.getElementById("content");
    var box = document.getElementById('image-box');

    var help = document.createElement('div');
    var helpText = "Use mouse wheel to zoom in/out, click and drag to pan. Press keys [1], [2], ... to switch between individual images.";
    if (!local) {
        helpText += " Press [?] to see more keybindings.";
    }
    help.appendChild(document.createTextNode(helpText));
    help.className = "help";
    box.appendChild(help);

    if (local) {
        new ImageBox(content, data['imageBoxes']);
    } else {
        var jeri = document.createElement('div');
        jeri.className = "jeri";
        var viewer = Jeri.renderViewer(jeri, data["jeri"]);
        box.appendChild(jeri);

        viewer.setState({ activeRow: 1 });
        
        content.appendChild(box);
    }
    
    // new ChartBox(content, data["stats"]);
    new TableBox(content, "Statistics", data["stats"]);
}