// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// Runs a Game Bits page (a windowed program in a web build) in headless
// Chromium, for tools/run-program.sh, which runs this under xvfb-run and says
// what a run does. Playwright must be found by require('playwright').
//
// Usage:
//   node tools/run-page.js --page <name>.html --out-dir <dir>
//       --timeout-secs <seconds> --timeout-file <file>
//       [--script <file> | --press-keys "<key>..."] [-- <argument>...]
//
// The page's folder is served from localhost, and the page is opened with
// the arguments after "--" as main()'s. With --script, the script is given to
// the page as /pilot/script.txt, the page is also given --pilot
// /pilot/script.txt --pilot-output /pilot/out, and once the program has
// stopped, the files it wrote to /pilot/out are copied to <out-dir>. With
// --press-keys, each key (by Playwright's name, such as Escape) is pressed in
// turn once the page has run for a second.
//
// Console messages are written to <out-dir>/stdout.txt, and console errors and
// page errors to <out-dir>/stderr.txt. This exits with the program's exit
// code, 134 if it aborted, or 1 if it is still running after <timeout-secs>,
// in which case <timeout-file> is created.

'use strict';

const fs = require('fs');
const http = require('http');
const path = require('path');
const { chromium } = require('playwright');

// Gives Chromium a WebGPU device without a GPU, on SwiftShader.
const kBrowserArgs = [
  '--enable-unsafe-webgpu',
  '--use-angle=swiftshader',
  '--enable-unsafe-swiftshader',
  '--ignore-gpu-blocklist',
  '--enable-gpu',
  '--enable-features=Vulkan',
  '--use-vulkan=swiftshader',
];

const kContentTypes = {
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.wasm': 'application/wasm',
};

// The exit code of a program that aborted, as on Linux (128 + SIGABRT).
const kAbortExitCode = 134;

const kScriptPath = '/pilot/script.txt';
const kOutputFolder = '/pilot/out';

// Keeps the output folder there however the program ends, as an error from the
// page's file system once the program has stopped aborts the page.
const kOutputKeep = '.keep';

const kPressKeysDelayMs = 1000;

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  const options = {programArgs: []};
  for (let i = 0; i < argv.length; ++i) {
    if (argv[i] === '--') {
      options.programArgs = argv.slice(i + 1);
      break;
    }
    const name = argv[i].replace(/^--/, '');
    if (i + 1 >= argv.length) {
      fail(`${argv[i]} needs a value.`);
    }
    options[name.replace(/-(\w)/g, (_, c) => c.toUpperCase())] = argv[++i];
  }
  for (const name of ['page', 'outDir', 'timeoutSecs', 'timeoutFile']) {
    if (options[name] === undefined) {
      fail(`--${name.replace(/[A-Z]/g, (c) => '-' + c.toLowerCase())} is ` +
           'required.');
    }
  }
  return options;
}

// Serves `folder` from localhost on a free port, resolving to the server.
function serve(folder) {
  const server = http.createServer((request, response) => {
    const name = decodeURIComponent(new URL(request.url, 'http://x').pathname);
    const file = path.join(folder, path.normalize(name));
    if (!file.startsWith(folder + path.sep)) {
      response.writeHead(404);
      response.end();
      return;
    }
    fs.readFile(file, (error, data) => {
      if (error) {
        response.writeHead(404);
        response.end();
        return;
      }
      const type =
          kContentTypes[path.extname(file)] || 'application/octet-stream';
      response.writeHead(200, {'Content-Type': type});
      response.end(data);
    });
  });
  return new Promise((resolve) => server.listen(0, 'localhost', () => {
    resolve(server);
  }));
}

// Copies every file the program wrote to the page's output folder to
// `outDir`.
async function copyOutput(page, outDir) {
  const files = await page.evaluate(([folder, keep]) => {
    const result = [];
    for (const name of Module.FS.readdir(folder)) {
      if (name === '.' || name === '..' || name === keep) {
        continue;
      }
      const filePath = folder + '/' + name;
      if (!Module.FS.isFile(Module.FS.stat(filePath).mode)) {
        continue;
      }
      const bytes = Module.FS.readFile(filePath);
      let text = '';
      for (let i = 0; i < bytes.length; i += 0x8000) {
        text += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
      }
      result.push({name: name, data: btoa(text)});
    }
    return result;
  }, [kOutputFolder, kOutputKeep]);
  for (const file of files) {
    fs.writeFileSync(path.join(outDir, file.name),
                     Buffer.from(file.data, 'base64'));
  }
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const pagePath = path.resolve(options.page);
  const timeoutMs = Number(options.timeoutSecs) * 1000;
  const stdout = fs.openSync(path.join(options.outDir, 'stdout.txt'), 'w');
  const stderr = fs.openSync(path.join(options.outDir, 'stderr.txt'), 'w');

  const files = {};
  const args = [...options.programArgs];
  if (options.script !== undefined) {
    files[kScriptPath] = fs.readFileSync(options.script, 'utf8');
    files[`${kOutputFolder}/${kOutputKeep}`] = '';
    args.push('--pilot', kScriptPath, '--pilot-output', kOutputFolder);
  }

  const server = await serve(path.dirname(pagePath));
  const browser = await chromium.launch({args: kBrowserArgs});
  let exitCode = 1;
  try {
    const page = await browser.newPage();
    page.on('console', (message) => {
      const file = message.type() === 'error' ? stderr : stdout;
      fs.writeSync(file, message.text() + '\n');
    });
    page.on('pageerror', (error) => {
      fs.writeSync(stderr, `Page error: ${error.message}\n`);
    });
    await page.addInitScript((given) => {
      globalThis.gbFiles = given;
    }, files);

    const query = args.map((arg) => 'arg=' + encodeURIComponent(arg));
    const address = `http://localhost:${server.address().port}/` +
        `${encodeURIComponent(path.basename(pagePath))}?${query.join('&')}`;
    const started = Date.now();
    await page.goto(address);
    if (options.pressKeys !== undefined) {
      await page.waitForTimeout(kPressKeysDelayMs);
      for (const key of options.pressKeys.split(/\s+/).filter(Boolean)) {
        await page.keyboard.press(key);
      }
    }

    const remainingMs = Math.max(0, timeoutMs - (Date.now() - started));
    let stopped = true;
    await page
        .waitForFunction(() => globalThis.gbExit !== undefined, null,
                         {timeout: remainingMs, polling: 100})
        .catch(() => {
          stopped = false;
        });
    if (!stopped) {
      fs.writeFileSync(options.timeoutFile, '');
      return 1;
    }
    const result = await page.evaluate(() => globalThis.gbExit);
    if (typeof result === 'number') {
      exitCode = result;
    } else {
      fs.writeSync(stderr, `Aborted: ${result}\n`);
      exitCode = kAbortExitCode;
    }
    if (options.script !== undefined) {
      await copyOutput(page, options.outDir);
    }
  } finally {
    await browser.close();
    server.close();
    fs.closeSync(stdout);
    fs.closeSync(stderr);
  }
  return exitCode;
}

main().then((code) => process.exit(code), (error) => fail(String(error)));
