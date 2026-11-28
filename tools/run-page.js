// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// Runs a Game Bits page (a windowed program in a web build) in headless
// Chromium for tools/run-program.sh, which says what a run does and runs this
// under xvfb-run. Playwright must be found by require('playwright').
//
// Usage:
//   node tools/run-page.js --page <name>.html --out-dir <dir>
//       --timeout-secs <seconds> --timeout-file <file>
//       [--script <file> | --press-keys "<key>..."] [-- <argument>...]
//
// The arguments after "--" are main()'s. This exits with the program's exit
// code, 134 if it aborted, or 1 if it is still running after <timeout-secs>,
// in which case <timeout-file> is created. The page and its console follow
// CMake/GameBitsShell.html.

'use strict';

const fs = require('fs');
const path = require('path');
const util = require('util');
const {chromium, errors} = require('playwright');

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

// The page is served from here by Playwright itself, which answers each
// request with the file of the same name in the page's folder.
const kOrigin = 'http://localhost';

// Cross-origin isolation, which a page built with threads needs for its
// shared memory, and which a page without them doesn't notice.
const kHeaders = {
  'Cross-Origin-Opener-Policy': 'same-origin',
  'Cross-Origin-Embedder-Policy': 'require-corp',
};

// The exit code of a program that aborted, as on Linux (128 + SIGABRT).
const kAbortExitCode = 134;

const kScriptPath = '/pilot/script.txt';
const kOutputFolder = '/pilot/out';

// Keeps the output folder there however the program ends, as an error from the
// page's file system once the program has stopped aborts the page.
const kOutputKeep = '.keep';

const kPressKeysDelayMs = 1000;

function parseOptions() {
  const {values, positionals} = util.parseArgs({
    options: {
      'page': {type: 'string'},
      'out-dir': {type: 'string'},
      'timeout-secs': {type: 'string'},
      'timeout-file': {type: 'string'},
      'script': {type: 'string'},
      'press-keys': {type: 'string'},
    },
    allowPositionals: true,
  });
  for (const name of ['page', 'out-dir', 'timeout-secs', 'timeout-file']) {
    if (values[name] === undefined) {
      throw new Error(`--${name} is required.`);
    }
  }
  return {values: values, programArgs: positionals};
}

// Copies every file the program wrote to the page's output folder to
// `outDir`, one at a time.
async function copyOutput(page, outDir) {
  const names = await page.evaluate(([folder, keep]) => {
    return Module.FS.readdir(folder).filter((name) => {
      return name !== '.' && name !== '..' && name !== keep &&
          Module.FS.isFile(Module.FS.stat(folder + '/' + name).mode);
    });
  }, [kOutputFolder, kOutputKeep]);
  for (const name of names) {
    const bytes = await page.evaluate((file) => {
      return Module.FS.readFile(file);
    }, `${kOutputFolder}/${name}`);
    fs.writeFileSync(path.join(outDir, name), bytes);
  }
}

// Returns the file in `folder` that `url` names, or undefined if there is none.
function findFile(folder, url) {
  let file;
  try {
    file = path.join(folder, decodeURIComponent(new URL(url).pathname));
  } catch (error) {
    return undefined;
  }
  if (!file.startsWith(folder + path.sep) || !fs.existsSync(file) ||
      !fs.statSync(file).isFile()) {
    return undefined;
  }
  return file;
}

// Opens the page at `address`, presses the keys --press-keys gives, and waits
// for the program to stop, all within --timeout-secs. Returns what the shell
// set gbExit to, or undefined if the program was still running at the end.
async function waitForExit(page, address, options) {
  const deadline = Date.now() + Number(options['timeout-secs']) * 1000;
  const remainingMs = () => Math.max(1, deadline - Date.now());
  try {
    await page.goto(address, {timeout: remainingMs()});
    if (options['press-keys'] !== undefined) {
      await page.waitForTimeout(kPressKeysDelayMs);
      for (const key of options['press-keys'].split(/\s+/).filter(Boolean)) {
        await page.keyboard.press(key);
      }
    }

    // The exit code is wrapped, as 0 would read as not stopped.
    const stopped = await page.waitForFunction(() => {
      return globalThis.gbExit !== undefined && {result: globalThis.gbExit};
    }, null, {timeout: remainingMs()});
    return (await stopped.jsonValue()).result;
  } catch (error) {
    if (!(error instanceof errors.TimeoutError)) {
      throw error;
    }
    return undefined;
  }
}

async function run(options, programArgs, stdout, stderr) {
  const pagePath = path.resolve(options['page']);
  const folder = path.dirname(pagePath);
  const files = {};
  const args = [...programArgs];
  if (options['script'] !== undefined) {
    files[kScriptPath] = fs.readFileSync(options['script'], 'utf8');
    files[`${kOutputFolder}/${kOutputKeep}`] = '';
    args.push('--pilot', kScriptPath, '--pilot-output', kOutputFolder);
  }

  const browser = await chromium.launch({args: kBrowserArgs});
  try {
    const context = await browser.newContext();
    await context.route(`${kOrigin}/**`, (route) => {
      const file = findFile(folder, route.request().url());
      if (file === undefined) {
        return route.fulfill({status: 404});
      }
      return route.fulfill({path: file, headers: kHeaders});
    });
    const page = await context.newPage();
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

    const query = new URLSearchParams(args.map((arg) => ['arg', arg]));
    const name = encodeURIComponent(path.basename(pagePath));
    const result = await waitForExit(page, `${kOrigin}/${name}?${query}`,
                                     options);
    if (result === undefined) {
      fs.writeFileSync(options['timeout-file'], '');
    } else if (typeof result !== 'number') {
      fs.writeSync(stderr, `Aborted: ${result}\n`);
    }

    // Frames saved before a timeout or an abort are copied too, as they show
    // how far the run got.
    if (options['script'] !== undefined) {
      try {
        await copyOutput(page, options['out-dir']);
      } catch (error) {
        fs.writeSync(stderr, `Saved files could not be copied: ${error}\n`);
      }
    }
    if (result === undefined) {
      return 1;
    }
    return typeof result === 'number' ? result : kAbortExitCode;
  } finally {
    await browser.close();
  }
}

async function main() {
  const {values, programArgs} = parseOptions();
  const stdout = fs.openSync(path.join(values['out-dir'], 'stdout.txt'), 'w');
  const stderr = fs.openSync(path.join(values['out-dir'], 'stderr.txt'), 'w');
  try {
    return await run(values, programArgs, stdout, stderr);
  } finally {
    fs.closeSync(stdout);
    fs.closeSync(stderr);
  }
}

main().then((code) => process.exit(code), (error) => {
  console.error(String(error));
  process.exit(1);
});
