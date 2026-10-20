// Checks the config.js the Windows installer writes.
// Run: node test/check-config.js <path to installed config.js> <expected presets folder> <true|false: inside Serum 2 presets>
'use strict';
const fs = require('fs');
const vm = require('vm');

const [file, expectedDir, expectedInSerum] = process.argv.slice(2);
const sandbox = { window: {} };
vm.runInNewContext(fs.readFileSync(file, 'utf8'), sandbox);
const cfg = sandbox.window.PRESET_MAKER_CONFIG;
console.log(JSON.stringify(cfg));
if (!cfg) throw new Error('config.js did not set window.PRESET_MAKER_CONFIG');
if (cfg.presetDir !== expectedDir) throw new Error('presetDir is ' + cfg.presetDir + ', expected ' + expectedDir);
if (String(cfg.insideSerumPresets) !== expectedInSerum) throw new Error('insideSerumPresets is ' + cfg.insideSerumPresets + ', expected ' + expectedInSerum);
console.log('config.js OK');
