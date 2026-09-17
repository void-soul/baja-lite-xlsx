'use strict';

/**
 * Micro-benchmark for baja-lite-xlsx.
 *
 * Usage:
 *   npm run bench                              # uses examples/sample.xlsx
 *   npm run bench -- big.xlsx                  # your own (large) file
 *   npm run bench -- big.xlsx --iterations 3
 *
 * Point it at a real 100k-1M row workbook to see how the read paths differ:
 * buffered, column-projected, image-less and streamed.
 */

const path = require('path');
const fs = require('fs');

const { readTableAsJSON, readTableAsJSONAsync } = require('..');

function parseArgs(argv) {
  const args = { iterations: 5, batchSize: 20000, file: null };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--iterations') {
      args.iterations = Math.max(1, parseInt(argv[++i], 10) || 1);
    } else if (arg === '--batch') {
      args.batchSize = Math.max(1, parseInt(argv[++i], 10) || 20000);
    } else if (!arg.startsWith('--')) {
      args.file = arg;
    }
  }
  if (!args.file) {
    args.file = path.join(__dirname, '..', 'examples', 'sample.xlsx');
  }
  return args;
}

function mb(bytes) {
  return (bytes / 1048576).toFixed(1) + ' MB';
}

async function measure(label, run, iterations) {
  // One warm-up pass so JIT/caches do not skew the first sample.
  const firstRows = await run();
  const before = process.memoryUsage();
  const samples = [];
  let rows = firstRows;

  for (let i = 0; i < iterations; i++) {
    const start = process.hrtime.bigint();
    rows = await run();
    samples.push(Number(process.hrtime.bigint() - start) / 1e6);
  }
  const after = process.memoryUsage();

  samples.sort((a, b) => a - b);
  return {
    label,
    rows,
    best: samples[0],
    median: samples[Math.floor(samples.length / 2)],
    heapDelta: after.heapUsed - before.heapUsed,
    rssDelta: after.rss - before.rss
  };
}

function rowCount(result) {
  if (Array.isArray(result)) return result.length;
  if (result && typeof result.rowCount === 'number') return result.rowCount;
  if (result && Array.isArray(result.rows)) return result.rows.length;
  return 0;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));

  if (!fs.existsSync(args.file)) {
    console.error(`Fixture not found: ${args.file}`);
    process.exitCode = 1;
    return;
  }

  const size = fs.statSync(args.file).size;
  console.log('baja-lite-xlsx benchmark');
  console.log(`  file:       ${args.file} (${mb(size)})`);
  console.log(`  iterations: ${args.iterations} (+1 warm-up)\n`);

  const results = [];

  results.push(await measure('sync buffered', () => readTableAsJSON(args.file), args.iterations));
  results.push(await measure('async buffered', () => readTableAsJSONAsync(args.file), args.iterations));
  results.push(await measure('includeImages: false', () => readTableAsJSON(args.file, { includeImages: false }), args.iterations));

  let firstHeader = null;
  try {
    const probe = readTableAsJSON(args.file);
    firstHeader = Object.keys((probe && probe[0]) || {})[0] || null;
  } catch (err) {
    // A headerless sheet cannot be probed; the projection run is skipped.
  }
  if (firstHeader) {
    results.push(await measure(
      `columns: ['${firstHeader}']`,
      () => readTableAsJSON(args.file, { columns: [firstHeader] }),
      args.iterations
    ));
  }

  if (typeof readTableAsJSONAsync === 'function') {
    results.push(await measure(
      `streamed onBatch (${args.batchSize})`,
      () => readTableAsJSONAsync(args.file, { batchSize: args.batchSize, onBatch: () => {} }),
      args.iterations
    ));
  }

  const width = Math.max(...results.map((r) => r.label.length));
  console.log('  scenario'.padEnd(width + 4) + 'best      median    rows       rows/s');
  console.log('  ' + '-'.repeat(width + 46));
  for (const r of results) {
    const perSecond = r.best > 0 ? Math.round(r.rows / (r.best / 1000)) : 0;
    console.log(
      '  ' + r.label.padEnd(width + 2) +
      r.best.toFixed(1).padStart(7) + 'ms' +
      r.median.toFixed(1).padStart(9) + 'ms' +
      String(r.rows).padStart(9) +
      String(perSecond).padStart(11)
    );
  }

  console.log('\n  memory (retained after the measured runs)');
  for (const r of results) {
    console.log(
      '  ' + r.label.padEnd(width + 2) + 'heap ' + mb(r.heapDelta).padStart(9) +
      '   rss ' + mb(r.rssDelta).padStart(9)
    );
  }
  console.log('\nNote: the smallest number is the most meaningful one; the streaming row');
  console.log('stays flat in memory no matter how large the sheet is, which is what the');
  console.log('heap/rss columns above show for a large file.');
}

main().catch((err) => {
  console.error(err);
  process.exitCode = 1;
});
