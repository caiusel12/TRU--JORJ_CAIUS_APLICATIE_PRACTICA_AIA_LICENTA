const express = require('express');
const bodyParser = require('body-parser');
const cors = require('cors');
const app = express();

app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true })); 

let latestData = { co2: 0, tvoc: 0, fan: 0, temp: 0, hum: 0 };
let currentCommand = "auto"; 

app.post('/api/command', (req, res) => {
    if (req.body && req.body.cmd) {
        currentCommand = req.body.cmd;
        console.log("=== COMANDĂ NOUĂ PRIMITĂ ==->", currentCommand);
        res.sendStatus(200);
    } else {
        res.status(400).send("Format comanda invalid");
    }
});

app.post('/api/data', (req, res) => {
    latestData = { ...latestData, ...req.body };
    res.json({ cmd: currentCommand });
});

app.get('/api/view', (req, res) => {
    res.json(latestData);
});

app.listen(3000, '0.0.0.0', () => {
    console.log("Server pornit pe portul 3000");
});