const char webpageCont[] PROGMEM =
    R"=====(
<!DOCTYPE HTML>
<html>
<title>ESP32_W5500 AsyncSocketServer</title>

<!---------------------------CSS-------------------------->

<style>

    h1 {font-size: 40px; color: red; text-align: center}

</style>

<!--------------------------HTML-------------------------->

<img id="image" />

<!----------------------JavaScript------------------------>

<script>

var websoc = new WebSocket('ws://' + window.location.hostname + ':80/ws');

// Fonction pour convertir un tableau d'octets en une chaîne d'URL Data
function arrayBufferToBase64(buffer) {
    var binary = '';
    var bytes = new Uint8Array(buffer);
    var len = bytes.byteLength;
    for (var i = 0; i < len; i++) {
        binary += String.fromCharCode(bytes[i]);
    }
    return 'data:image/jpeg;base64,' + btoa(binary);
}

websoc.binaryType = 'arraybuffer';

websoc.onmessage = function (event) {
    // Convertir les données binaires en une URL Data pour l'image
    var imageDataUrl = arrayBufferToBase64(event.data);

    // Mettre à jour la source de l'image
    document.getElementById('image').src = imageDataUrl;
};

</script>
</html>
)=====";
